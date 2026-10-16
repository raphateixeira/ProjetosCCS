/*
 * Perifericos_setup.c
 */
#include "Perifericos_setup.h"

void Setup_GPIO(void){
    EALLOW; // Libera registradores protegidos

    // --- 1. LEDs de Diagnóstico (GPIO31 e GPIO34) ---
    GpioCtrlRegs.GPAGMUX2.bit.GPIO31 = 0;
    GpioCtrlRegs.GPAMUX2.bit.GPIO31 = 0;
    GpioCtrlRegs.GPBGMUX1.bit.GPIO34 = 0;
    GpioCtrlRegs.GPBMUX1.bit.GPIO34 = 0;

    GpioCtrlRegs.GPAPUD.bit.GPIO31 = 1;   // Desabilita Pull-Up
    GpioCtrlRegs.GPBPUD.bit.GPIO34 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO31 = 1;   // Configura como saída
    GpioCtrlRegs.GPBDIR.bit.GPIO34 = 1;

    GpioCtrlRegs.GPACSEL4.bit.GPIO31 = GPIO_MUX_CPU1;
    GpioCtrlRegs.GPBCSEL1.bit.GPIO34 = GPIO_MUX_CPU1;

    // --- 2. Saída PWM1A no Pino GPIO0 (Pino 40 do Conector J4) ---
    GpioCtrlRegs.GPAGMUX1.bit.GPIO0 = 0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1;   // MUX 1 = Função ePWM1A
    GpioCtrlRegs.GPAPUD.bit.GPIO0 = 1;    // Desabilita Pull-Up

    EDIS; // Bloqueia registradores protegidos
}

void Setup_ePWM(void){
    EALLOW;

    CpuSysRegs.PCLKCR2.bit.EPWM1 = 1;      // Habilita o clock para o ePWM1
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 0;  // Bloqueia a contagem do timer

    // --- CONFIGURAÇÃO DO ePWM1A (20kHz | Duty Inicial 34.6%) ---
    EPwm1Regs.TBPRD = 2500;                        // Período de 20kHz
    EPwm1Regs.CMPA.bit.CMPA = 865;                 // CMPA = 865 (Corresponde a 34.6% Duty)

    EPwm1Regs.TBPHS.bit.TBPHS = 0;
    EPwm1Regs.TBCTR = 0x0000;

    EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Contagem simétrica
    EPwm1Regs.TBCTL.bit.PHSEN = TB_DISABLE;
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;
    EPwm1Regs.TBCTL.bit.CLKDIV = TB_DIV1;

    EPwm1Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    EPwm1Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;

    EPwm1Regs.AQCTLA.bit.ZRO = AQ_SET;             // Lógica de acionamento do PWM
    EPwm1Regs.AQCTLA.bit.CAU = AQ_CLEAR;
    EPwm1Regs.AQCTLA.bit.CAD = AQ_SET;
    EPwm1Regs.AQCTLA.bit.PRD = AQ_NO_ACTION;

    // --- SINAL DE TRIGGER PARA O ADC (SOCA) ---
    EPwm1Regs.ETSEL.bit.SOCAEN = 1;                // Habilita o pulso ePWM1SOCA
    EPwm1Regs.ETSEL.bit.SOCASEL = ET_CTR_ZERO;     // Dispara o ADC no zero do ePWM
    EPwm1Regs.ETPS.bit.SOCAPRD = ET_1ST;           // Gera o disparo a cada 1 evento de PWM

    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;  // Libera a contagem dos ePWMs

    EDIS;
}

void Setup_ADC(void){
    EALLOW;

    AdcaRegs.ADCCTL2.bit.PRESCALE = 6;              // Prescaler SYSCLK/4
    AdcSetMode(ADC_ADCA, ADC_RESOLUTION_12BIT, ADC_SIGNALMODE_SINGLE);
    AdcaRegs.ADCCTL1.bit.ADCPWDNZ = 1;              // Ligar o módulo ADC

    DELAY_US(1000);                                 // Estabilização da alimentação do ADC

    // Canal ADCINA0 configurado no SOC0
    AdcaRegs.ADCSOC0CTL.bit.CHSEL = 0;              // ADCINA0 (Pino J3-30)
    AdcaRegs.ADCSOC0CTL.bit.ACQPS = 49;             // Janela de amostragem adequada para R_th = 2.7k
    AdcaRegs.ADCSOC0CTL.bit.TRIGSEL = 5;            // Triga automaticamente via ePWM1SOCA

    // Flags e Interrupção do SOC0
    AdcaRegs.ADCINTSEL1N2.bit.INT1SEL = 0;          // SOC0 aciona INT1
    AdcaRegs.ADCINTSEL1N2.bit.INT1E = 1;            // Habilita flag de conversão concluída
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;

    EDIS;
}
