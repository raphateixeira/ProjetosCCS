#ifndef PLACA_H
#define PLACA_H

// ============================================================================
//  Placa LAUNCHXL-F28379D comandando um conversor buck (chave + diodo)
//
//    u(k) -> ePWM1: EPWM1A (GPIO0, pino J4.40) -> gate driver da chave
//    y(k) -> ADC-A, canal ADCINA3 (pino J3.26), 12 bits, 0 .. VREF, por um
//            divisor resistivo de ganho kSensor (y_ADC = kSensor * y)
//    Ts   -> o ePWM1 dispara a conversao do ADC a cada Ts/(1/fPWM) periodos
//            (contador = 0, meio do intervalo com a chave ligada, longe das
//            comutacoes); o fim da conversao chama o laco de amostragem
//
//  PWM simetrico (contador sobe e desce, EPWMCLK = 100 MHz): a chave fica
//  ligada uma fracao d do periodo. Em conducao continua a tensao media
//  aplicada ao filtro LC e
//
//      u = d Vin      <=>      d = u/Vin
//
//  Ordem no laco de amostragem:  y = ADC2y();  ...  PWM2u(u);
//  O novo d so vale no proximo periodo do PWM: atraso de uma amostra.
// ============================================================================

#include "driverlib.h"
#include "device.h"

#define VREF        3.0f      // referencia do ADC (VREFHI) [V]
#define FUNDO_ESC   4095.0f   // fundo de escala de 12 bits
#define EPWMCLK_HZ  100e6f    // relogio dos ePWM: SYSCLK/2 = 100 MHz

typedef struct
{
    float fPWM;       // frequencia de chaveamento [Hz]
    float Ts;         // periodo de amostragem [s] (1 a 15 periodos do PWM)
    float Vin;        // tensao de entrada do buck [V]
    float uMax;       // maior tensao media comandada [V]: d <= uMax/Vin
    float kSensor;    // ganho do divisor resistivo de y (tensao no ADC / y)
} ParametrosConversor;

// Inicializa o microcontrolador (clock, GPIO, interrupcoes), o ePWM1 e o ADC;
// associa o laco de amostragem (ISR). A chave comeca desligada (d = 0).
void ConfiguraF28379D(const ParametrosConversor *c, void (*lacoAmostragem)(void));

// Habilita a interrupcao e liga o contador do ePWM1: chaveamento e amostragem comecam.
void IniciaAmostragem(void);

// u(k): tensao media aplicada ao filtro [V] -> razao ciclica d = u/Vin,
// limitada a 0 .. uMax/Vin
void PWM2u(float u);

// y(k): tensao na saida do buck [V], lida no ADC (ADCINA3) e corrigida pelo divisor
float ADC2y(void);

// Fim do laco de amostragem: limpa o pedido do ADC e libera o grupo de
// interrupcoes para a proxima amostra
static inline void FimAmostragem(void)
{
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

#endif
