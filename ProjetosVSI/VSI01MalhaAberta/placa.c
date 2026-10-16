#include "placa.h"

#define PINO_HABILITA   124U      // GPIO124: habilita o driver do BOOSTXL

static const uint32_t bracos[3] = {EPWM1_BASE, EPWM2_BASE, EPWM3_BASE};  // A, B, C

static uint16_t periodo;          // TBPRD: o contador vai de 0 a periodo e volta
static float    Vcc;              // tensao do barramento CC [V]

// Pinos dos ePWM 1, 2 e 3 (saidas A e B de cada um) e pino de habilitacao
static void ConfiguraGPIO(void)
{
    GPIO_setPinConfig(GPIO_0_EPWM1A);      // braco A, chave de cima
    GPIO_setPinConfig(GPIO_1_EPWM1B);      // braco A, chave de baixo
    GPIO_setPinConfig(GPIO_2_EPWM2A);      // braco B, chave de cima
    GPIO_setPinConfig(GPIO_3_EPWM2B);      // braco B, chave de baixo
    GPIO_setPinConfig(GPIO_4_EPWM3A);      // braco C, chave de cima
    GPIO_setPinConfig(GPIO_5_EPWM3B);      // braco C, chave de baixo

    GPIO_setPinConfig(GPIO_124_GPIO124);   // habilitacao: comeca desligada
    GPIO_writePin(PINO_HABILITA, 0U);
    GPIO_setDirectionMode(PINO_HABILITA, GPIO_DIR_MODE_OUT);
}

// Um braco: contador sobe e desce (PWM simetrico), saida A em nivel alto
// enquanto contador < CMPA (razao ciclica d = CMPA/periodo); saida B =
// complemento de A; tempo morto de nMorto contagens nas duas transicoes
static void ConfiguraBraco(uint32_t base, uint16_t nMorto)
{
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setTimeBasePeriod(base, periodo);
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_disablePhaseShiftLoad(base);

    // CMPA com sombra: o valor novo so vale no proximo inicio de periodo
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, periodo / 2U);  // d = 1/2

    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);

    // Tempo morto: A atrasada na subida, B = A invertida atrasada na descida
    EPWM_setRisingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setFallingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_FED, true);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_RED, EPWM_DB_POLARITY_ACTIVE_HIGH);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_FED, EPWM_DB_POLARITY_ACTIVE_LOW);
    EPWM_setRisingEdgeDelayCount(base, nMorto);
    EPWM_setFallingEdgeDelayCount(base, nMorto);
}

void ConfiguraF28379D(float fPWM, float tMorto, float vcc, void (*lacoPWM)(void))
{
    uint16_t i;

    Device_init();                    // clock de 200 MHz, perifericos habilitados
    Device_initGPIO();
    Interrupt_initModule();           // controlador de interrupcoes (PIE)
    Interrupt_initVectorTable();

    ConfiguraGPIO();

    Vcc     = vcc;
    periodo = (uint16_t)(EPWMCLK_HZ / (2.0f * fPWM));     // sobe e desce: 2 periodo contagens

    SysCtl_setEPWMClockDivider(SYSCTL_EPWMCLK_DIV_2);     // EPWMCLK = 100 MHz
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC); // contadores parados
    for (i = 0U; i < 3U; i++)
    {
        ConfiguraBraco(bracos[i], (uint16_t)(tMorto * EPWMCLK_HZ + 0.5f));
    }

    // ePWM1 chama o laco do PWM no inicio de cada periodo (contador = 0)
    EPWM_setInterruptSource(EPWM1_BASE, EPWM_INT_TBCTR_ZERO);
    EPWM_setInterruptEventCount(EPWM1_BASE, 1U);
    EPWM_enableInterrupt(EPWM1_BASE);
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);

    Interrupt_register(INT_EPWM1, lacoPWM);
}

void IniciaPWM(void)
{
    GPIO_writePin(PINO_HABILITA, 1U);                     // driver habilitado
    Interrupt_enable(INT_EPWM1);
    EINT;                             // interrupcoes globais
    ERTM;                             // depuracao em tempo real (WATCH com o programa rodando)
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC); // os tres contadores partem juntos
}

void PWM2v(float va, float vb, float vc)
{
    const float v[3] = {va, vb, vc};
    uint16_t i;

    for (i = 0U; i < 3U; i++)
    {
        float d = 0.5f + v[i] / Vcc;                      // razao ciclica do braco
        if (d < 0.0f) d = 0.0f;
        if (d > 1.0f) d = 1.0f;
        EPWM_setCounterCompareValue(bracos[i], EPWM_COUNTER_COMPARE_A,
                                    (uint16_t)(d * (float)periodo));
    }
}
