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
static uint16_t kPre;         // amostra em que a PRBS comeca (tPre/Ts)
static uint16_t nBit;         // amostras por bit (Tb/Ts)
static uint16_t nAcomoda;     // amostras de acomodacao (tAcomoda/Ts)
static uint16_t semente;
static uint16_t acomoda;      // amostras de acomodacao que ainda faltam

// Estado da PRBS
static uint16_t lfsr;         // registrador de deslocamento (8 bits)
static float    b;            // bit atual: +1 ou -1

static uint16_t Amostras(float t, float Ts)
{
    return (uint16_t)(t / Ts + 0.5f);
}

// Proximo bit da PRBS: LFSR de 8 bits, x^8 + x^6 + x^5 + x^4 + 1
static float ProximoBit(void)
{
    uint16_t realimenta = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 1U;
    lfsr = (lfsr >> 1) | (realimenta << 7);
    return ((lfsr & 1U) != 0U) ? 1.0f : -1.0f;
}

static void ReiniciaEnsaio(void)
{
    k = 0U;
    ensaioConcluido = 0U;
    acomoda = nAcomoda;
    lfsr = semente;
    b = 1.0f;
}

void ConfiguraEnsaioPRBS(const ParametrosPRBS *p)
{
    u0       = p->u0;
    A        = p->A;
    kPre     = Amostras(p->tPre, p->Ts);
    nBit     = Amostras(p->Tb, p->Ts);
    nAcomoda = Amostras(p->tAcomoda, p->Ts);
    semente  = p->semente & 0xFFU;
    if (nBit == 0U)
    {
        nBit = 1U;
    }
    if (semente == 0U)                   // LFSR em zero ficaria parado
    {
        semente = 1U;
    }

    ReiniciaEnsaio();
    DAC2u(u0);                           // nivel inicial
}

float PRBS(uint16_t k)
{
    if ((acomoda > 0U) || (k < kPre) || (k >= N_AMOSTRAS))   // fora da PRBS: u0
    {
        return u0;
    }
    if (((k - kPre) % nBit) == 0U)       // inicio de um bit: sorteia o proximo
    {
        b = ProximoBit();
    }
    return u0 + A * b;
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
