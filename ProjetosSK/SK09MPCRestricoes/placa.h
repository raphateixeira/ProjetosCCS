#ifndef PLACA_H
#define PLACA_H

// ============================================================================
//  Placa LAUNCHXL-F28379D: configuracao e acesso as grandezas do ensaio
//
//    y(k) -> ADC-A, canal ADCINA3 (pino J3.26), 12 bits, 0 .. VREF
//    u(k) -> DAC-A, saida DACOUTA (pino J3.30), 12 bits, 0 .. VREF
//    Ts   -> CPU Timer 0 dispara a conversao do ADC a cada Ts; o fim da
//            conversao chama o laco de amostragem (ISR definida no main.c)
//    Cronometro -> CPU Timer 1 livre, 1 contagem por ciclo de CPU (5 ns a 200 MHz):
//            mede o tempo de calculo (ciclos = t0 - t1, contagem decrescente)
//
//  Ordem no laco de amostragem:  y = ADC2y();  ...  DAC2u(u);
//  u(k) so chega a planta depois da leitura de y(k): atraso de uma amostra.
// ============================================================================

#include "driverlib.h"
#include "device.h"

#define VREF        3.0f      // referencia do ADC e do DAC (VREFHI) [V]
#define FUNDO_ESC   4095.0f   // fundo de escala de 12 bits

// Inicializa o microcontrolador (clock, GPIO, interrupcoes), o ADC, o DAC e o
// timer de amostragem com periodo Ts [s]; associa o laco de amostragem (ISR).
void ConfiguraF28379D(float Ts, void (*lacoAmostragem)(void));

// Habilita a interrupcao e liga o timer: a amostragem comeca.
void IniciaAmostragem(void);

// y(k): tensao na saida do filtro [V], lida no ADC (ADCINA3)
static inline float ADC2y(void)
{
    return (float)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0) * VREF / FUNDO_ESC;
}

// u(k): tensao aplicada na entrada do filtro [V], escrita no DAC (DACOUTA),
// limitada a faixa 0 .. VREF
static inline void DAC2u(float u)
{
    if (u < 0.0f) u = 0.0f;
    if (u > VREF) u = VREF;
    DAC_setShadowValue(DACA_BASE, (uint16_t)(u * FUNDO_ESC / VREF));
}

// Cronometro: contagem do CPU Timer 1 (decrescente, um ciclo de CPU por contagem).
// Ciclos gastos entre duas leituras: t0 - t1 (vale mesmo se o contador der a volta).
static inline uint32_t Cronometro(void)
{
    return CPUTimer_getTimerCount(CPUTIMER1_BASE);
}

#define CICLOS_POR_US   (DEVICE_SYSCLK_FREQ / 1000000U)   // 200 ciclos = 1 us

// Estouro de Ts: a conversao seguinte terminou antes de o laco de amostragem
// acabar (calculo mais longo que Ts; amostras perdidas). Devolve 1 e limpa o aviso.
static inline uint16_t EstourouTs(void)
{
    if (ADC_getInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1))
    {
        ADC_clearInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1);
        return 1U;
    }
    return 0U;
}

// Fim do laco de amostragem: limpa o pedido do ADC e libera o grupo de
// interrupcoes para a proxima amostra
static inline void FimAmostragem(void)
{
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

#endif
