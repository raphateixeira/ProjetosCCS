#ifndef MPC_H
#define MPC_H

// ============================================================================
//  MPC com restricoes: programa quadratico (QP) resolvido a cada amostra
//
//  Modelo (ARX da Nota 02 em forma canonica observavel, desvios u~ = u - uOp,
//  y~ = y - yOp) e modelo aumentado com integrador (Wang, 2009):
//      xm(k+1) = Am xm(k) + Bm u~(k),       y~(k) = Cm xm(k)
//      x(k)    = [xm(k) - xm(k-1); y~(k)]:  x(k+1) = A x(k) + B du(k),  y~(k) = C x(k)
//  Observador corrente (polos em alfaObs):
//      x^(k|k) = x^(k|k-1) + L [y~(k) - C x^(k|k-1)],   x^(k+1|k) = A x^(k|k) + B du(k)
//  Predicao no horizonte Np, com Nu movimentos dU = [du(k) .. du(k+Nu-1)]':
//      Y^ = F x^(k|k) + Phi dU     (F x^: resposta livre; Phi: resposta ao degrau)
//  Custo:
//      J = sum_{j=1..Np} [r~(k) - y~^(k+j)]^2 + lambda sum_{j=0..Nu-1} du(k+j)^2
//        = 1/2 dU' H dU + g' dU + cte,   H = Phi'Phi + lambda I,   g = -Phi'(r~ - F x^)
//  Horizonte deslizante: so du(k) e aplicado;  u(k) = sat(u(k-1) + du(k)).
//
//      min J(dU)  s.a.  M dU <= gama(k)
//  Restricoes (ligadas no main.c), j = 0 .. Nu-1:
//      u   : uMin <= u(k+j) = u(k-1) + du(k) + ... + du(k+j) <= uMax
//      du  : |du(k+j)| <= duMax
//      y   : y^(k+j) <= yMax nos pontos j = passoY, 2 passoY, ... <= Np  (linhas de Phi)
//  Com u e du ligadas, as linhas de u(k) e de du(k) sao paralelas (dual
//  degenerado): a de u(k) sai, e du(k) fica com o limite mais apertado.
//  Metodo de Hildreth (subida coordenada no problema dual):
//      dU0 = -H^-1 g (minimo sem restricoes; se viavel, pronto)
//      P = M H^-1 M' (fixa), d = gama - M dU0
//      l_i <- max(0, -(d_i + sum_{j != i} P_ij l_j) / P_ii)
//      dU  = dU0 - H^-1 M' l
//  Partida a quente (l da amostra anterior), no maximo itMax iteracoes; o
//  primeiro movimento e recortado a +-duMax (garantia se nao convergir).
// ============================================================================

#include <stdint.h>

#define NP_MAX  100U     // maior horizonte de predicao
#define NU_MAX  10U      // maior horizonte de controle
#define M_MAX   60U      // maior numero de restricoes (P: M_MAX x M_MAX floats)

typedef struct
{
    float a1, a2, b1, b2;     // coeficientes do ARX
    float uOp, yOp;           // ponto de operacao da identificacao [V]
} ModeloARX;

typedef struct
{
    ModeloARX modelo;
    uint16_t  Np;             // horizonte de predicao [amostras]
    uint16_t  Nu;             // horizonte de controle [amostras]
    float     lambda;         // peso de du no custo
    float     alfaObs;        // polos do observador (0: deadbeat; perto de 1: filtra mais)
    float     uMin, uMax;     // limites do atuador [V] (tambem a restricao de u)
    uint16_t  usaU;           // 1: restricao uMin <= u <= uMax no QP
    uint16_t  usaDU;          // 1: restricao |du| <= duMax no QP
    float     duMax;          // [V por amostra]
    uint16_t  passoY;         // 0: sem restricao de saida; n: y^ <= yMax a cada n pontos
    float     yMax;           // [V]
    uint16_t  itMax;          // limite de iteracoes do Hildreth
    float     tol;            // parada: sum (l_novo - l)^2 < tol
} ParametrosMPC;

// Desempenho (leitura pelo WATCH/Expressions). Tempos em ciclos de CPU (5 ns).
typedef struct
{
    uint32_t inicializacao;   // ConfiguraMPC: F, Phi, L do observador, H, H^-1, M, M H^-1 e P
    uint32_t predicao;        // ultima amostra: observador e resposta livre F x^
    uint32_t predicaoMax;
    uint32_t otimizacao;      // ultima amostra: g, dU0, viabilidade e Hildreth
    uint32_t otimizacaoMax;
    uint32_t laco;            // ultima amostra: laco de amostragem inteiro (ISR)
    uint32_t lacoMax;
    float    ocupacaoMax;     // lacoMax em % de Ts
    uint16_t nRestricoes;     // linhas de M
    uint16_t iteracoes;       // ultima amostra: iteracoes do otimizador
    uint16_t iteracoesMax;
    uint16_t naoConvergiu;    // amostras em que o otimizador parou em itMax
    uint16_t estouros;        // amostras em que o laco passou de Ts
} Desempenho;

extern volatile Desempenho desempenho;

// Monta o modelo aumentado, F, Phi, o observador e as matrizes do metodo (mede
// o tempo gasto). Estados zerados: x^ = 0, u(k-1) = uOp.
void ConfiguraMPC(const ParametrosMPC *p, float Ts);

// Uma amostra do MPC: recebe r(k) e y(k) [V], devolve u(k) [V] (ja saturado)
float MPC(float r, float y);

// Fim do laco de amostragem: tempo total desde t0 (Cronometro no inicio da ISR),
// ocupacao de Ts e estouros
void FimLaco(uint32_t t0);

#endif
