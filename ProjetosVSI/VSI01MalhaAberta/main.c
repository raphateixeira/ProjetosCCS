// ============================================================================
//  VSI01MalhaAberta - primeiro teste do inversor trifasico (chaveamento)
//
//  Placas: LAUNCHXL-F28379D + BOOSTXL-3PHGANINV (encaixado em J1-J4)
//  LINCE / UFPA
//
//  O programa gera o PWM das tres fases do inversor, sem controle (malha
//  aberta). Serve para ver o chaveamento no osciloscopio:
//
//    modo 0: razao ciclica de 50 % nas tres fases (nenhuma tensao na carga)
//    modo 1: 25 % na fase A, 50 % na B e 75 % na C (para reconhecer as fases)
//    modo 2: razao ciclica variando como senoide, 120 graus entre as fases
//    modo 3: razao ciclica manual, escolhida em razaoManual[0..2] (A, B, C)
//
//  Variaveis para a janela Watch do CCS (pause o programa, mude o valor e
//  continue): modo, habilita, indice, frequencia, razaoManual.
//  Para observar: razaoCiclica, tensaoBarramento.
//
//  Gravacao (etapa B): o BoosterPack entrega as tensoes das fases divididas
//  por 24,7 e filtradas (1 kHz) em J3-24/25/26; o ADC as le a cada periodo.
//  Escreva grava = 1: os vetores tensaoA, tensaoB, tensaoC [V] recebem
//  N_AMOSTRAS amostras (grava vale 2 durante a gravacao e volta a 0 no fim).
//  Depois, pause e exporte os vetores da janela Watch para CSV.
//
//  Etapa A (so a LaunchPad): medir os pinos J4-40 a J4-35.
//  Etapa B (com o BoosterPack, barramento de 12 V): habilita = 1 liga o
//  inversor; habilita = 0 desliga.
// ============================================================================

#include "driverlib.h"
#include "device.h"
#include <math.h>

// ---------------------------- Parametros ------------------------------------
#define FREQ_RELOGIO_PWM   100e6f    // relogio dos modulos de PWM: 200 MHz / 2 = 100 MHz
#define FREQ_PWM           20e3f     // frequencia de chaveamento: 20 kHz (periodo de 50 us)
#define TEMPO_MORTO        100e-9f   // tempo morto entre as chaves de um braco: 100 ns
#define RAZAO_MINIMA       0.05f     // limites da razao ciclica: evitam pulsos mais
#define RAZAO_MAXIMA       0.95f     // curtos que o tempo morto
#define PINO_HABILITA      124U      // GPIO124 (J2-13): /PWM enable do BOOSTXL, ativo em nivel BAIXO

// O contador do PWM sobe de 0 ate TOPO e desce de volta a 0: um periodo de
// PWM tem 2 x TOPO contagens de 10 ns. TOPO = 100 MHz / (2 x 20 kHz) = 2500.
#define TOPO               ((uint16_t)(FREQ_RELOGIO_PWM / (2.0f * FREQ_PWM)))
#define CONTAGENS_MORTO    ((uint16_t)(TEMPO_MORTO * FREQ_RELOGIO_PWM + 0.5f))   // 10
#define PERIODO_AMOSTRA    (1.0f / FREQ_PWM)                                     // 50 us
#define DOIS_PI            6.283185307f

// Medida das tensoes: ADC de 12 bits com referencia de 3,0 V; divisor de
// 100 kOhm / 4,22 kOhm no BoosterPack (tensao real = 24,7 x tensao no pino)
#define N_AMOSTRAS         2000U     // amostras por fase: 2000 x 50 us = 100 ms (6 ciclos de 60 Hz)
#define DECIMACAO          1U        // grava 1 amostra a cada DECIMACAO periodos de PWM
#define VOLTS_POR_CONTAGEM (3.0f / 4096.0f * 24.7f)                              // 18,1 mV

// --------------------- Variaveis para a janela Watch ------------------------
volatile uint16_t modo       = 0;       // 0: 50 % | 1: 25/50/75 % | 2: senoidal | 3: manual
volatile uint16_t habilita   = 0;       // 1 = liga o inversor (etapa B)
volatile float    indice     = 0.5f;    // indice de modulacao do modo 2 (0 a 1)
volatile float    frequencia = 60.0f;   // frequencia da senoide do modo 2 [Hz]
volatile float    razaoManual[3] = {0.5f, 0.5f, 0.5f};   // modo 3: A, B, C (0,05 a 0,95)

volatile uint16_t grava      = 0;       // 1 = comecar a gravar | 2 = gravando | 0 = parado

volatile float    razaoCiclica[3];      // razoes ciclicas das fases A, B, C (so leitura)
volatile float    tensaoBarramento;     // tensao do barramento agora [V] (so leitura)
volatile uint16_t amostra    = 0;       // amostras ja gravadas (so leitura)
static float      angulo = 0.0f;        // angulo da senoide [rad]
static uint16_t   contaDecimacao = 0;

// Vetores gravados [V], um por bloco de RAM (secoes "ramgs0..2" do .cmd)
#pragma DATA_SECTION(tensaoA, "ramgs0")
#pragma DATA_SECTION(tensaoB, "ramgs1")
#pragma DATA_SECTION(tensaoC, "ramgs2")
float tensaoA[N_AMOSTRAS];
float tensaoB[N_AMOSTRAS];
float tensaoC[N_AMOSTRAS];

static const uint32_t modulo[3] = {EPWM1_BASE, EPWM2_BASE, EPWM3_BASE};   // fases A, B, C

__interrupt void AtualizaPWM(void);
static void ConfiguraPinos(void);
static void ConfiguraModuloPWM(uint32_t base);
static void EscreveRazao(uint32_t base, float razao);
static void ConfiguraADC(void);
static void LeTensoes(void);

// ============================================================================
void main(void)
{
    uint16_t fase;

    Device_init();                  // relogio de 200 MHz e perifericos ligados
    Device_initGPIO();
    Interrupt_initModule();         // controlador de interrupcoes
    Interrupt_initVectorTable();

    ConfiguraPinos();               // inversor desligado desde o inicio

    // Os tres modulos sao configurados com os contadores parados e partem
    // juntos quando o relogio comum (TBCLKSYNC) e liberado: ficam sincronizados.
    SysCtl_setEPWMClockDivider(SYSCTL_EPWMCLK_DIV_2);           // 100 MHz
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    for (fase = 0U; fase < 3U; fase++)
    {
        ConfiguraModuloPWM(modulo[fase]);
    }
    ConfiguraADC();                 // conversoes disparadas pelo PWM da fase A

    // Interrupcao no inicio de cada periodo do PWM da fase A (contador = 0):
    // calcula as razoes ciclicas do periodo seguinte.
    EPWM_setInterruptSource(EPWM1_BASE, EPWM_INT_TBCTR_ZERO);
    EPWM_setInterruptEventCount(EPWM1_BASE, 1U);
    EPWM_enableInterrupt(EPWM1_BASE);
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_register(INT_EPWM1, &AtualizaPWM);
    Interrupt_enable(INT_EPWM1);

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);       // os tres contadores partem

    EINT;                           // libera as interrupcoes
    ERTM;                           // modo de depuracao em tempo real

    for (;;)
    {
        // Habilitacao do inversor (ativa em nivel baixo): 0 liga, 1 desliga
        GPIO_writePin(PINO_HABILITA, (habilita == 1U) ? 0U : 1U);
    }
}

// ============================================================================
// Chamada a cada 50 us, no inicio do periodo do PWM
__interrupt void AtualizaPWM(void)
{
    uint16_t fase;

    switch (modo)
    {
    case 1:                         // uma razao ciclica diferente em cada fase
        razaoCiclica[0] = 0.25f;
        razaoCiclica[1] = 0.50f;
        razaoCiclica[2] = 0.75f;
        break;

    case 2:                         // senoides defasadas de 120 graus
        angulo += DOIS_PI * frequencia * PERIODO_AMOSTRA;
        if (angulo >= DOIS_PI)
        {
            angulo -= DOIS_PI;
        }
        razaoCiclica[0] = 0.5f + 0.5f * indice * sinf(angulo);
        razaoCiclica[1] = 0.5f + 0.5f * indice * sinf(angulo - DOIS_PI / 3.0f);
        razaoCiclica[2] = 0.5f + 0.5f * indice * sinf(angulo + DOIS_PI / 3.0f);
        break;

    case 3:                         // razao ciclica escolhida na janela Watch
        razaoCiclica[0] = razaoManual[0];
        razaoCiclica[1] = razaoManual[1];
        razaoCiclica[2] = razaoManual[2];
        break;

    default:                       // modo 0: 50 % nas tres fases
        razaoCiclica[0] = 0.5f;
        razaoCiclica[1] = 0.5f;
        razaoCiclica[2] = 0.5f;
        break;
    }

    for (fase = 0U; fase < 3U; fase++)
    {
        EscreveRazao(modulo[fase], razaoCiclica[fase]);
    }

    LeTensoes();                    // le o ADC e grava, se pedido

    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

// ============================================================================
// Razao ciclica -> valor de comparacao (CMPA). A saida A fica em nivel alto
// enquanto o contador esta abaixo de CMPA: razao = CMPA / TOPO.
static void EscreveRazao(uint32_t base, float razao)
{
    if (razao < RAZAO_MINIMA) razao = RAZAO_MINIMA;
    if (razao > RAZAO_MAXIMA) razao = RAZAO_MAXIMA;
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A,
                                (uint16_t)(razao * (float)TOPO));
}

// ============================================================================
// Le as tensoes das fases e do barramento e grava nos vetores.
// As conversoes comecam quando o contador da fase A passa por zero, no mesmo
// instante desta interrupcao; espera-se o fim delas (cerca de 1 us).
static void LeTensoes(void)
{
    uint16_t espera = 0U;

    while ((ADC_getInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1) == false) && (espera < 1000U))
    {
        espera++;                   // limite: nunca trava a interrupcao
    }
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    if (ADC_getInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1))
    {
        ADC_clearInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1);   // marca perdida (pausa)
    }

    tensaoBarramento = VOLTS_POR_CONTAGEM * (float)ADC_readResult(ADCDRESULT_BASE, ADC_SOC_NUMBER0);

    if (grava == 1U)                // pedido de gravacao: comeca do zero
    {
        amostra = 0U;
        contaDecimacao = 0U;
        grava = 2U;
    }
    if (grava == 2U)
    {
        if (++contaDecimacao >= DECIMACAO)
        {
            contaDecimacao = 0U;
            tensaoA[amostra] = VOLTS_POR_CONTAGEM * (float)ADC_readResult(ADCCRESULT_BASE, ADC_SOC_NUMBER0);
            tensaoB[amostra] = VOLTS_POR_CONTAGEM * (float)ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER0);
            tensaoC[amostra] = VOLTS_POR_CONTAGEM * (float)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
            amostra++;
            if (amostra >= N_AMOSTRAS)
            {
                grava = 0U;         // vetores completos
            }
        }
    }
}

// ============================================================================
// ADC: tensao da fase A no ADCINC3 (J3-24), B no ADCINB3 (J3-25), C no ADCINA3
// (J3-26) e barramento no ADCIN14 (J3-23, lido pela ADCD). As quatro
// conversoes sao disparadas juntas pelo PWM da fase A, no zero do contador.
static void ConfiguraADC(void)
{
    const uint32_t adc[4] = {ADCA_BASE, ADCB_BASE, ADCC_BASE, ADCD_BASE};
    uint16_t k;

    for (k = 0U; k < 4U; k++)
    {
        ADC_setPrescaler(adc[k], ADC_CLK_DIV_4_0);                       // 50 MHz
        ADC_setMode(adc[k], ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
        ADC_setInterruptPulseMode(adc[k], ADC_PULSE_END_OF_CONV);
        ADC_enableConverter(adc[k]);
    }
    DEVICE_DELAY_US(1000U);         // espera os ADCs ligarem

    // janela de amostragem: 20 ciclos de 5 ns = 100 ns
    ADC_setupSOC(ADCC_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN3, 20U);  // fase A
    ADC_setupSOC(ADCB_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN3, 20U);  // fase B
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN3, 20U);  // fase C
    ADC_setupSOC(ADCD_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN14, 20U); // barramento

    // fim da conversao da ADCA marca "medidas prontas"
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);
    ADC_enableContinuousMode(ADCA_BASE, ADC_INT_NUMBER1);   // marca o fim de toda conversao,
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);        // mesmo sem a marca anterior limpa
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);

    // o PWM da fase A dispara as conversoes quando o contador passa por zero
    EPWM_setADCTriggerSource(EPWM1_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_ZERO);
    EPWM_setADCTriggerEventPrescale(EPWM1_BASE, EPWM_SOC_A, 1U);
    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
}

// ============================================================================
// Um modulo de PWM = um braco do inversor. Saida A comanda a chave de cima,
// saida B a de baixo (complemento de A, com tempo morto).
static void ConfiguraModuloPWM(uint32_t base)
{
    // Contador: sobe e desce entre 0 e TOPO, com o relogio de 100 MHz
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setTimeBasePeriod(base, TOPO);
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_disablePhaseShiftLoad(base);

    // Valor de comparacao com "sombra": o valor novo so vale quando o
    // contador passa por zero (nao muda no meio de um pulso)
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, TOPO / 2U);

    // Saida A: desce quando o contador, subindo, alcanca CMPA; sobe quando o
    // contador, descendo, passa por CMPA -> alta enquanto contador < CMPA
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);

    // Tempo morto: A liga atrasada; B = A invertida, tambem liga atrasada.
    // Assim as duas chaves nunca ficam ligadas ao mesmo tempo.
    EPWM_setRisingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setFallingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_RED, EPWM_DB_POLARITY_ACTIVE_HIGH);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_FED, EPWM_DB_POLARITY_ACTIVE_LOW);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_FED, true);
    EPWM_setRisingEdgeDelayCount(base, CONTAGENS_MORTO);
    EPWM_setFallingEdgeDelayCount(base, CONTAGENS_MORTO);

    // Seguranca: quando o depurador pausa o programa, o contador congela e a
    // saida ficaria parada (uma chave ligada direto). O sinal de parada do
    // emulador (TZ6) forca A e B em nivel baixo: as duas chaves desligam.
    EPWM_enableTripZoneSignals(base, EPWM_TZ_SIGNAL_CBC6);
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZA, EPWM_TZ_ACTION_LOW);
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_LOW);
}

// ============================================================================
static void ConfiguraPinos(void)
{
    // Habilitacao primeiro: comeca em 1 (inversor desligado)
    GPIO_writePin(PINO_HABILITA, 1U);
    GPIO_setPadConfig(PINO_HABILITA, GPIO_PIN_TYPE_PULLUP);
    GPIO_setPinConfig(GPIO_124_GPIO124);
    GPIO_setDirectionMode(PINO_HABILITA, GPIO_DIR_MODE_OUT);

    // Saidas de PWM (J4-40 a J4-35)
    GPIO_setPadConfig(0U, GPIO_PIN_TYPE_STD);  GPIO_setPinConfig(GPIO_0_EPWM1A);  // A, chave de cima
    GPIO_setPadConfig(1U, GPIO_PIN_TYPE_STD);  GPIO_setPinConfig(GPIO_1_EPWM1B);  // A, chave de baixo
    GPIO_setPadConfig(2U, GPIO_PIN_TYPE_STD);  GPIO_setPinConfig(GPIO_2_EPWM2A);  // B, chave de cima
    GPIO_setPadConfig(3U, GPIO_PIN_TYPE_STD);  GPIO_setPinConfig(GPIO_3_EPWM2B);  // B, chave de baixo
    GPIO_setPadConfig(4U, GPIO_PIN_TYPE_STD);  GPIO_setPinConfig(GPIO_4_EPWM3A);  // C, chave de cima
    GPIO_setPadConfig(5U, GPIO_PIN_TYPE_STD);  GPIO_setPinConfig(GPIO_5_EPWM3B);  // C, chave de baixo
}
