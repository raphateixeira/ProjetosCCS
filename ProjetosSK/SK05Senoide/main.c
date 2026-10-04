#include "driverlib.h"
#include "device.h"
#include "perifericos.h"
#include "controle.h"

__interrupt void adcA1ISR(void);

void main(void)
{
    Device_init();
    Device_initGPIO();
    Interrupt_initModule();
    Interrupt_initVectorTable();

    perifericos_init();

    controle_inicia();                        // ensaio comeca sozinho apos 2 s
    dac_escreve_volts(controle_valor_inicial());

    Interrupt_register(INT_ADCA1, &adcA1ISR);
    Interrupt_enable(INT_ADCA1);
    EINT;
    ERTM;

    CPUTimer_startTimer(CPUTIMER0_BASE);

    for (;;)
    {
        // tarefas de fundo (UART etc.) entram aqui depois
    }
}

__interrupt void adcA1ISR(void)
{
    float y = adc_le_volts();
    float u = controle_passo(y);
    dac_escreve_volts(u);

    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}
