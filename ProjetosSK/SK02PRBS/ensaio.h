#ifndef ENSAIO_H
#define ENSAIO_H

// ============================================================================
//  Ensaio PRBS em malha aberta
//
//      u(k) = u0                 para k < kPre
//      u(k) = u0 + A*b(k)        para k >= kPre,   b(k) = +1 ou -1
//
//  com kPre = tPre/Ts. b(k) e o bit atual de uma PRBS de 8 bits (LFSR
//  x^8 + x^6 + x^5 + x^4 + 1, periodo de 255 bits) e muda a cada Tb segundos.
//  Antes de gravar, u fica tAcomoda segundos em u0 para o filtro acomodar.
//
//  Gravacao: uV[k] = u(k) e yV[k] = y(k), k = 0 .. N_AMOSTRAS-1.
//  "Capacity" do grafico do CCS = N_AMOSTRAS.
// ============================================================================

#include <stdint.h>

#define N_AMOSTRAS  2000U   // amostras gravadas (tamanho de uV e yV); 2000 floats = 4000 palavras (cabe num bloco RAMGS de 4096)

typedef struct
{
    float    Ts;         // periodo de amostragem [s]
    float    u0;         // nivel medio de u (ponto de operacao) [V]
    float    A;          // amplitude: u alterna entre u0 - A e u0 + A [V]
    float    Tb;         // duracao de cada bit da PRBS [s]
    float    tPre;       // tempo em u0 antes da PRBS, ja gravando [s]
    float    tAcomoda;   // tempo em u0 antes de comecar a gravar [s]
    uint16_t semente;    // estado inicial do LFSR (1 .. 255, nunca 0)
} ParametrosPRBS;

extern float uV[N_AMOSTRAS];                 // u(k) gravado [V]
extern float yV[N_AMOSTRAS];                 // y(k) gravado [V]
extern volatile uint16_t k;                  // amostra atual do ensaio
extern volatile uint16_t ensaioConcluido;    // 1 quando uV e yV estao completos
extern volatile uint16_t reiniciar;          // WATCH: 1 = repete o ensaio

// Guarda os parametros, zera o ensaio e aplica o nivel inicial u0
void ConfiguraEnsaioPRBS(const ParametrosPRBS *p);

// u(k) = u0 + A b(k); durante a acomodacao, antes de kPre e depois do fim, u0
float PRBS(uint16_t k);

// Grava uV[k] = u(k) e yV[k] = y(k) e avanca k (nada grava durante a
// acomodacao); ao completar N_AMOSTRAS, ensaioConcluido = 1
void Grava(float u, float y);

#endif
