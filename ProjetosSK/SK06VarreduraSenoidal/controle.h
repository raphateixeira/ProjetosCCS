#ifndef CONTROLE_H
#define CONTROLE_H

#include <stdint.h>

// Resposta em frequencia por seno em degraus de frequencia, com demodulacao sincrona
// (lock-in) na propria placa. Para cada frequencia da tabela:
//   1) aplica u = U_OP + U_AMP*sen(2*pi*m/N), N = periodo em amostras (f = 1/(N*Ts));
//   2) espera o transitorio (acomodacao);
//   3) integra y e u contra sen e cos ao longo de um numero inteiro de periodos e calcula
//      H = Y/U -> ganho e fase.
// Resultado: tabelas freq_hz[], ganho[], ganho_db[], fase_graus[] (NF valores) e o sinal y(k)
// da varredura inteira em y_buf[] (N_VARREDURA = 8730 amostras; u e reconstruido na analise,
// pois e deterministico: U_OP nas N_INICIAL primeiras amostras e, em cada frequencia,
// U_OP + U_AMP sen(2 pi j / N), j = 0, 1, ... desde o inicio daquela frequencia).

#define NF            15U     // numero de frequencias
#define TS_ENSAIO     0.005f  // periodo de amostragem [s] (igual a TS_S de perifericos.h)
#define U_OP          1.0f    // ponto de operacao [V]
#define U_AMP         0.3f    // amplitude da senoide [V] (u entre 0,7 e 1,3 V)
#define N_INICIAL     400U    // amostras em U_OP antes do ensaio (2 s)
#define N_ACOMODA_MIN 100U    // acomodacao minima em cada frequencia (0,5 s >> 5*tau)
#define N_MEDE_MIN    400U    // integracao minima em cada frequencia (2 s)
#define N_VARREDURA   8730U   // amostras de toda a varredura (N_INICIAL + soma das frequencias)
#define N_BUF         N_VARREDURA   // y_buf com EXATAMENTE o tamanho da varredura: o grafico do CCS
                                    // exporta as ultimas "Capacity" posicoes do vetor; com
                                    // Capacity = 8730 = N_BUF sai o vetor inteiro, de 0 a 8729

// Resultados (exportar estes vetores: NF = 15 valores cada)
extern float freq_hz[NF];
extern float ganho[NF];        // |H| = |Y|/|U|
extern float ganho_db[NF];     // 20 log10 |H|
extern float fase_graus[NF];   // fase de H, em graus
extern float amp_u[NF];        // amplitude medida de u [V]
extern float amp_y[NF];        // amplitude medida de y [V]

// y(k) da varredura inteira ("Capacity" do grafico do CCS = 8730)
extern float y_buf[N_BUF];

extern volatile uint16_t k;                 // amostras gravadas (termina em 8730)
extern volatile uint16_t i_freq;            // frequencia em andamento (0..NF-1)
extern volatile uint16_t ensaio_concluido;  // 1 quando todas as frequencias terminaram
extern volatile uint16_t reiniciar;         // opcional (WATCH): 1 = repete o ensaio

void  controle_inicia(void);
float controle_valor_inicial(void);
float controle_passo(float y);

#endif
