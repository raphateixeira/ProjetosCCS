#ifndef ENSAIO_H
#define ENSAIO_H

// ============================================================================
//  Ensaio ao degrau em malha aberta (onda quadrada)
//
//      u(k) = u0 + A*q(k),   q(k) = -1 para k < k1 (nivel baixo)
//                            q(k) = +1 / -1 alternando a cada meio periodo Tq/2
//
//  com k1 = tAtraso/Ts. O degrau de subida tem altura 2A. Antes de gravar, u
//  fica tAcomoda segundos no nivel baixo para o filtro acomodar.
//
//  Gravacao: uV[k] = u(k) e yV[k] = y(k), k = 0 .. N_AMOSTRAS-1.
//  "Capacity" do grafico do CCS = N_AMOSTRAS.
// ============================================================================

#include <stdint.h>

#define N_AMOSTRAS  2000U   // amostras gravadas (tamanho de uV e yV); 2000 floats = 4000 palavras (cabe num bloco RAMGS de 4096)

typedef struct
{
    float Ts;         // periodo de amostragem [s]
    float u0;         // nivel medio de u (ponto de operacao) [V]
    float A;          // amplitude: u alterna entre u0 - A e u0 + A [V]
    float Tq;         // periodo da onda quadrada [s] (frequencia 1/Tq)
    float tAtraso;    // tempo no nivel baixo antes da primeira subida [s]
    float tAcomoda;   // tempo no nivel baixo antes de comecar a gravar [s]
} ParametrosDegrau;

extern float uV[N_AMOSTRAS];                 // u(k) gravado [V]
extern float yV[N_AMOSTRAS];                 // y(k) gravado [V]
extern volatile uint16_t k;                  // amostra atual do ensaio
extern volatile uint16_t ensaioConcluido;    // 1 quando uV e yV estao completos
extern volatile uint16_t reiniciar;          // WATCH: 1 = repete o ensaio

// Guarda os parametros, zera o ensaio e aplica o nivel inicial u0 - A
void ConfiguraEnsaioDegrau(const ParametrosDegrau *p);

// u(k) = u0 + A q(k); durante a acomodacao e depois do fim do ensaio, u0 - A
float Degrau(uint16_t k);

// Grava uV[k] = u(k) e yV[k] = y(k) e avanca k (nada grava durante a
// acomodacao); ao completar N_AMOSTRAS, ensaioConcluido = 1
void Grava(float u, float y);

#endif
