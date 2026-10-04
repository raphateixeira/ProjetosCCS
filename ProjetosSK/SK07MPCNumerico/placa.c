#include "placa.h"

// DAC-A: saida DACOUTA, referencia VREFHI, atualizacao imediata
static void ConfiguraDAC(void)
{
    DAC_setReferenceVoltage(DACA_BASE, DAC_REF_ADC_VREFHI);
    DAC_setLoadMode(DACA_BASE, DAC_LOAD_SYSCLK);
    DAC_enableOutput(DACA_BASE);
    DAC_setShadowValue(DACA_BASE, 0U);
    DEVICE_DELAY_US(10);
}

// ADC-A: 12 bits; conversao SOC0 do canal ADCINA3 disparada pelo CPU Timer 0;
// o fim da conversao gera a interrupcao ADCA INT1, que chama o laco de amostragem
static void ConfiguraADC(void)
{
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(ADCA_BASE);
    DEVICE_DELAY_US(1000);

    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_CPU1_TINT0,
                 ADC_CH_ADCIN3, 63U);                  // janela de 64 ciclos

    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}

// CPU Timer 0 com periodo Ts: cada estouro (TINT0) dispara uma conversao do ADC
static void ConfiguraTimerAmostragem(float Ts)
{
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0U);
    CPUTimer_setPeriod(CPUTIMER0_BASE, (uint32_t)(DEVICE_SYSCLK_FREQ * Ts) - 1U);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
}

// CPU Timer 1 livre (periodo maximo, 21,5 s por volta): cronometro de ciclos
static void ConfiguraCronometro(void)
{
    CPUTimer_stopTimer(CPUTIMER1_BASE);
    CPUTimer_setPreScaler(CPUTIMER1_BASE, 0U);
    CPUTimer_setPeriod(CPUTIMER1_BASE, 0xFFFFFFFFUL);
    CPUTimer_reloadTimerCounter(CPUTIMER1_BASE);
    CPUTimer_disableInterrupt(CPUTIMER1_BASE);
    CPUTimer_startTimer(CPUTIMER1_BASE);
}

// Ts e o endereco da ISR ficam em MEMORIA durante a configuracao. O Device_init chama
// a calibracao de fabrica da TI (Device_cal, na OTP, via SysCtl_deviceCal), que altera
// o registrador XAR1 sem restaura-lo. Com -O2 o compilador guardava o endereco da ISR
// em XAR1 durante essa chamada: o vetor da interrupcao saia errado (0x70000) e a
// primeira amostra caia em ITRAP (Interrupt_illegalOperationHandler).
static volatile float Ts_;
static void (*volatile isr_)(void);

void ConfiguraF28379D(float Ts, void (*lacoAmostragem)(void))
{
    Ts_  = Ts;
    isr_ = lacoAmostragem;

    Device_init();                    // clock de 200 MHz, perifericos habilitados
    Device_initGPIO();
    Interrupt_initModule();           // controlador de interrupcoes (PIE)
    Interrupt_initVectorTable();

    ConfiguraDAC();
    ConfiguraADC();
    ConfiguraTimerAmostragem(Ts_);
    ConfiguraCronometro();

    Interrupt_register(INT_ADCA1, isr_);
}

void IniciaAmostragem(void)
{
    Interrupt_enable(INT_ADCA1);
    EINT;                             // interrupcoes globais
    ERTM;                             // depuracao em tempo real (WATCH com o programa rodando)
    CPUTimer_startTimer(CPUTIMER0_BASE);
}
