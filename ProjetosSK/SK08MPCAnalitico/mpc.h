#ifndef MPC_H
#define MPC_H

// ============================================================================
//  MPC sem restricoes, solucao analitica calculada antes do laco
//
//  Modelo (ARX da identificacao do Sallen-Key em forma canonica observavel, desvios u~ = u - uOp,
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
//  Sem restricoes, o minimo tem forma fechada:
//      dU* = (Phi'Phi + lambda I)^-1 Phi'(r~ - F x^)
//  e so a 1a linha (k1) interessa:  du(k) = Ky r~(k) - Kmpc x^(k|k),
//      Ky = k1' [1 .. 1]',  Kmpc = k1' F   (Ky e a 3a componente de Kmpc sao iguais)
//  Toda a algebra (F, Phi, H, H^-1, k1, Ky, Kmpc) e feita UMA vez, em ConfiguraMPC.
// ============================================================================

#include <stdint.h>

#define NP_MAX  100U     // maior horizonte de predicao
#define NU_MAX  20U      // maior horizonte de controle

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
    float     uMin, uMax;     // limites do atuador [V] (saturacao depois do calculo)
} ParametrosMPC;

// Desempenho (leitura pelo WATCH/Expressions). Tempos em ciclos de CPU (5 ns).
typedef struct
{
    uint32_t inicializacao;   // ConfiguraMPC: F, Phi, L do observador, H, inversao de H, Ky e Kmpc
    uint32_t predicao;        // ultima amostra: observador
    uint32_t predicaoMax;
    uint32_t otimizacao;      // ultima amostra: du(k) = Ky r~ - Kmpc x^
    uint32_t otimizacaoMax;
    uint32_t laco;            // ultima amostra: laco de amostragem inteiro (ISR)
    uint32_t lacoMax;
    float    ocupacaoMax;     // lacoMax em % de Ts
    uint16_t iteracoes;       // ultima amostra: iteracoes do otimizador
    uint16_t iteracoesMax;
    uint16_t naoConvergiu;    // amostras em que o otimizador parou em itMax
    uint16_t estouros;        // amostras em que o laco passou de Ts
} Desempenho;

extern volatile Desempenho desempenho;
extern float Ky;               // ganho da referencia
extern float Kmpc[3];          // realimentacao de estados

// Monta o modelo aumentado, F, Phi, o observador e as matrizes do metodo (mede
// o tempo gasto). Estados zerados: x^ = 0, u(k-1) = uOp.
void ConfiguraMPC(const ParametrosMPC *p, float Ts);

// Uma amostra do MPC: recebe r(k) e y(k) [V], devolve u(k) [V] (ja saturado)
float MPC(float r, float y);

// Fim do laco de amostragem: tempo total desde t0 (Cronometro no inicio da ISR),
// ocupacao de Ts e estouros
void FimLaco(uint32_t t0);

#endif
