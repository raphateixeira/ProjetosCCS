#include "mpc.h"
#include "placa.h"

// Matrizes em RAMGS2..RAMGS5 (secao "mpcdados"): a .bss (RAMLS5) tem so 2048 palavras
#pragma DATA_SECTION(F, "mpcdados")
#pragma DATA_SECTION(Phi, "mpcdados")
#pragma DATA_SECTION(degrau, "mpcdados")
#pragma DATA_SECTION(H, "mpcdados")
#pragma DATA_SECTION(W, "mpcdados")
#pragma DATA_SECTION(Hinv, "mpcdados")
#pragma DATA_SECTION(M, "mpcdados")
#pragma DATA_SECTION(MHinv, "mpcdados")
#pragma DATA_SECTION(P, "mpcdados")
#pragma DATA_SECTION(f, "mpcdados")
static float F[NP_MAX][3];          // linhas C A^j: resposta livre F x^
static float Phi[NP_MAX][NU_MAX];    // resposta ao degrau (Toeplitz inferior)
static float degrau[NP_MAX];         // g_j = C A^(j-1) B
static float H[NU_MAX][NU_MAX];      // Phi'Phi + lambda I
static float W[NU_MAX][NU_MAX];      // copia de trabalho de H
static float Hinv[NU_MAX][NU_MAX];   // H^-1
static float M[M_MAX][NU_MAX];       // restricoes: M dU <= gama
static float MHinv[M_MAX][NU_MAX];   // M H^-1
static float P[M_MAX][M_MAX];        // M H^-1 M'
static float f[NP_MAX];              // resposta livre (desvio)

static float g[NU_MAX];              // gradiente em dU = 0: -Phi'(r~ - f)
static float dU[NU_MAX];             // solucao (movimentos de u)
static float    invPii[M_MAX];       // 1/P_ii
static float    gama[M_MAX];
static float    dd[M_MAX];           // gama - M dU0
static float    lam[M_MAX];          // multiplicadores de Lagrange (partida a quente)
static uint16_t tipo[M_MAX];         // tipo de cada linha de M
static uint16_t indice[M_MAX];       // j de cada linha (para y^: ponto do horizonte)

enum { R_UMAX, R_UMIN, R_DUMAX, R_DUMIN, R_YMAX };

static float A[3][3], B[3];          // modelo aumentado (C = [0 0 1])
static float Lo[3];                  // ganho do observador
static float x[3];                   // x^(k|k)
static float xp[3];                  // x^(k|k-1)
static float uAnt;                   // u(k-1) aplicado [V]

volatile Desempenho desempenho;

// Parametros
static float    a1, a2, b1, b2, uOp, yOp, uMin, uMax;
static float    duMax, yMax, tol;
static uint16_t nM, itMax, usaU, usaDU;
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

// Resposta livre f = F x^(k|k) (desvio em relacao a yOp)
static void RespostaLivre(void)
{
    uint16_t j;

    for (j = 0U; j < Np; j++)
    {
        f[j] = F[j][0] * x[0] + F[j][1] * x[1] + F[j][2] * x[2];
    }
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

// g = -Phi'(r~ - f): gradiente de J em dU = 0
static void MontaGradiente0(float rt)
{
    uint16_t i, j;
    float    s;

    for (i = 0U; i < Nu; i++)
    {
        s = 0.0f;
        for (j = i; j < Np; j++)                     // Phi[j][i] = 0 para j < i
        {
            s -= Phi[j][i] * (rt - f[j]);
        }
        g[i] = s;
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

// Acrescenta uma linha a M (se couber)
static void Linha(uint16_t t, uint16_t j, const float *v, float sinal)
{
    uint16_t i;

    if (nM >= M_MAX)
    {
        return;
    }
    for (i = 0U; i < Nu; i++)
    {
        M[nM][i] = sinal * v[i];
    }
    tipo[nM] = t;
    indice[nM] = j;
    nM++;
}

// Linhas de M: u(k+j) = u(k-1) + soma de du(k..k+j)  ->  linha [1 .. 1 0 .. 0];
// du(k+j) -> linha e_j; y^(k+j+1) -> linha j de Phi
static void MontaRestricoes(uint16_t passoY)
{
    float    v[NU_MAX];
    uint16_t j, i, j0;

    nM = 0U;
    j0 = usaDU ? 1U : 0U;                         // u(k) fica com a linha de du(k)
    if (usaU)
    {
        for (j = j0; j < Nu; j++)
        {
            for (i = 0U; i < Nu; i++)
            {
                v[i] = (i <= j) ? 1.0f : 0.0f;
            }
            Linha(R_UMAX, j, v, 1.0f);
            Linha(R_UMIN, j, v, -1.0f);
        }
    }
    if (usaDU)
    {
        for (j = 0U; j < Nu; j++)
        {
            for (i = 0U; i < Nu; i++)
            {
                v[i] = (i == j) ? 1.0f : 0.0f;
            }
            Linha(R_DUMAX, j, v, 1.0f);
            Linha(R_DUMIN, j, v, -1.0f);
        }
    }
    if (passoY > 0U)
    {
        for (j = passoY - 1U; j < Np; j += passoY)
        {
            Linha(R_YMAX, j, Phi[j], 1.0f);
        }
    }
}

// M H^-1 e P = M H^-1 M' (fixas); 1/P_ii; multiplicadores zerados
static void MontaP(void)
{
    uint16_t i, l, c;
    float    s;

    for (i = 0U; i < nM; i++)
    {
        for (c = 0U; c < Nu; c++)
        {
            s = 0.0f;
            for (l = 0U; l < Nu; l++)
            {
                s += M[i][l] * Hinv[l][c];
            }
            MHinv[i][c] = s;
        }
    }
    for (i = 0U; i < nM; i++)
    {
        for (l = i; l < nM; l++)
        {
            s = 0.0f;
            for (c = 0U; c < Nu; c++)
            {
                s += MHinv[i][c] * M[l][c];
            }
            P[i][l] = s;
            P[l][i] = s;
        }
        invPii[i] = 1.0f / P[i][i];
        lam[i] = 0.0f;
    }
}

// Lado direito das restricoes nesta amostra (dependem de u(k-1) e de f)
static void MontaGama(void)
{
    uint16_t i;
    float    lim;

    for (i = 0U; i < nM; i++)
    {
        switch (tipo[i])
        {
        case R_UMAX: gama[i] = uMax - uAnt; break;
        case R_UMIN: gama[i] = uAnt - uMin; break;
        case R_DUMAX:
            lim = duMax;
            if (usaU && (indice[i] == 0U) && (uMax - uAnt < lim)) lim = uMax - uAnt;
            gama[i] = lim;
            break;
        case R_DUMIN:
            lim = duMax;
            if (usaU && (indice[i] == 0U) && (uAnt - uMin < lim)) lim = uAnt - uMin;
            gama[i] = lim;
            break;
        default:     gama[i] = (yMax - yOp) - f[indice[i]]; break;
        }
    }
}

// Resolve o QP; devolve o numero de iteracoes do Hildreth (0: dU0 ja viavel)
static uint16_t Hildreth(void)
{
    uint16_t i, l, it;
    float    s, w, var;
    uint16_t viavel = 1U;

    for (i = 0U; i < Nu; i++)                            // dU0 = -H^-1 g
    {
        s = 0.0f;
        for (l = 0U; l < Nu; l++)
        {
            s -= Hinv[i][l] * g[l];
        }
        dU[i] = s;
    }
    if (nM == 0U)
    {
        return 0U;
    }

    MontaGama();
    for (i = 0U; i < nM; i++)                            // d = gama - M dU0
    {
        s = 0.0f;
        for (l = 0U; l < Nu; l++)
        {
            s += M[i][l] * dU[l];
        }
        dd[i] = gama[i] - s;
        if (dd[i] < 0.0f)
        {
            viavel = 0U;
        }
    }
    if (viavel)                                          // nenhuma restricao ativa
    {
        for (i = 0U; i < nM; i++)
        {
            lam[i] = 0.0f;
        }
        return 0U;
    }

    for (it = 1U; it <= itMax; it++)                     // subida coordenada no dual
    {
        var = 0.0f;
        for (i = 0U; i < nM; i++)
        {
            s = dd[i];
            for (l = 0U; l < nM; l++)
            {
                s += P[i][l] * lam[l];
            }
            w = lam[i] - s * invPii[i];                  // -(d_i + sum_{l != i} P_il l_l) / P_ii
            if (w < 0.0f) w = 0.0f;
            var += (w - lam[i]) * (w - lam[i]);
            lam[i] = w;
        }
        if (var < tol)
        {
            break;
        }
    }
    if (it > itMax)
    {
        it = itMax;
        desempenho.naoConvergiu++;
    }

    for (l = 0U; l < Nu; l++)                            // dU = dU0 - H^-1 M' l
    {
        s = 0.0f;
        for (i = 0U; i < nM; i++)
        {
            s += MHinv[i][l] * lam[i];
        }
        dU[l] -= s;
    }
    return it;
}

void ConfiguraMPC(const ParametrosMPC *p, float Ts)
{
    uint32_t t0 = Cronometro();
    uint16_t i;

    MontaModelo(&p->modelo);
    uMin = p->uMin;  uMax = p->uMax;
    usaU = p->usaU;  usaDU = p->usaDU;
    duMax = p->duMax;  yMax = p->yMax;
    itMax = p->itMax;  tol = p->tol;
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
    MontaRestricoes(p->passoY);
    MontaP();

    // Estados: modelo parado no ponto de operacao
    x[0] = x[1] = x[2] = 0.0f;
    xp[0] = xp[1] = xp[2] = 0.0f;
    uAnt = uOp;
    for (i = 0U; i < Nu; i++)
    {
        dU[i] = 0.0f;
    }

    desempenho.inicializacao = t0 - Cronometro();
    desempenho.nRestricoes = nM;
    desempenho.predicaoMax = desempenho.otimizacaoMax = desempenho.lacoMax = 0UL;
    desempenho.iteracoes = desempenho.iteracoesMax = 0U;
    desempenho.naoConvergiu = desempenho.estouros = 0U;
    desempenho.ocupacaoMax = 0.0f;
}

float MPC(float r, float y)
{
    uint32_t t0, t1, t2;
    float    u, du;
    uint16_t it;

    t0 = Cronometro();
    Observador(y);                                       // x^(k|k)
    RespostaLivre();                                     // f = F x^
    t1 = Cronometro();
    MontaGradiente0(r - yOp);                            // g = -Phi'(r~ - f)
    it = Hildreth();
    t2 = Cronometro();

    du = dU[0];                                          // garantia: |du(k)| <= duMax
    if (usaDU)
    {
        if (du > duMax)  du = duMax;
        if (du < -duMax) du = -duMax;
    }
    u = uAnt + du;                                       // saturacao do atuador
    if (u < uMin) u = uMin;
    if (u > uMax) u = uMax;
    AtualizaEstado(u);                                   // x^(k+1|k)

    desempenho.predicao   = t0 - t1;
    desempenho.otimizacao = t1 - t2;
    desempenho.iteracoes  = it;
    if (desempenho.predicao > desempenho.predicaoMax)     desempenho.predicaoMax = desempenho.predicao;
    if (desempenho.otimizacao > desempenho.otimizacaoMax) desempenho.otimizacaoMax = desempenho.otimizacao;
    if (it > desempenho.iteracoesMax)                     desempenho.iteracoesMax = it;
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
