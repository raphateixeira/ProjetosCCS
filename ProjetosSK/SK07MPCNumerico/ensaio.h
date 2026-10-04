#ifndef ENSAIO_H
#define ENSAIO_H

// ============================================================================
//  Ensaio de rastreamento em malha fechada (onda quadrada na referencia)
//
//      r(k) = rBaixo                para k < k1 = tInicio/Ts
//      r(k) = rAlto / rBaixo        alternando a cada meio periodo Tq/2
//
//  Antes de gravar, a malha fica tAcomoda segundos com r = rBaixo (o filtro e
//  os estados do controlador acomodam).
//
//  Gravacao, k = 0 .. N_AMOSTRAS-1 ("Capacity" do grafico do CCS = N_AMOSTRAS):
//      rV[k] = r(k), uV[k] = u(k), yV[k] = y(k)            [V]
//      tV[k] = tempo de calculo do controlador na amostra k [us]
//      nV[k] = iteracoes do otimizador na amostra k
// ============================================================================

#include <stdint.h>

#define N_AMOSTRAS  800U    // amostras gravadas (0,8 s com Ts = 1 ms)

typedef struct
{
    float Ts;         // periodo de amostragem [s]
    float rBaixo;     // nivel baixo da referencia [V]
    float rAlto;      // nivel alto da referencia [V]
    float Tq;         // periodo da onda quadrada [s]
    float tInicio;    // tempo em rBaixo antes da primeira subida [s]
    float tAcomoda;   // tempo em malha fechada antes de comecar a gravar [s]
} ParametrosEnsaio;

extern float    rV[N_AMOSTRAS];              // r(k) gravado [V]
extern float    uV[N_AMOSTRAS];              // u(k) gravado [V]
extern float    yV[N_AMOSTRAS];              // y(k) gravado [V]
extern float    tV[N_AMOSTRAS];              // tempo de calculo [us]
extern uint16_t nV[N_AMOSTRAS];              // iteracoes do otimizador
extern volatile uint16_t k;                  // amostra atual do ensaio
extern volatile uint16_t ensaioConcluido;    // 1 quando os vetores estao completos

// Guarda os parametros e zera o ensaio
void ConfiguraEnsaio(const ParametrosEnsaio *p);

// r(k); durante a acomodacao e depois do fim do ensaio, rBaixo
float Referencia(uint16_t k);

// Grava r, u, y, tempo [ciclos] e iteracoes na posicao k e avanca k (nada
// grava durante a acomodacao); ao completar N_AMOSTRAS, ensaioConcluido = 1
void Grava(float r, float u, float y, uint32_t ciclos, uint16_t iteracoes);

#endif
