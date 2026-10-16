#ifndef SENOIDES_H
#define SENOIDES_H

// ============================================================================
//  Tres senoides defasadas de 120 graus (sistema trifasico equilibrado)
//
//      va(n) = V(n) sin(theta(n))
//      vb(n) = V(n) sin(theta(n) - 2 pi/3)
//      vc(n) = V(n) sin(theta(n) + 2 pi/3)
//
//  theta(n+1) = theta(n) + 2 pi f Ts   (angulo, mantido em 0 .. 2 pi)
//  V(n) = Vp n/nRampa durante a rampa (nRampa = tRampa/Ts) e Vp depois.
//
//  Gravacao (depois da rampa): vaV[n], vbV[n], vcV[n], n = 0 .. N_AMOSTRAS-1.
//  "Capacity" do grafico do CCS = N_AMOSTRAS.
// ============================================================================

#include <stdint.h>

#define N_AMOSTRAS  2000U   // amostras gravadas; 2000 floats = 4000 palavras (cabe num bloco RAMGS de 4096)

typedef struct
{
    float fPWM;       // frequencia de chaveamento [Hz] (periodo de amostragem Ts = 1/fPWM)
    float tMorto;     // tempo morto entre as chaves de um braco [s]
    float Vcc;        // tensao do barramento CC [V]
    float Vp;         // amplitude das tensoes de fase [V] (limitada a Vcc/2)
    float f;          // frequencia das senoides [Hz]
    float tRampa;     // tempo de subida da amplitude, de 0 a Vp [s]
} ParametrosInversor;

extern float vaV[N_AMOSTRAS];                // va gravada [V]
extern float vbV[N_AMOSTRAS];                // vb gravada [V]
extern float vcV[N_AMOSTRAS];                // vc gravada [V]
extern volatile uint16_t n;                  // amostra gravada atual
extern volatile uint16_t gravacaoConcluida;  // 1 quando vaV, vbV e vcV estao completos

// Guarda os parametros e zera o angulo, a rampa e a gravacao
void ConfiguraSenoides(const ParametrosInversor *p);

// Calcula va, vb, vc no instante atual e avanca o angulo de 2 pi f Ts
void Senoides(float *va, float *vb, float *vc);

// Grava vaV[n], vbV[n], vcV[n] e avanca n (nada grava durante a rampa);
// ao completar N_AMOSTRAS, gravacaoConcluida = 1
void Grava(float va, float vb, float vc);

#endif
