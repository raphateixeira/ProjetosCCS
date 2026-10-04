#ifndef CONTROLE_H
#define CONTROLE_H

#include <stdint.h>

// Ensaio: referencia em onda quadrada
#define N_PRE       20U      // amostras iniciais
#define N_MEIO      60U      // amostras em cada nivel (60*Ts = 300 ms)
#define N_CICLOS    3U       // ciclos completos da onda quadrada
#define N_AMOSTRAS  (N_PRE + 2U * N_MEIO * N_CICLOS)   // 380 amostras

#define U_OP        1.0f     // ponto de operacao [V]
#define R_ALTO      1.2f     // nivel alto da referencia [V]
#define R_BAIXO     0.8f     // nivel baixo da referencia [V]

#define U_MIN       0.0f     // saturacao do atuador [V]
#define U_MAX       1.7f     // limite util do LM358 em 3,3 V [V]

extern float r_buf[N_AMOSTRAS];
extern float u_buf[N_AMOSTRAS];
extern float y_buf[N_AMOSTRAS];

extern volatile uint16_t k;
extern volatile uint16_t ensaio_concluido;

// Parametros alteraveis pelo WATCH durante a execucao
extern volatile float    Kp;
extern volatile uint16_t usar_bias;
extern volatile uint16_t reiniciar;
extern volatile uint16_t modo_continuo;

float controle_passo(float y);

#endif
