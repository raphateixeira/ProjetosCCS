#ifndef PLACA_H
#define PLACA_H

// ============================================================================
//  Placa LAUNCHXL-F28379D + BOOSTXL-3PHGANINV (encaixado em J1-J4)
//
//    Braco A -> ePWM1: EPWM1A (GPIO0) chave de cima, EPWM1B (GPIO1) chave de baixo
//    Braco B -> ePWM2: EPWM2A (GPIO2) chave de cima, EPWM2B (GPIO3) chave de baixo
//    Braco C -> ePWM3: EPWM3A (GPIO4) chave de cima, EPWM3B (GPIO5) chave de baixo
//    Habilita o driver -> GPIO124 em nivel alto (como no MotorControl SDK da TI)
//
//  PWM simetrico (contador sobe e desce, EPWMCLK = 100 MHz). Em cada braco a
//  chave de cima fica ligada uma fracao d do periodo (razao ciclica) e a de
//  baixo e o complemento, com tempo morto entre elas. A tensao media do braco,
//  medida a partir do ponto medio do barramento, e
//
//      v = (d - 1/2) Vcc      <=>      d = 1/2 + v/Vcc
//
//  O inicio de cada periodo do ePWM1 (contador = 0) chama o laco do PWM (ISR
//  definida no main.c): Ts = 1/fPWM.
// ============================================================================

#include "driverlib.h"
#include "device.h"

#define EPWMCLK_HZ  100e6f    // relogio dos ePWM: SYSCLK/2 = 100 MHz

// Inicializa o microcontrolador (clock, GPIO, interrupcoes) e os ePWM 1, 2 e 3
// com frequencia fPWM [Hz] e tempo morto tMorto [s]; Vcc [V] e a tensao do
// barramento (para converter tensao em razao ciclica); associa o laco do PWM.
void ConfiguraF28379D(float fPWM, float tMorto, float Vcc, void (*lacoPWM)(void));

// Habilita o driver, a interrupcao e liga os contadores: o inversor comeca a chavear.
void IniciaPWM(void);

// va, vb, vc: tensoes de fase [V] (em relacao a Vcc/2) -> razoes ciclicas dos
// bracos A, B e C, limitadas a 0 .. 1
void PWM2v(float va, float vb, float vc);

// Fim do laco do PWM: limpa o pedido do ePWM1 e libera o grupo de interrupcoes
static inline void FimLacoPWM(void)
{
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

#endif
