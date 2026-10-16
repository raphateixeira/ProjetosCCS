#ifndef ENSAIO_H
#define ENSAIO_H

// ============================================================================
//  Ensaio ao degrau em malha aberta (onda quadrada em torno de um ponto de operacao)
//
//  1. Rampa:       u sobe de 0 a vBaixo em tRampa (partida suave do buck)
//  2. Acomodacao:  u = vBaixo durante tAcomoda (a saida chega ao ponto de operacao)
//  3. Gravacao:    u(k) = vBaixo para k < k1 = tAtraso/Ts; depois alterna entre
//                  vAlto e vBaixo a cada meio periodo Tq/2 (onda quadrada)
//  4. Fim:         u = vBaixo
//
//  O degrau de subida tem altura vAlto - vBaixo, a partir do offset vBaixo.
//  Gravacao: uV[k] = u(k) e yV[k] = y(k), k = 0 .. N_AMOSTRAS-1.
//  "Capacity" do grafico do CCS = N_AMOSTRAS.
// ============================================================================

#include <stdint.h>

#define N_AMOSTRAS  2000U   // amostras gravadas (tamanho de uV e yV); 2000 floats = 4000 palavras (cabe num bloco RAMGS de 4096)

typedef struct
{
    float Ts;         // periodo de amostragem [s]
    float vBaixo;     // nivel baixo de u (ponto de operacao) [V]
    float vAlto;      // nivel alto de u [V]
    float Tq;         // periodo da onda quadrada [s] (frequencia 1/Tq)
    float tAtraso;    // tempo no nivel baixo antes da primeira subida [s]
    float tRampa;     // duracao da rampa de 0 a vBaixo [s]
    float tAcomoda;   // tempo em vBaixo depois da rampa, antes de gravar [s]
} ParametrosDegrau;

extern float uV[N_AMOSTRAS];                 // u(k) gravado [V]
extern float yV[N_AMOSTRAS];                 // y(k) gravado [V]
extern volatile uint16_t k;                  // amostra atual do ensaio
extern volatile uint16_t ensaioConcluido;    // 1 quando uV e yV estao completos
extern volatile uint16_t reiniciar;          // WATCH: 1 = repete o ensaio (sem a rampa)

// Guarda os parametros e zera o ensaio (comeca pela rampa)
void ConfiguraEnsaioDegrau(const ParametrosDegrau *p);

// u(k): rampa, acomodacao em vBaixo, onda quadrada ou vBaixo depois do fim
float Degrau(uint16_t k);

// Grava uV[k] = u(k) e yV[k] = y(k) e avanca k (nada grava durante a rampa e
// a acomodacao); ao completar N_AMOSTRAS, ensaioConcluido = 1
void Grava(float u, float y);

#endif
