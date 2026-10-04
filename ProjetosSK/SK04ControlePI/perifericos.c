#include "perifericos.h"

static void dac_init(void)
{
    DAC_setReferenceVoltage(DACA_BASE, DAC_REF_ADC_VREFHI);
    DAC_setLoadMode(DACA_BASE, DAC_LOAD_SYSCLK);
    DAC_enableOutput(DACA_BASE);
    DAC_setShadowValue(DACA_BASE, 0U);
    DEVICE_DELAY_US(10);
}

static void adc_init(void)
{
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(ADCA_BASE);
    DEVICE_DELAY_US(1000);

    // SOC0: canal ADCINA3, disparado pelo CPU Timer 0, janela de 64 ciclos
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_CPU1_TINT0,
                 ADC_CH_ADCIN3, 63U);

    // Fim da conversao do SOC0 gera ADCA INT1
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}

static void timer_init(void)
{
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0U);
    CPUTimer_setPeriod(CPUTIMER0_BASE, (uint32_t)(DEVICE_SYSCLK_FREQ * TS_S) - 1U);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);   // gera o TINT0 que dispara o ADC
}

void perifericos_init(void)
{
    dac_init();
    adc_init();
    timer_init();
}
