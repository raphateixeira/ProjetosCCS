#include <math.h>
#include "controle.h"

#define DOIS_PI     6.283185307f
#define RAIZ3       1.732050808f
#define INV_RAIZ3   0.577350269f
#define K_MULTI     100U        // harmonicas do multisseno: f0 .. K_MULTI f0 (20 Hz a 2 kHz com Ts = 50 us)

Referencias ref = {
    .nivel = NIVEL_PWM50, .f = 60.0f, .V = 0.0f,
    .idRef = 0.0f, .iqRef = 0.0f, .vd0 = 0.0f, .vq0 = 0.0f,
    .tipoExc = EXC_PRBS, .ampExc = 0.0f, .eixoExc = EIXO_D,
    .nbPRBS = 4U, .nDegrau = 200U,
};
Estado est;

// Tabela de um periodo do multisseno, normalizada para pico 1 (RAMGS4)
#pragma DATA_SECTION(multisseno, "ramgs4")
static float multisseno[N_MULTI];

typedef struct
{
    float kp, kiTs;      // ganhos (kiTs = ki Ts)
    float xi;            // integrador [V]
} PI;

static PI piD, piQ;
static float Ts, L, vMax, VdcNom;
static uint16_t prbsD, prbsQ;        // registradores de deslocamento (9 bits)

// ---------------------------------------------------------------------------
void Controle_Configura(const ParametrosControle *p)
{
    float wc = DOIS_PI * p->fc;
    uint16_t n, k;
    float pico = 0.0f;

    Ts = p->Ts;
    L = p->L;
    vMax = p->vMax;
    VdcNom = p->VdcNom;

    // Cancelamento do polo da carga: C(s) = kp + ki/s = wc (L s + R)/s
    // -> malha fechada de 1a ordem com banda wc (sem contar o atraso do PWM)
    piD.kp = wc * p->L;
    piD.kiTs = wc * p->R * p->Ts;
    piQ = piD;

    // Multisseno com fases de Schroeder: soma de cos(2 pi k n/N + phi_k), k = 1..K
    for (n = 0U; n < N_MULTI; n++)
    {
        float s = 0.0f;
        for (k = 1U; k <= K_MULTI; k++)
        {
            float phi = -3.141592654f * (float)k * (float)(k - 1U) / (float)K_MULTI;
            s += cosf(DOIS_PI * (float)k * (float)n / (float)N_MULTI + phi);
        }
        multisseno[n] = s;
        if (fabsf(s) > pico) pico = fabsf(s);
    }
    for (n = 0U; n < N_MULTI; n++)
    {
        multisseno[n] /= pico;
    }

    est.th = 0.0f;
    est.Vdc = VdcNom;
    Controle_Reinicia();
    Controle_ReiniciaExcitacao();
}

void Controle_Reinicia(void)
{
    piD.xi = 0.0f;
    piQ.xi = 0.0f;
}

void Controle_ReiniciaExcitacao(void)
{
    est.k = 0UL;
    prbsD = 0x1FFU;                  // estados iniciais diferentes: sequencias deslocadas
    prbsQ = 0x0AAU;
}

// ---------------------------------------------------------------------------
// PI com integrador limitado a +-uLim (anti-windup simples)
static float PassoPI(PI *c, float e, float uLim)
{
    float u;

    c->xi += c->kiTs * e;
    if (c->xi > uLim)  c->xi = uLim;
    if (c->xi < -uLim) c->xi = -uLim;
    u = c->kp * e + c->xi;
    if (u > uLim)  u = uLim;
    if (u < -uLim) u = -uLim;
    return u;
}

// PRBS de 9 bits, polinomio x^9 + x^5 + 1 (periodo 511 bits): devolve +-1
static float PassoPRBS(uint16_t *r)
{
    uint16_t bit = ((*r >> 8) ^ (*r >> 4)) & 1U;
    *r = ((*r << 1) | bit) & 0x1FFU;
    return (*r & 1U) ? 1.0f : -1.0f;
}

// Excitacao do nivel 4 na amostra est.k (sem a amplitude)
static void Excitacao(void)
{
    uint32_t k = est.k;

    switch (ref.tipoExc)
    {
    case EXC_PRBS:
        if ((k % ref.nbPRBS) == 0UL)          // novo bit a cada nbPRBS amostras
        {
            est.ed = PassoPRBS(&prbsD);
            est.eq = PassoPRBS(&prbsQ);
        }
        break;
    case EXC_DEGRAU:
        est.ed = ((k / ref.nDegrau) & 1UL) ? 1.0f : 0.0f;
        est.eq = est.ed;
        break;
    default:                                  // EXC_MULTISSENO
        est.ed = multisseno[k % N_MULTI];
        est.eq = multisseno[(k + N_MULTI / 2U) % N_MULTI];
        break;
    }
    if (ref.eixoExc == EIXO_D) est.eq = 0.0f;
    if (ref.eixoExc == EIXO_Q) est.ed = 0.0f;
    est.k++;
}

// ---------------------------------------------------------------------------
void Controle_Passo(const Medidas *m, uint16_t habilitado, float d[3])
{
    float dth = DOIS_PI * ref.f * Ts;         // passo do angulo por amostra
    float w = DOIS_PI * ref.f;
    float c = cosf(est.th), s = sinf(est.th);
    float Vdc, vLim, vd, vq, mod, valfa, vbeta, v0;

    // Barramento filtrado (~160 Hz com Ts = 50 us)
    est.Vdc += 0.05f * (m->Vdc - est.Vdc);
    Vdc = (est.Vdc > 5.0f) ? est.Vdc : VdcNom;

    // Clarke e Park
    est.ialfa = (2.0f * m->ia - m->ib - m->ic) * (1.0f / 3.0f);
    est.ibeta = (m->ib - m->ic) * INV_RAIZ3;
    est.id =  est.ialfa * c + est.ibeta * s;
    est.iq = -est.ialfa * s + est.ibeta * c;

    // Limite da tensao de fase: modulacao e vMax
#if MODULACAO == MOD_SVPWM
    vLim = Vdc * INV_RAIZ3;
#else
    vLim = 0.5f * Vdc;
#endif
    if (vLim > vMax) vLim = vMax;

    vd = 0.0f;
    vq = 0.0f;
    switch (ref.nivel)
    {
    case NIVEL_MALHA_ABERTA:
        vd = ref.V;
        break;
    case NIVEL_MALHA_CORRENTE:
        if (habilitado)
        {
            vd = PassoPI(&piD, ref.idRef - est.id, vLim) - w * L * est.iq;
            vq = PassoPI(&piQ, ref.iqRef - est.iq, vLim) + w * L * est.id;
        }
        else
        {
            Controle_Reinicia();
        }
        break;
    case NIVEL_IDENTIFICACAO:
        Excitacao();
        vd = ref.vd0 + ref.ampExc * est.ed;
        vq = ref.vq0 + ref.ampExc * est.eq;
        break;
    default:                                  // NIVEL_PWM50
        break;
    }

    // Saturacao do vetor de tensao (circulo de raio vLim)
    mod = sqrtf(vd * vd + vq * vq);
    if (mod > vLim)
    {
        vd *= vLim / mod;
        vq *= vLim / mod;
    }
    est.vd = vd;
    est.vq = vq;

    // Park e Clarke inversas, com o angulo adiantado de 1,5 Ts: a razao
    // ciclica calculada agora so vale no proximo periodo (CMPA com sombra)
    {
        float th = est.th + 1.5f * dth;
        float cc = cosf(th), ss = sinf(th);
        valfa = vd * cc - vq * ss;
        vbeta = vd * ss + vq * cc;
    }
    est.va = valfa;
    est.vb = -0.5f * valfa + 0.5f * RAIZ3 * vbeta;
    est.vc = -0.5f * valfa - 0.5f * RAIZ3 * vbeta;

    // Modulacao: d = 1/2 + (v + v0)/Vdc
#if MODULACAO == MOD_SVPWM
    {
        float vmax = est.va, vmin = est.va;
        if (est.vb > vmax) vmax = est.vb;
        if (est.vc > vmax) vmax = est.vc;
        if (est.vb < vmin) vmin = est.vb;
        if (est.vc < vmin) vmin = est.vc;
        v0 = -0.5f * (vmax + vmin);
    }
#else
    v0 = 0.0f;
#endif
    if (ref.nivel == NIVEL_PWM50)
    {
        d[0] = d[1] = d[2] = 0.5f;
    }
    else
    {
        d[0] = 0.5f + (est.va + v0) / Vdc;
        d[1] = 0.5f + (est.vb + v0) / Vdc;
        d[2] = 0.5f + (est.vc + v0) / Vdc;
    }

    // Angulo (frequencia fixa), mantido em 0 .. 2 pi
    est.th += dth;
    if (est.th >= DOIS_PI) est.th -= DOIS_PI;
    if (est.th < 0.0f)     est.th += DOIS_PI;
}
