#include "hal.h"

#define PINO_HABILITA   124U      // /PWM enable do BOOSTXL: ativo em nivel baixo
#define PINO_OT         24U       // /PCB OT alert: ativo em nivel baixo
#define JANELA_ADC      20U       // janela de aquisicao: 20 SYSCLK = 100 ns (minimo 75 ns em 12 bits)

static const uint32_t bracos[3] = {EPWM1_BASE, EPWM2_BASE, EPWM3_BASE};     // A, B, C
static const uint32_t comparadores[3] = {CMPSS6_BASE, CMPSS3_BASE, CMPSS1_BASE}; // iA, iB, iC

static uint16_t periodo;          // TBPRD: o contador vai de 0 a periodo e volta

// Conversao das contagens do ADC
static const float kI = SINAL_CORRENTE * VREF_ADC / (4096.0f * K_INA240);   // A por contagem (7,32 mA)
static const float kV = VREF_ADC * K_DIVISOR / 4096.0f;                      // V por contagem (18,1 mV)

// Offsets (contagens): valor nominal ate a calibracao
float offsetA = N_MEIO_NOM, offsetB = N_MEIO_NOM, offsetC = N_MEIO_NOM;

static uint32_t somaA, somaB, somaC;
static uint16_t nSoma, nCal;

// ---------------------------------------------------------------------------
// GPIO: habilitacao primeiro (desligada), depois os pinos de PWM e o alerta
static void ConfiguraGPIO(void)
{
    GPIO_writePin(PINO_HABILITA, 1U);                    // 1 = buffer desligado
    GPIO_setPadConfig(PINO_HABILITA, GPIO_PIN_TYPE_PULLUP);
    GPIO_setPinConfig(GPIO_124_GPIO124);
    GPIO_setDirectionMode(PINO_HABILITA, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_0_EPWM1A);      // braco A, chave de cima
    GPIO_setPinConfig(GPIO_1_EPWM1B);      // braco A, chave de baixo
    GPIO_setPinConfig(GPIO_2_EPWM2A);      // braco B
    GPIO_setPinConfig(GPIO_3_EPWM2B);
    GPIO_setPinConfig(GPIO_4_EPWM3A);      // braco C
    GPIO_setPinConfig(GPIO_5_EPWM3B);

    GPIO_setPinConfig(GPIO_24_GPIO24);     // alerta de temperatura (dreno aberto, pull-up de 10k na placa)
    GPIO_setDirectionMode(PINO_OT, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(PINO_OT, GPIO_PIN_TYPE_INVERT);    // invertido: nivel alto no XBAR = alerta
    GPIO_setQualificationMode(PINO_OT, GPIO_QUAL_6SAMPLE);
}

// ---------------------------------------------------------------------------
// Um braco: PWM simetrico com tempo morto e trip por DCAEVT1 (TRIP4)
static void ConfiguraBraco(uint32_t base, uint16_t nMorto)
{
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setTimeBasePeriod(base, periodo);
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_disablePhaseShiftLoad(base);      // os tres partem juntos pelo TBCLKSYNC

    // CMPA com sombra: o valor novo so vale no proximo zero do contador
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, periodo / 2U);

    // A: alto enquanto contador < CMPA (d = CMPA/periodo)
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

    // Trip: TRIP4 (OU dos comparadores e do alerta) -> DCAH -> DCAEVT1 -> OST
    EPWM_selectDigitalCompareTripInput(base, EPWM_DC_TRIP_TRIPIN4, EPWM_DC_TYPE_DCAH);
    EPWM_setTripZoneDigitalCompareEventCondition(base, EPWM_TZ_DC_OUTPUT_A1,
                                                 EPWM_TZ_EVENT_DCXH_HIGH);
    EPWM_setDigitalCompareEventSource(base, EPWM_DC_MODULE_A, EPWM_DC_EVENT_1,
                                      EPWM_DC_EVENT_SOURCE_ORIG_SIGNAL);
    EPWM_setDigitalCompareEventSyncMode(base, EPWM_DC_MODULE_A, EPWM_DC_EVENT_1,
                                        EPWM_DC_EVENT_INPUT_NOT_SYNCED);
    EPWM_enableTripZoneSignals(base, EPWM_TZ_SIGNAL_DCAEVT1);   // DCAEVT1 como fonte de OST
    EPWM_enableTripZoneSignals(base, EPWM_TZ_SIGNAL_CBC6);      // parada do emulador
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZA, EPWM_TZ_ACTION_LOW);
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_LOW);
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_DCAEVT1, EPWM_TZ_ACTION_LOW);

    EPWM_forceTripZoneEvent(base, EPWM_TZ_FORCE_EVENT_OST);    // comeca desligado
}

// ---------------------------------------------------------------------------
// ADC: 12 bits, ADCCLK = 50 MHz; SOC0 = corrente, SOC1 = tensao de fase,
// disparados juntos pelo SOCA do EPWM1; fim do SOC1 da ADCA -> INT1
static void ConfiguraADC(uint16_t nControle)
{
    const uint32_t adcs[4] = {ADCA_BASE, ADCB_BASE, ADCC_BASE, ADCD_BASE};
    uint16_t i;

    for (i = 0U; i < 4U; i++)
    {
        ADC_setPrescaler(adcs[i], ADC_CLK_DIV_4_0);
        ADC_setMode(adcs[i], ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
        ADC_setInterruptPulseMode(adcs[i], ADC_PULSE_END_OF_CONV);
        ADC_enableConverter(adcs[i]);
    }
    DEVICE_DELAY_US(1000U);                // ADCs ligando

    ADC_setupSOC(ADCC_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN2, JANELA_ADC);  // iA
    ADC_setupSOC(ADCB_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN2, JANELA_ADC);  // iB
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN2, JANELA_ADC);  // iC
    ADC_setupSOC(ADCC_BASE, ADC_SOC_NUMBER1, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN3, JANELA_ADC);  // vA
    ADC_setupSOC(ADCB_BASE, ADC_SOC_NUMBER1, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN3, JANELA_ADC);  // vB
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER1, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN3, JANELA_ADC);  // vC
    ADC_setupSOC(ADCD_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN14, JANELA_ADC); // Vdc

    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER1);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);

    // SOCA no zero do contador do EPWM1, a cada nControle periodos
    EPWM_setADCTriggerSource(EPWM1_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_ZERO);
    EPWM_setADCTriggerEventPrescale(EPWM1_BASE, EPWM_SOC_A, nControle);
    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
}

// ---------------------------------------------------------------------------
// Comparadores: H dispara com corrente acima do limite, L (invertido) abaixo
static void ConfiguraCMPSS(void)
{
    uint16_t i;

    for (i = 0U; i < 3U; i++)
    {
        uint32_t b = comparadores[i];
        CMPSS_configHighComparator(b, CMPSS_INSRC_DAC);
        CMPSS_configLowComparator(b, CMPSS_INSRC_DAC | CMPSS_INV_INVERTED);
        CMPSS_configOutputsHigh(b, CMPSS_TRIP_FILTER | CMPSS_TRIPOUT_FILTER);
        CMPSS_configOutputsLow(b, CMPSS_TRIP_FILTER | CMPSS_TRIPOUT_FILTER);
        CMPSS_setHysteresis(b, 2U);
        CMPSS_configDAC(b, CMPSS_DACREF_VDDA | CMPSS_DACVAL_SYSCLK | CMPSS_DACSRC_SHDW);
        // Filtro digital (como no SDK): amostra a cada 20 ciclos, 18 de 30 amostras -> ~3 us
        CMPSS_configFilterHigh(b, 20U, 30U, 18U);
        CMPSS_initFilterHigh(b);
        CMPSS_configFilterLow(b, 20U, 30U, 18U);
        CMPSS_initFilterLow(b);
        CMPSS_enableModule(b);
    }

    // XBAR: TRIP4 = CMPSS1 OU CMPSS3 OU CMPSS6 (H ou L) [OU alerta de temperatura]
    XBAR_setInputPin(XBAR_INPUT1, PINO_OT);
    XBAR_setEPWMMuxConfig(XBAR_TRIP4, XBAR_EPWM_MUX00_CMPSS1_CTRIPH_OR_L);
    XBAR_setEPWMMuxConfig(XBAR_TRIP4, XBAR_EPWM_MUX04_CMPSS3_CTRIPH_OR_L);
    XBAR_setEPWMMuxConfig(XBAR_TRIP4, XBAR_EPWM_MUX10_CMPSS6_CTRIPH_OR_L);
    XBAR_disableEPWMMux(XBAR_TRIP4, 0xFFFFFFFFU);
#if OT_NO_TRIP
    XBAR_setEPWMMuxConfig(XBAR_TRIP4, XBAR_EPWM_MUX01_INPUTXBAR1);
    XBAR_enableEPWMMux(XBAR_TRIP4, XBAR_MUX00 | XBAR_MUX04 | XBAR_MUX10 | XBAR_MUX01);
#else
    XBAR_enableEPWMMux(XBAR_TRIP4, XBAR_MUX00 | XBAR_MUX04 | XBAR_MUX10);
#endif
}

// ---------------------------------------------------------------------------
void HAL_Configura(const ParametrosHAL *p, void (*lacoControle)(void))
{
    // Copias volateis antes do Device_init: o Device_cal (chamado por ele)
    // altera XAR1 sem restaurar, o que corrompe argumentos com otimizacao (-O2)
    volatile float    fPWM = p->fPWM, tMorto = p->tMorto, iLimite = p->iLimite;
    volatile uint16_t nControle = p->nControle;
    void (* volatile laco)(void) = lacoControle;
    uint16_t i;

    Device_init();                    // clock de 200 MHz, perifericos habilitados
    Device_initGPIO();
    Interrupt_initModule();           // controlador de interrupcoes (PIE)
    Interrupt_initVectorTable();

    ConfiguraGPIO();                  // habilitacao em 1 antes de tudo

    periodo = (uint16_t)(EPWMCLK_HZ / (2.0f * fPWM));         // 2500 para 20 kHz

    SysCtl_setEPWMClockDivider(SYSCTL_EPWMCLK_DIV_2);         // EPWMCLK = 100 MHz
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);    // contadores parados
    for (i = 0U; i < 3U; i++)
    {
        ConfiguraBraco(bracos[i], (uint16_t)(tMorto * EPWMCLK_HZ + 0.5f));
    }

    ConfiguraCMPSS();                 // protecao pronta antes de ligar os contadores
    HAL_LimiteCorrente(iLimite);
    ConfiguraADC(nControle);

    Interrupt_register(INT_ADCA1, laco);
}

void HAL_IniciaContadores(void)
{
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);     // os tres partem juntos
    HAL_LimpaTrip();                  // limpa os latches do arranque (o OST forcado continua)
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    Interrupt_enable(INT_ADCA1);
    EINT;                             // interrupcoes globais
    ERTM;                             // depuracao em tempo real (Expressions com o programa rodando)
}

// ---------------------------------------------------------------------------
static uint16_t nA, nB, nC;           // ultimas contagens das correntes (para a calibracao)

void HAL_Le(Medidas *m)
{
    nA = ADC_readResult(ADCCRESULT_BASE, ADC_SOC_NUMBER0);
    nB = ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER0);
    nC = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);

    m->ia = kI * ((float)nA - offsetA);
    m->ib = kI * ((float)nB - offsetB);
    m->ic = kI * ((float)nC - offsetC);
    m->va = kV * (float)ADC_readResult(ADCCRESULT_BASE, ADC_SOC_NUMBER1);
    m->vb = kV * (float)ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER1);
    m->vc = kV * (float)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1);
    m->Vdc = kV * (float)ADC_readResult(ADCDRESULT_BASE, ADC_SOC_NUMBER0);
}

void HAL_PedeCalibracao(uint16_t nAmostras)
{
    somaA = somaB = somaC = 0UL;
    nSoma = 0U;
    nCal  = nAmostras;
}

uint16_t HAL_AcumulaOffset(void)
{
    somaA += nA;
    somaB += nB;
    somaC += nC;
    nSoma++;
    if (nSoma < nCal)
    {
        return 0U;
    }
    offsetA = (float)somaA / (float)nSoma;
    offsetB = (float)somaB / (float)nSoma;
    offsetC = (float)somaC / (float)nSoma;
    return 1U;
}

uint16_t HAL_OffsetsValidos(void)
{
    // 1,65 V +- 0,15 V: sem barramento o INA240 fica sem alimentacao e le ~0
    const float nMin = 1.50f * 4096.0f / VREF_ADC, nMax = 1.80f * 4096.0f / VREF_ADC;
    return (offsetA > nMin && offsetA < nMax && offsetB > nMin && offsetB < nMax &&
            offsetC > nMin && offsetC < nMax) ? 1U : 0U;
}

// DAC do CMPSS (referencia VDDA = 3,3 V): N = 4096/3,3 (vMeio +- 0,1 iLim),
// com vMeio = offset medido de cada fase (em volts)
void HAL_LimiteCorrente(float iLim)
{
    const float offsets[3] = {offsetA, offsetB, offsetC};
    uint16_t i;

    for (i = 0U; i < 3U; i++)
    {
        float vMeio = offsets[i] * VREF_ADC / 4096.0f;
        float nH = (vMeio + K_INA240 * iLim) * 4096.0f / VDDA_CMPSS;
        float nL = (vMeio - K_INA240 * iLim) * 4096.0f / VDDA_CMPSS;
        if (nH > 4095.0f) nH = 4095.0f;
        if (nL < 0.0f)    nL = 0.0f;
        CMPSS_setDACValueHigh(comparadores[i], (uint16_t)nH);
        CMPSS_setDACValueLow(comparadores[i], (uint16_t)nL);
    }
}

void HAL_AplicaRazoes(const float d[3])
{
    uint16_t i;

    for (i = 0U; i < 3U; i++)
    {
        float di = d[i];
        if (di < 0.0f) di = 0.0f;
        if (di > 1.0f) di = 1.0f;
        EPWM_setCounterCompareValue(bracos[i], EPWM_COUNTER_COMPARE_A,
                                    (uint16_t)(di * (float)periodo));
    }
}

// ---------------------------------------------------------------------------
void HAL_LimpaTrip(void)
{
    uint16_t i;

    // Latches dos comparadores e flags do DCAEVT1; o OST continua (saidas em
    // nivel baixo) ate HAL_HabilitaPWM. Se a causa persistir, a flag volta.
    for (i = 0U; i < 3U; i++)
    {
        CMPSS_clearFilterLatchHigh(comparadores[i]);
        CMPSS_clearFilterLatchLow(comparadores[i]);
        EPWM_clearOneShotTripZoneFlag(bracos[i], EPWM_TZ_OST_FLAG_DCAEVT1);
        EPWM_clearTripZoneFlag(bracos[i], EPWM_TZ_INTERRUPT | EPWM_TZ_FLAG_DCAEVT1);
    }
}

void HAL_HabilitaPWM(void)
{
    uint16_t i;

    HAL_LimpaTrip();
    for (i = 0U; i < 3U; i++)
    {
        EPWM_clearTripZoneFlag(bracos[i], EPWM_TZ_FLAG_OST);   // libera as saidas
    }
    GPIO_writePin(PINO_HABILITA, 0U); // 0 = buffer ligado
}

void HAL_DesabilitaPWM(void)
{
    uint16_t i;

    for (i = 0U; i < 3U; i++)
    {
        EPWM_forceTripZoneEvent(bracos[i], EPWM_TZ_FORCE_EVENT_OST);  // saidas em nivel baixo
    }
    GPIO_writePin(PINO_HABILITA, 1U); // 1 = buffer desligado
}

uint16_t HAL_Trip(void)
{
    return ((EPWM_getTripZoneFlagStatus(EPWM1_BASE) |
             EPWM_getTripZoneFlagStatus(EPWM2_BASE) |
             EPWM_getTripZoneFlagStatus(EPWM3_BASE)) & EPWM_TZ_FLAG_DCAEVT1) ? 1U : 0U;
}

uint16_t HAL_OrigemTrip(void)
{
    const uint16_t latch = CMPSS_STS_HI_LATCHFILTOUT | CMPSS_STS_LO_LATCHFILTOUT;
    uint16_t o = 0U, i;

    for (i = 0U; i < 3U; i++)
    {
        if (CMPSS_getStatus(comparadores[i]) & latch)
        {
            o |= (uint16_t)(1U << i);
        }
    }
    if (HAL_SobreTemperatura())
    {
        o |= 8U;
    }
    return o;
}

// ---------------------------------------------------------------------------
void HAL_ConfiguraSCI(uint32_t baud)
{
    GPIO_setPinConfig(DEVICE_GPIO_CFG_SCIRXDA);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_SCIRXDA, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_SCIRXDA, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(DEVICE_GPIO_PIN_SCIRXDA, GPIO_QUAL_ASYNC);

    GPIO_setPinConfig(DEVICE_GPIO_CFG_SCITXDA);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_SCITXDA, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_SCITXDA, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(DEVICE_GPIO_PIN_SCITXDA, GPIO_QUAL_ASYNC);

    SCI_performSoftwareReset(SCIA_BASE);
    SCI_setConfig(SCIA_BASE, DEVICE_LSPCLK_FREQ, baud,
                  SCI_CONFIG_WLEN_8 | SCI_CONFIG_STOP_ONE | SCI_CONFIG_PAR_NONE);
    SCI_resetChannels(SCIA_BASE);
    SCI_resetRxFIFO(SCIA_BASE);
    SCI_resetTxFIFO(SCIA_BASE);
    SCI_clearInterruptStatus(SCIA_BASE, SCI_INT_TXFF | SCI_INT_RXFF);
    SCI_enableFIFO(SCIA_BASE);
    SCI_enableModule(SCIA_BASE);
    SCI_performSoftwareReset(SCIA_BASE);
}
