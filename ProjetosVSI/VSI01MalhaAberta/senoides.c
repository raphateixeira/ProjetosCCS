#include <math.h>
#include "senoides.h"

#define DOIS_PI       6.283185307f
#define DOIS_PI_3     2.094395102f     // 120 graus

// vaV em RAMGS0, vbV em RAMGS1 e vcV em RAMGS2 (secoes "ramgs0..2" do linker)
#pragma DATA_SECTION(vaV, "ramgs0")
#pragma DATA_SECTION(vbV, "ramgs1")
#pragma DATA_SECTION(vcV, "ramgs2")
float vaV[N_AMOSTRAS];
float vbV[N_AMOSTRAS];
float vcV[N_AMOSTRAS];

volatile uint16_t n = 0U;
volatile uint16_t gravacaoConcluida = 0U;

static float    Vp;           // amplitude final [V]
static float    dTheta;       // passo do angulo por amostra: 2 pi f Ts [rad]
static float    theta;        // angulo atual [rad]
static uint32_t nRampa;       // amostras da rampa (tRampa/Ts)
static uint32_t kRampa;       // amostras ja passadas na rampa

void ConfiguraSenoides(const ParametrosInversor *p)
{
    float Ts = 1.0f / p->fPWM;

    Vp     = (p->Vp > p->Vcc / 2.0f) ? p->Vcc / 2.0f : p->Vp;   // sem sobremodulacao
    dTheta = DOIS_PI * p->f * Ts;
    nRampa = (uint32_t)(p->tRampa / Ts + 0.5f);

    theta  = 0.0f;
    kRampa = 0U;
    n = 0U;
    gravacaoConcluida = 0U;
}

void Senoides(float *va, float *vb, float *vc)
{
    float V = Vp;                           // amplitude no instante atual
    if (kRampa < nRampa)
    {
        V = Vp * (float)kRampa / (float)nRampa;
        kRampa++;
    }

    *va = V * sinf(theta);
    *vb = V * sinf(theta - DOIS_PI_3);
    *vc = V * sinf(theta + DOIS_PI_3);

    theta += dTheta;
    if (theta >= DOIS_PI)
    {
        theta -= DOIS_PI;
    }
}

void Grava(float va, float vb, float vc)
{
    if ((kRampa < nRampa) || (n >= N_AMOSTRAS))   // rampa ou gravacao completa
    {
        return;
    }

    vaV[n] = va;
    vbV[n] = vb;
    vcV[n] = vc;
    n++;
    if (n >= N_AMOSTRAS)
    {
        gravacaoConcluida = 1U;
    }
}
