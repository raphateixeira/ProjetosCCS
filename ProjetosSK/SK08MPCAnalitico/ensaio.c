#include "ensaio.h"
#include "placa.h"

// RAMGS0: uV e yV (3200 palavras); RAMGS1: rV, tV e nV (4000 palavras)
#pragma DATA_SECTION(uV, "ramgs0")
#pragma DATA_SECTION(yV, "ramgs0")
#pragma DATA_SECTION(rV, "ramgs1")
#pragma DATA_SECTION(tV, "ramgs1")
#pragma DATA_SECTION(nV, "ramgs1")
float    rV[N_AMOSTRAS];
float    uV[N_AMOSTRAS];
float    yV[N_AMOSTRAS];
float    tV[N_AMOSTRAS];
uint16_t nV[N_AMOSTRAS];

volatile uint16_t k = 0U;
volatile uint16_t ensaioConcluido = 0U;

// Parametros do ensaio convertidos para amostras
static float    rBaixo, rAlto;
static uint16_t k1;           // amostra da primeira subida (tInicio/Ts)
static uint16_t nMeio;        // amostras em cada nivel (Tq/2/Ts)
static uint32_t acomoda;      // amostras de acomodacao que ainda faltam

static uint32_t Amostras(float t, float Ts)
{
    return (uint32_t)(t / Ts + 0.5f);
}

void ConfiguraEnsaio(const ParametrosEnsaio *p)
{
    rBaixo  = p->rBaixo;
    rAlto   = p->rAlto;
    k1      = (uint16_t)Amostras(p->tInicio, p->Ts);
    nMeio   = (uint16_t)Amostras(p->Tq / 2.0f, p->Ts);
    acomoda = Amostras(p->tAcomoda, p->Ts);
    if (nMeio == 0U)
    {
        nMeio = 1U;
    }
    k = 0U;
    ensaioConcluido = 0U;
}

float Referencia(uint16_t k)
{
    if ((acomoda > 0U) || (k >= N_AMOSTRAS) || (k < k1))
    {
        return rBaixo;
    }
    return ((((k - k1) / nMeio) % 2U) == 0U) ? rAlto : rBaixo;
}

void Grava(float r, float u, float y, uint32_t ciclos, uint16_t iteracoes)
{
    if (acomoda > 0U)                    // acomodacao: nao grava
    {
        acomoda--;
        return;
    }

    if (k < N_AMOSTRAS)
    {
        rV[k] = r;
        uV[k] = u;
        yV[k] = y;
        tV[k] = (float)ciclos / (float)CICLOS_POR_US;
        nV[k] = iteracoes;
        k++;
        if (k >= N_AMOSTRAS)
        {
            ensaioConcluido = 1U;
        }
    }
}
