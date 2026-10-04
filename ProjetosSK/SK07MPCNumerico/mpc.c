#include <math.h>
#include "mpc.h"
#include "placa.h"

volatile Desempenho desempenho;

// Parametros
static float    a1, a2, b1, b2, lambda, passo, uMin, uMax;
static uint16_t Np, Nu, nIter;
static uint32_t ciclosTs;              // ciclos de CPU em um periodo de amostragem

static float s[NP_MAX];                // resposta ao degrau do modelo (sensibilidade)
static float yp[NP_MAX];               // saida prevista y^(k+1) .. y^(k+Np)
static float dU[NU_MAX];               // plano: du(k) .. du(k+Nu-1)
static float grad[NU_MAX];

// Medidas e entradas passadas
static float y0m, y1m, y2m;            // y(k), y(k-1), y(k-2)
static float u1, u2;                   // u(k-1), u(k-2) aplicados
static uint16_t primeiraAmostra;

// s_j: saida do modelo para um degrau unitario em u aplicado em k
static void RespostaDegrau(void)
{
    float    yA = 0.0f, yB = 0.0f, uAnt = 0.0f, yn;
    uint16_t j;

    for (j = 0U; j < Np; j++)
    {
        yn = a1 * yA + a2 * yB + b1 + b2 * uAnt;
        uAnt = 1.0f;
        yB = yA;
        yA = yn;
        s[j] = yn;
    }
}

// Passo do gradiente: 1/L, com L >= maior curvatura de J (soma de linhas da
// matriz de curvatura, calculada uma vez aqui a partir de s)
static float PassoGradiente(void)
{
    float    L = 0.0f, linha, hil;
    uint16_t i, l, j;

    for (i = 0U; i < Nu; i++)
    {
        linha = lambda;
        for (l = 0U; l < Nu; l++)
        {
            hil = 0.0f;
            for (j = (i > l) ? i : l; j < Np; j++)
            {
                hil += s[j - i] * s[j - l];
            }
            linha += fabsf(hil);
        }
        if (linha > L) L = linha;
    }
    return 1.0f / L;
}

void ConfiguraMPC(const ParametrosMPC *p, float Ts)
{
    uint32_t t0 = Cronometro();
    uint16_t i;

    a1 = p->modelo.a1;  a2 = p->modelo.a2;
    b1 = p->modelo.b1;  b2 = p->modelo.b2;
    lambda = p->lambda;
    nIter = p->nIter;
    uMin = p->uMin;     uMax = p->uMax;
    Np = (p->Np > NP_MAX) ? NP_MAX : p->Np;
    Nu = (p->Nu > NU_MAX) ? NU_MAX : p->Nu;
    if (Nu > Np)
    {
        Nu = Np;
    }
    ciclosTs = (uint32_t)(Ts * (float)DEVICE_SYSCLK_FREQ);

    RespostaDegrau();
    passo = PassoGradiente();

    primeiraAmostra = 1U;                 // a primeira medida preenche o historico
    u1 = u2 = p->uInicial;
    for (i = 0U; i < Nu; i++)
    {
        dU[i] = 0.0f;
    }

    desempenho.inicializacao = t0 - Cronometro();
    desempenho.calculoMax = desempenho.lacoMax = 0UL;
    desempenho.iteracoes = nIter;
    desempenho.estouros = 0U;
    desempenho.ocupacaoMax = 0.0f;
}

// y^(k+1..k+Np) para o plano dU: simula o modelo nas variacoes, a partir das medidas
static void Preve(void)
{
    float    dyA = y0m - y1m, dyB = y1m - y2m, duAnt = u1 - u2, yy = y0m, du, dy;
    uint16_t j;

    for (j = 0U; j < Np; j++)
    {
        du = (j < Nu) ? dU[j] : 0.0f;
        dy = a1 * dyA + a2 * dyB + b1 * du + b2 * duAnt;
        duAnt = du;
        dyB = dyA;
        dyA = dy;
        yy += dy;
        yp[j] = yy;
    }
}

// Uma iteracao do gradiente: preve, calcula grad e da um passo
static void IteracaoGradiente(float r)
{
    uint16_t i, j;
    float    g;

    Preve();
    for (i = 0U; i < Nu; i++)                           // grad_i = -sum s_(j-i) e_j + lambda du_i
    {
        g = lambda * dU[i];
        for (j = i; j < Np; j++)
        {
            g -= s[j - i] * (r - yp[j]);
        }
        grad[i] = g;
    }
    for (i = 0U; i < Nu; i++)
    {
        dU[i] -= passo * grad[i];
    }
}

float MPC(float r, float y)
{
    uint32_t t0 = Cronometro();
    float    u;
    uint16_t i, it;

    if (primeiraAmostra)                                // historico = primeira medida
    {
        y1m = y2m = y;
        primeiraAmostra = 0U;
    }
    else
    {
        y2m = y1m;
        y1m = y0m;
    }
    y0m = y;

    for (i = 0U; i + 1U < Nu; i++)                      // partida a quente: plano anterior
    {                                                   // deslocado de uma posicao
        dU[i] = dU[i + 1U];
    }
    dU[Nu - 1U] = 0.0f;

    for (it = 0U; it < nIter; it++)
    {
        IteracaoGradiente(r);
    }

    u = u1 + dU[0];                                     // so o primeiro movimento
    if (u < uMin) u = uMin;                             // saturacao do atuador
    if (u > uMax) u = uMax;
    dU[0] = u - u1;                                     // o plano parte do que foi aplicado
    u2 = u1;
    u1 = u;

    desempenho.calculo = t0 - Cronometro();
    if (desempenho.calculo > desempenho.calculoMax) desempenho.calculoMax = desempenho.calculo;
    return u;
}

void FimLaco(uint32_t t0)
{
    desempenho.laco = t0 - Cronometro();
    if (desempenho.laco > desempenho.lacoMax)
    {
        desempenho.lacoMax = desempenho.laco;
        desempenho.ocupacaoMax = 100.0f * (float)desempenho.lacoMax / (float)ciclosTs;
    }
    if (EstourouTs())
    {
        desempenho.estouros++;
    }
}
