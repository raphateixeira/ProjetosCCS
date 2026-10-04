#ifndef CONTROLE_H
#define CONTROLE_H

#include <stdint.h>

// Ensaio em malha aberta: uma senoide u = U_OP + U_AMP sen(2 pi k / N_PERIODO).
// f = 1/(N_PERIODO*Ts): N_PERIODO = 40 -> 5 Hz (corte do filtro ~ 6,5 Hz).
// A senoide ja roda durante a acomodacao: a gravacao pega o regime permanente.

#define N_AMOSTRAS  800U     // tamanho do ensaio (4 s); "Capacity" do grafico do CCS = 800
#define N_ACOMODA   400U     // amostras com a senoide rodando antes de gravar (2 s)
#define N_PERIODO   40U      // amostras por periodo (f = 1/(N_PERIODO*0,005 s) = 5 Hz)
#define U_OP        1.0f     // ponto de operacao [V]
#define U_AMP       0.3f     // amplitude [V] (u entre 0,7 e 1,3 V)

extern float u_buf[N_AMOSTRAS];
extern float y_buf[N_AMOSTRAS];
extern volatile uint16_t k;
extern volatile uint16_t ensaio_concluido;
extern volatile uint16_t reiniciar;          // opcional (WATCH): 1 = repete o ensaio

void  controle_inicia(void);
float controle_valor_inicial(void);
float controle_passo(float y);

#endif
