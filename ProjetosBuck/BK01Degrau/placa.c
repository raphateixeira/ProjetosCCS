#include "placa.h"

static uint16_t periodo;          // TBPRD: o contador vai de 0 a periodo e volta
static float    Vin;              // tensao de entrada [V]
static float    dMax;             // maior razao ciclica (uMax/Vin)
static float    kADC;             // contagens do ADC -> y [V]

// Pino do ePWM1 (so a saida A: uma chave, com diodo de roda livre)
static void ConfiguraGPIO(void)
{
    GPIO_setPinConfig(GPIO_0_EPWM1A);      // chave do buck
}

// ePWM1: contador sobe e desce (PWM simetrico), saida A em nivel alto enquanto
// contador < CMPA (razao ciclica d = CMPA/periodo); a cada nPWM periodos, no
// contador = 0, dispara a conversao do ADC (SOCA)
static void ConfiguraPWM(float fPWM, float Ts)
{
    uint16_t nPWM = (uint16_t)(Ts * fPWM + 0.5f);       // periodos do PWM por amostra
    if (nPWM < 1U)  nPWM = 1U;
    if (nPWM > 15U) nPWM = 15U;

    periodo = (uint16_t)(EPWMCLK_HZ / (2.0f * fPWM));   // sobe e desce: 2 periodo contagens

    SysCtl_setEPWMClockDivider(SYSCTL_EPWMCLK_DIV_2);   // EPWMCLK = 100 MHz
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC); // contador parado

    EPWM_setClockPrescaler(EPWM1_BASE, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setTimeBasePeriod(EPWM1_BASE, periodo);
    EPWM_setTimeBaseCounter(EPWM1_BASE, 0U);
    EPWM_disablePhaseShiftLoad(EPWM1_BASE);

    // CMPA com sombra: o valor novo so vale no proximo inicio de periodo
    EPWM_setCounterCompareShadowLoadMode(EPWM1_BASE, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, 0U);  // d = 0

    EPWM_setActionQualifierAction(EPWM1_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(EPWM1_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);

    // SOCA no contador = 0, a cada nPWM periodos: dispara o ADC (amostragem Ts)
    EPWM_setADCTriggerSource(EPWM1_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_ZERO);
    EPWM_setADCTriggerEventPrescale(EPWM1_BASE, EPWM_SOC_A, nPWM);
    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
}

// ADC-A: 12 bits; conversao SOC0 do canal ADCINA3 disparada pelo ePWM1 (SOCA);
// o fim da conversao gera a interrupcao ADCA INT1, que chama o laco de amostragem
static void ConfiguraADC(void)
{
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(ADCA_BASE);
    DEVICE_DELAY_US(1000);

    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA,
                 ADC_CH_ADCIN3, 63U);                  // janela de 64 ciclos

    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}

void ConfiguraF28379D(const ParametrosConversor *c, void (*lacoAmostragem)(void))
{
    Device_init();                    // clock de 200 MHz, perifericos habilitados
    Device_initGPIO();
    Interrupt_initModule();           // controlador de interrupcoes (PIE)
    Interrupt_initVectorTable();

    Vin  = c->Vin;
    dMax = c->uMax / c->Vin;
    if (dMax > 1.0f) dMax = 1.0f;
    kADC = VREF / (FUNDO_ESC * c->kSensor);

    ConfiguraGPIO();
    ConfiguraPWM(c->fPWM, c->Ts);
    ConfiguraADC();

    Interrupt_register(INT_ADCA1, lacoAmostragem);
}

void IniciaAmostragem(void)
{
    Interrupt_enable(INT_ADCA1);
    EINT;                             // interrupcoes globais
    ERTM;                             // depuracao em tempo real (WATCH com o programa rodando)
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC); // contador do ePWM1 parte
}

void PWM2u(float u)
{
    float d = u / Vin;                                  // razao ciclica
    if (d < 0.0f) d = 0.0f;
    if (d > dMax) d = dMax;
    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A,
                                (uint16_t)(d * (float)periodo));
}

float ADC2y(void)
{
    return (float)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0) * kADC;
}
