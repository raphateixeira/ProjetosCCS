#include "ensaio.h"
#include "placa.h"

// uV em RAMGS0 e yV em RAMGS1 (secoes "ramgs0" e "ramgs1" do linker)
#pragma DATA_SECTION(uV, "ramgs0")
#pragma DATA_SECTION(yV, "ramgs1")
float uV[N_AMOSTRAS];
float yV[N_AMOSTRAS];

volatile uint16_t k = 0U;
volatile uint16_t ensaioConcluido = 0U;
volatile uint16_t reiniciar = 0U;

// Parametros do ensaio convertidos para amostras
static float    u0, A;
static uint16_t k1;           // amostra da primeira subida (tAtraso/Ts)
static uint16_t nMeio;        // amostras em cada nivel (Tq/2/Ts)
static uint16_t nAcomoda;     // amostras de acomodacao (tAcomoda/Ts)
static uint16_t acomoda;      // amostras de acomodacao que ainda faltam

static uint16_t Amostras(float t, float Ts)
{
    return (uint16_t)(t / Ts + 0.5f);
}

// q(k) = -1 (nivel baixo) ou +1 (nivel alto)
static float q(uint16_t n)
{
    if (n < k1)
    {
        return -1.0f;
    }
    return ((((n - k1) / nMeio) % 2U) == 0U) ? 1.0f : -1.0f;
}

static void ReiniciaEnsaio(void)
{
    k = 0U;
    ensaioConcluido = 0U;
    acomoda = nAcomoda;
}

void ConfiguraEnsaioDegrau(const ParametrosDegrau *p)
{
    u0       = p->u0;
    A        = p->A;
    k1       = Amostras(p->tAtraso, p->Ts);
    nMeio    = Amostras(p->Tq / 2.0f, p->Ts);
    nAcomoda = Amostras(p->tAcomoda, p->Ts);
    if (nMeio == 0U)
    {
        nMeio = 1U;
    }

    ReiniciaEnsaio();
    DAC2u(u0 - A);                       // nivel inicial (baixo)
}

float Degrau(uint16_t k)
{
    if ((acomoda > 0U) || (k >= N_AMOSTRAS))   // acomodacao ou fim: nivel baixo
    {
        return u0 - A;
    }
    return u0 + A * q(k);
}

void Grava(float u, float y)
{
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
