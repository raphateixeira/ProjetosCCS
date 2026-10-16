/*
 * Perifericos_setup.h
 */
#ifndef PERIFERICOS_SETUP_H_
#define PERIFERICOS_SETUP_H_

#include "F28x_Project.h"

// Constantes do sensor de tensão originais do projeto
#define R1               27000.0f
#define R2               3300.0f
#define FATOR_DIVISOR    7.84613f // Fator calibrado empiricamente (Multímetro: 9.05V)
#define VREF_ADC         3.3f
#define MAX_ADC_VAL      4095.0f

// Configuração do Experimento (ARX)
#define TAM_EXP          2044     // Um período da PRBS: 511 bits x Nb = 4 amostras (102,2 ms)
#define TS_SEC           0.00005f // Ts = 50 us (Frequência de 20 kHz, definida pelo TBPRD)

// Protótipos das funções
void Setup_GPIO(void);
void Setup_ePWM(void);
void Setup_ADC(void);

#endif /* PERIFERICOS_SETUP_H_ */
