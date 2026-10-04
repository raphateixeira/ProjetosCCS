#include "mpc.h"
#include "placa.h"

// Matrizes em RAMGS2..RAMGS5 (secao "mpcdados"): a .bss (RAMLS5) tem so 2048 palavras
#pragma DATA_SECTION(F, "mpcdados")
#pragma DATA_SECTION(Phi, "mpcdados")
#pragma DATA_SECTION(degrau, "mpcdados")
#pragma DATA_SECTION(H, "mpcdados")
#pragma DATA_SECTION(W, "mpcdados")
#pragma DATA_SECTION(Hinv, "mpcdados")
static float F[NP_MAX][3];          // linhas C A^j: resposta livre F x^
static float Phi[NP_MAX][NU_MAX];    // resposta ao degrau (Toeplitz inferior)
static float degrau[NP_MAX];         // g_j = C A^(j-1) B
static float H[NU_MAX][NU_MAX];      // Phi'Phi + lambda I
static float W[NU_MAX][NU_MAX];      // copia de trabalho de H
static float Hinv[NU_MAX][NU_MAX];   // H^-1


static float A[3][3], B[3];          // modelo aumentado (C = [0 0 1])
static float Lo[3];                  // ganho do observador
static float x[3];                   // x^(k|k)
static float xp[3];                  // x^(k|k-1)
static float uAnt;                   // u(k-1) aplicado [V]

volatile Desempenho desempenho;

float Ky;                             // du(k) = Ky r~ - Kmpc x^
float Kmpc[3];

// Parametros
static float    a1, a2, b1, b2, uOp, yOp, uMin, uMax;
static uint16_t Np, Nu;
static uint32_t ciclosTs;            // ciclos de CPU em um periodo de amostragem

// ---------------------------------------------------------------------------
// Modelo aumentado (Wang, 2009) a partir do ARX em forma canonica observavel:
//     Am = [a1 1; a2 0], Bm = [b1; b2], Cm = [1 0]
//     A  = [Am 0; Cm Am 1], B = [Bm; Cm Bm], C = [0 0 1],  x = [dxm; y~]
// ---------------------------------------------------------------------------
static void MontaModelo(const ModeloARX *m)
{
    a1 = m->a1;  a2 = m->a2;  b1 = m->b1;  b2 = m->b2;
    uOp = m->uOp; yOp = m->yOp;

    A[0][0] = a1;   A[0][1] = 1.0f;  A[0][2] = 0.0f;
    A[1][0] = a2;   A[1][1] = 0.0f;  A[1][2] = 0.0f;
    A[2][0] = a1;   A[2][1] = 1.0f;  A[2][2] = 1.0f;     // Cm Am = [a1 1]
    B[0] = b1;      B[1] = b2;       B[2] = b1;          // Cm Bm = b1
}

// v <- v A (vetor linha 1x3 vezes A)
static void VezesA(float *v)
{
    float w0 = v[0] * A[0][0] + v[1] * A[1][0] + v[2] * A[2][0];
    float w1 = v[0] * A[0][1] + v[1] * A[1][1] + v[2] * A[2][1];
    float w2 = v[0] * A[0][2] + v[1] * A[1][2] + v[2] * A[2][2];
    v[0] = w0;  v[1] = w1;  v[2] = w2;
}

// F[j] = C A^(j+1) (Np x 3) e Phi[j][i] = C A^(j-i) B, j >= i (Np x Nu)
static void MontaFPhi(void)
{
    float    v[3] = {0.0f, 0.0f, 1.0f};               // C
    uint16_t j, i;

    for (j = 0U; j < Np; j++)                         // degrau[j] = g_(j+1) = C A^j B
    {
        degrau[j] = v[0] * B[0] + v[1] * B[1] + v[2] * B[2];
        VezesA(v);
        F[j][0] = v[0];  F[j][1] = v[1];  F[j][2] = v[2];
    }
    for (j = 0U; j < Np; j++)
    {
        for (i = 0U; i < Nu; i++)
        {
            Phi[j][i] = (i <= j) ? degrau[j - i] : 0.0f;
        }
    }
}

// Ganho do observador corrente, polos de (I - L C) A nos tres em alfa (Ackermann):
//     L = phi(A) O^-1 e3,  O = [C A; C A^2; C A^3],  phi(A) = (A - alfa I)^3
static void GanhoObservador(float alfa)
{
    float    O[3][3], M1[3][3], M2[3][3], M3[3][3], v[3] = {0.0f, 0.0f, 1.0f};
    float    c0, c1, c2, det, w[3], s;
    uint16_t i, j, l;

    for (i = 0U; i < 3U; i++)                         // linhas C A, C A^2, C A^3
    {
        VezesA(v);
        O[i][0] = v[0];  O[i][1] = v[1];  O[i][2] = v[2];
    }
    // w = O^-1 e3 = 3a coluna de O^-1 = cofatores da 3a linha / det
    c0 =   O[0][1] * O[1][2] - O[0][2] * O[1][1];
    c1 = -(O[0][0] * O[1][2] - O[0][2] * O[1][0]);
    c2 =   O[0][0] * O[1][1] - O[0][1] * O[1][0];
    det = O[2][0] * c0 + O[2][1] * c1 + O[2][2] * c2;
    w[0] = c0 / det;  w[1] = c1 / det;  w[2] = c2 / det;

    for (i = 0U; i < 3U; i++)                         // M1 = A - alfa I
    {
        for (j = 0U; j < 3U; j++)
        {
            M1[i][j] = A[i][j] - ((i == j) ? alfa : 0.0f);
        }
    }
    for (i = 0U; i < 3U; i++)                         // M2 = M1 M1
    {
        for (j = 0U; j < 3U; j++)
        {
            s = 0.0f;
            for (l = 0U; l < 3U; l++) s += M1[i][l] * M1[l][j];
            M2[i][j] = s;
        }
    }
    for (i = 0U; i < 3U; i++)                         // M3 = M2 M1
    {
        for (j = 0U; j < 3U; j++)
        {
            s = 0.0f;
            for (l = 0U; l < 3U; l++) s += M2[i][l] * M1[l][j];
            M3[i][j] = s;
        }
    }
    for (i = 0U; i < 3U; i++)                         // L = M3 w
    {
        Lo[i] = M3[i][0] * w[0] + M3[i][1] * w[1] + M3[i][2] * w[2];
    }
}

// Observador corrente: x^(k|k) = x^(k|k-1) + L [y~(k) - C x^(k|k-1)]
static void Observador(float y)
{
    float e = (y - yOp) - xp[2];

    x[0] = xp[0] + Lo[0] * e;
    x[1] = xp[1] + Lo[1] * e;
    x[2] = xp[2] + Lo[2] * e;
}

// Depois de aplicar u(k) (saturado): x^(k+1|k) = A x^(k|k) + B du(k)
static void AtualizaEstado(float u)
{
    float    du = u - uAnt;
    uint16_t i;

    for (i = 0U; i < 3U; i++)
    {
        xp[i] = A[i][0] * x[0] + A[i][1] * x[1] + A[i][2] * x[2] + B[i] * du;
    }
    uAnt = u;
}

// H = Phi'Phi + lambda I
static void MontaH(float lambda)
{
    uint16_t i, l, j;
    float    s;

    for (i = 0U; i < Nu; i++)
    {
        for (l = i; l < Nu; l++)
        {
            s = 0.0f;
            for (j = 0U; j < Np; j++)
            {
                s += Phi[j][i] * Phi[j][l];
            }
            H[i][l] = s;
            H[l][i] = s;
        }
        H[i][i] += lambda;
    }
}

// Hinv = H^-1 por Gauss-Jordan (H simetrica definida positiva: sem pivotamento).
// Usa W como copia de trabalho.
static void InverteH(void)
{
    uint16_t c, l, i;
    float    piv, fator;

    for (l = 0U; l < Nu; l++)
    {
        for (i = 0U; i < Nu; i++)
        {
            W[l][i]    = H[l][i];
            Hinv[l][i] = (l == i) ? 1.0f : 0.0f;
        }
    }
    for (c = 0U; c < Nu; c++)
    {
        piv = 1.0f / W[c][c];
        for (i = 0U; i < Nu; i++)
        {
            W[c][i]    *= piv;
            Hinv[c][i] *= piv;
        }
        for (l = 0U; l < Nu; l++)
        {
            if (l != c)
            {
                fator = W[l][c];
                for (i = 0U; i < Nu; i++)
                {
                    W[l][i]    -= fator * W[c][i];
                    Hinv[l][i] -= fator * Hinv[c][i];
                }
            }
        }
    }
}

void ConfiguraMPC(const ParametrosMPC *p, float Ts)
{
    uint32_t t0 = Cronometro();
    uint16_t i, j;
    float    s;

    MontaModelo(&p->modelo);
    uMin = p->uMin;  uMax = p->uMax;
    Np = (p->Np > NP_MAX) ? NP_MAX : p->Np;
    Nu = (p->Nu > NU_MAX) ? NU_MAX : p->Nu;
    if (Nu > Np)
    {
        Nu = Np;
    }
    ciclosTs = (uint32_t)(Ts * (float)DEVICE_SYSCLK_FREQ);

    MontaFPhi();
    GanhoObservador(p->alfaObs);
    MontaH(p->lambda);
    InverteH();

    // k1[j] = (1a linha de H^-1 Phi')[j];  Ky = soma de k1;  Kmpc = k1' F
    Ky = 0.0f;
    Kmpc[0] = Kmpc[1] = Kmpc[2] = 0.0f;
    for (j = 0U; j < Np; j++)
    {
        s = 0.0f;
        for (i = 0U; i < Nu; i++)
        {
            s += Hinv[0][i] * Phi[j][i];
        }
        Ky += s;
        Kmpc[0] += s * F[j][0];
        Kmpc[1] += s * F[j][1];
        Kmpc[2] += s * F[j][2];
    }

    // Estados: modelo parado no ponto de operacao
    x[0] = x[1] = x[2] = 0.0f;
    xp[0] = xp[1] = xp[2] = 0.0f;
    uAnt = uOp;

    desempenho.inicializacao = t0 - Cronometro();
    desempenho.predicaoMax = desempenho.otimizacaoMax = desempenho.lacoMax = 0UL;
    desempenho.iteracoes = desempenho.iteracoesMax = 0U;
    desempenho.naoConvergiu = desempenho.estouros = 0U;
    desempenho.ocupacaoMax = 0.0f;
}

float MPC(float r, float y)
{
    uint32_t t0, t1, t2;
    float    du, u;

    t0 = Cronometro();
    Observador(y);                                       // x^(k|k)
    t1 = Cronometro();
    du = Ky * (r - yOp) - (Kmpc[0] * x[0] + Kmpc[1] * x[1] + Kmpc[2] * x[2]);
    t2 = Cronometro();

    u = uAnt + du;                                       // saturacao do atuador
    if (u < uMin) u = uMin;
    if (u > uMax) u = uMax;
    AtualizaEstado(u);                                   // x^(k+1|k)

    desempenho.predicao   = t0 - t1;
    desempenho.otimizacao = t1 - t2;
    desempenho.iteracoes  = 0U;
    if (desempenho.predicao > desempenho.predicaoMax)     desempenho.predicaoMax = desempenho.predicao;
    if (desempenho.otimizacao > desempenho.otimizacaoMax) desempenho.otimizacaoMax = desempenho.otimizacao;
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
