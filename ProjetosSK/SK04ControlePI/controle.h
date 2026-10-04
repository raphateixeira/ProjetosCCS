#ifndef CONTROLE_H
#define CONTROLE_H

#include <stdint.h>

// Buffers do ensaio
#define N_PRE       20U      // amostras iniciais com r = r_centro (modo unico)
#define N_AMOSTRAS  380U     // tamanho dos buffers (1,9 s)

#define TS          0.005f   // periodo de amostragem [s]
#define U_OP        1.0f     // ponto de operacao na partida [V]

#define U_MIN       0.0f     // saturacao do atuador [V]
#define U_MAX       1.7f     // limite util do LM358 em 3,3 V [V]

extern float r_buf[N_AMOSTRAS];
extern float u_buf[N_AMOSTRAS];
extern float y_buf[N_AMOSTRAS];
extern float i_buf[N_AMOSTRAS];   // estado do integrador

extern volatile uint16_t k;
extern volatile uint16_t ensaio_concluido;

// Parametros alteraveis pelo WATCH durante a execucao
extern volatile float    Kc;
extern volatile float    z0;
extern volatile uint16_t anti_windup;
extern volatile float    r_centro;
extern volatile float    r_amp;
extern volatile uint16_t n_meio;
extern volatile uint16_t reiniciar;
extern volatile uint16_t modo_continuo;

float controle_passo(float y);

#endif
