/*
 * main.c - Gerador PRBS idêntico ao bloco PN Sequence Generator do Simulink
 * Polinômio: [9 5 0] (9 bits, período 511 bits) | Ts = 50us (Sem divisor)
 * Nb = 4 amostras por bit: Tb = 200us | Ponto Nominal CMPA = 865
 */
#include <stdint.h>
#include "Perifericos_setup.h"

#pragma DATA_SECTION(DadosBuck, "dados_arx");
float DadosBuck[3][TAM_EXP];

volatile uint16_t idx = 0;
volatile uint16_t gravacao_concluida = 0;

volatile uint16_t adc_raw = 0;
volatile float v_pino = 0.0f;
volatile float v_carga_real = 0.0f;
volatile float duty_atual = 0.346f; // Duty nominal: CMPA = 865 (34.6%)

// Faixa de pequeno-sinal (34.6% +/- 5%)
#define PRBS_DUTY_LOW   0.296f  // CMPA = 740
#define PRBS_DUTY_HIGH  0.396f  // CMPA = 990

// 30 segundos a 50 us por amostra = 600.000 amostras
#define SAMPLES_STABILIZE  600000UL

// Amostras por bit da PRBS: Tb = PRBS_NB * Ts = 4 * 50us = 200us
#define PRBS_NB  4U

// Registrador de deslocamento de 9 bits (ones(1, 9) -> 0x01FF)
static uint16_t lfsr = 0x01FF;

// Contador de amostras dentro do bit atual e nível do bit atual
static uint16_t prbs_cont = 0;
static float prbs_duty = PRBS_DUTY_LOW;

// ALTERAÇÃO CRÍTICA: uint32_t para suportar contagens maiores que 65.535
static uint32_t settle_counter = 0;

/*
 * LFSR - Polinômio [9 5 0] (x^9 + x^5 + 1)
 * Chamado a cada Ts = 50us (uma amostra por ciclo de PWM); o registrador só
 * avança a cada PRBS_NB chamadas: cada bit dura PRBS_NB amostras (Tb = 200us)
 */
static inline float Gerar_Proximo_PRBS_Simulink(void) {
    if (prbs_cont == 0) {
        uint16_t bit9 = (lfsr >> 8) & 0x0001; // Bit 9
        uint16_t bit5 = (lfsr >> 4) & 0x0001; // Bit 5
        uint16_t feedback = bit9 ^ bit5;      // XOR entre bit 9 e bit 5

        lfsr = ((lfsr << 1) | feedback) & 0x01FF; // Desloca e insere feedback no LSB

        // Mapeia saída do Simulink (0 ou 1) para os níveis de Duty
        prbs_duty = (feedback != 0) ? PRBS_DUTY_HIGH : PRBS_DUTY_LOW;
    }

    prbs_cont++;
    if (prbs_cont >= PRBS_NB) {
        prbs_cont = 0;
    }

    return prbs_duty;
}

int main(void){
    InitSysCtrl();
    DINT;
    InitPieCtrl();
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();

    Setup_GPIO();
    Setup_ADC();
    Setup_ePWM();

    EINT;
    ERTM;

    while(1){
        // Sincronizado com o disparo do ePWM (Ts = 50 us)
        while(AdcaRegs.ADCINTFLG.bit.ADCINT1 == 0);
        AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;

        adc_raw = AdcaResultRegs.ADCRESULT0;
        v_pino = ((float)adc_raw * VREF_ADC) * (1.0f / MAX_ADC_VAL);
        v_carga_real = v_pino * FATOR_DIVISOR;

        // Mantém a carga no ponto de operação nominal por 30 segundos
        if(settle_counter < SAMPLES_STABILIZE){
            settle_counter++;
            duty_atual = 0.346f; // Estabiliza em 34.6% (CMPA = 865)
        }
        else {
            // Após 30s, inicia a perturbação PRBS e a coleta de dados simultaneamente
            duty_atual = Gerar_Proximo_PRBS_Simulink();

            if(!gravacao_concluida){
                if(idx < TAM_EXP){
                    DadosBuck[0][idx] = (float)idx * TS_SEC;
                    DadosBuck[1][idx] = duty_atual;
                    DadosBuck[2][idx] = v_carga_real;
                    idx++;
                } else {
                    gravacao_concluida = 1;
                }
            }
        }

        // Atualização do ePWM1A
        EPwm1Regs.CMPA.bit.CMPA = (uint16_t)(duty_atual * 2500.0f);
    }
}
