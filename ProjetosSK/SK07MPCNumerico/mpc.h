#ifndef MPC_H
#define MPC_H

// ============================================================================
//  MPC convencional: custo minimizado pelo metodo do gradiente a cada amostra
//
//  Modelo: ARX identificado (identificacao do Sallen-Key), escrito nas VARIACOES de y e u
//      dy(k) = a1 dy(k-1) + a2 dy(k-2) + b1 du(k-1) + b2 du(k-2)
//      dy(k) = y(k) - y(k-1),  du(k) = u(k) - u(k-1)
//  (nas variacoes nao entra o ponto de operacao, e um erro de ganho do modelo
//  nao deixa erro em regime)
//
//  Previsao: simula o modelo Np passos a frente, a partir das ultimas medidas,
//  para um plano de Nu movimentos futuros dU = [du(k) .. du(k+Nu-1)] (depois
//  deles, u fica constante).
//
//  Custo:
//      J = sum_{j=1..Np} [r(k) - y^(k+j)]^2 + lambda sum_{i=0..Nu-1} du(k+i)^2
//
//  Minimizacao pelo gradiente, nIter iteracoes por amostra:
//      du_i <- du_i - passo * (-sum_j s_(j-i) e_j + lambda du_i),   e_j = r - y^(k+j)
//  s_j: resposta do modelo a um degrau unitario em u (quanto y^(k+j) muda por
//  unidade de du(k)); calculada uma vez, na inicializacao.
//  Cada amostra parte do plano da amostra anterior, deslocado de uma posicao.
//  So o primeiro movimento e aplicado: u(k) = sat(u(k-1) + du(k)).
// ============================================================================

#include <stdint.h>

#define NP_MAX  100U     // maior horizonte de previsao
#define NU_MAX  10U      // maior numero de movimentos planejados

typedef struct
{
    float a1, a2, b1, b2;     // coeficientes do ARX
} ModeloARX;

typedef struct
{
    ModeloARX modelo;
    uint16_t  Np;             // horizonte de previsao [amostras]
    uint16_t  Nu;             // movimentos planejados [amostras]
    float     lambda;         // peso de du no custo
    uint16_t  nIter;          // iteracoes do gradiente por amostra
    float     uMin, uMax;     // limites do atuador [V]
    float     uInicial;       // u aplicado na partida [V]
} ParametrosMPC;

// Desempenho (leitura pelo WATCH/Expressions). Tempos em ciclos de CPU (5 ns).
typedef struct
{
    uint32_t inicializacao;   // ConfiguraMPC: resposta ao degrau e passo do gradiente
    uint32_t calculo;         // ultima amostra: MPC() inteiro (previsao + gradiente)
    uint32_t calculoMax;
    uint32_t laco;            // ultima amostra: laco de amostragem inteiro (ISR)
    uint32_t lacoMax;
    float    ocupacaoMax;     // lacoMax em % de Ts
    uint16_t iteracoes;       // iteracoes do gradiente por amostra (= nIter)
    uint16_t estouros;        // amostras em que o laco passou de Ts
} Desempenho;

extern volatile Desempenho desempenho;

// Calcula a resposta ao degrau do modelo e o passo do gradiente (mede o tempo).
void ConfiguraMPC(const ParametrosMPC *p, float Ts);

// Uma amostra do MPC: recebe r(k) e y(k) [V], devolve u(k) [V] (ja saturado)
float MPC(float r, float y);

// Fim do laco de amostragem: tempo total desde t0 (Cronometro no inicio da ISR),
// ocupacao de Ts e estouros
void FimLaco(uint32_t t0);

#endif
