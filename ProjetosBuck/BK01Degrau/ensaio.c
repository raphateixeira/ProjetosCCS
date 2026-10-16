#include "ensaio.h"

// uV em RAMGS0 e yV em RAMGS1 (secoes "ramgs0" e "ramgs1" do linker)
#pragma DATA_SECTION(uV, "ramgs0")
#pragma DATA_SECTION(yV, "ramgs1")
float uV[N_AMOSTRAS];
float yV[N_AMOSTRAS];

volatile uint16_t k = 0U;
volatile uint16_t ensaioConcluido = 0U;
volatile uint16_t reiniciar = 0U;

// Parametros do ensaio convertidos para amostras
static float    vBaixo, vAlto;
static uint16_t k1;           // amostra da primeira subida (tAtraso/Ts)
static uint16_t nMeio;        // amostras em cada nivel (Tq/2/Ts)
static uint32_t nRampa;       // amostras da rampa (tRampa/Ts)
static uint32_t nAcomoda;     // amostras de acomodacao (tAcomoda/Ts)
static uint32_t rampa;        // amostras de rampa que ainda faltam
static uint32_t acomoda;      // amostras de acomodacao que ainda faltam

static uint32_t Amostras(float t, float Ts)
{
    return (uint32_t)(t / Ts + 0.5f);
}

// Nivel da onda quadrada na amostra n: vBaixo antes de k1, depois vAlto / vBaixo
static float Nivel(uint16_t n)
{
    if (n < k1)
    {
        return vBaixo;
    }
    return ((((n - k1) / nMeio) % 2U) == 0U) ? vAlto : vBaixo;
}

static void ReiniciaEnsaio(void)
{
    k = 0U;
    ensaioConcluido = 0U;
    acomoda = nAcomoda;
}

void ConfiguraEnsaioDegrau(const ParametrosDegrau *p)
{
    vBaixo   = p->vBaixo;
    vAlto    = p->vAlto;
    k1       = (uint16_t)Amostras(p->tAtraso, p->Ts);
    nMeio    = (uint16_t)Amostras(p->Tq / 2.0f, p->Ts);
    nRampa   = Amostras(p->tRampa, p->Ts);
    nAcomoda = Amostras(p->tAcomoda, p->Ts);
    if (nMeio == 0U)
    {
        nMeio = 1U;
    }

    rampa = nRampa;
    ReiniciaEnsaio();
}

float Degrau(uint16_t k)
{
    if (rampa > 0U)                            // rampa: 0 -> vBaixo
    {
        return vBaixo * (float)(nRampa - rampa) / (float)nRampa;
    }
    if ((acomoda > 0U) || (k >= N_AMOSTRAS))   // acomodacao ou fim: nivel baixo
    {
        return vBaixo;
    }
    return Nivel(k);
}

void Grava(float u, float y)
{
    if (rampa > 0U)                      // rampa: nao grava
    {
        rampa--;
        return;
    }

    if (reiniciar != 0U)                 // pedido pelo WATCH: recomeca o ensaio
    {
        reiniciar = 0U;
        ReiniciaEnsaio();
    }

    if (acomoda > 0U)                    // acomodacao: nao grava
    {
        acomoda--;
        return;
    }

    if (k < N_AMOSTRAS)
    {
        uV[k] = u;
        yV[k] = y;
        k++;
        if (k >= N_AMOSTRAS)
        {
            ensaioConcluido = 1U;
        }
    }
}
