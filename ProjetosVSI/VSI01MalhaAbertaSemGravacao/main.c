// ============================================================================
//  VSI01MalhaAberta - primeiro teste do inversor trifasico (chaveamento)
//
//  Placas: LAUNCHXL-F28379D + BOOSTXL-3PHGANINV (encaixado em J1-J4)
//  LINCE / UFPA
//
//  O programa gera o PWM das tres fases do inversor, sem medir nada e sem
//  controle (malha aberta). Serve para ver o chaveamento no osciloscopio:
//
//    modo 0: razao ciclica de 50 % nas tres fases (nenhuma tensao na carga)
//    modo 1: 25 % na fase A, 50 % na B e 75 % na C (para reconhecer as fases)
//    modo 2: razao ciclica variando como senoide, 120 graus entre as fases
//    modo 3: razao ciclica manual, escolhida em razaoManual[0..2] (A, B, C)
//
//  Variaveis para a janela Watch do CCS (pause o programa, mude o valor e
//  continue): modo, habilita, indice, frequencia, razaoManual.
//  Para observar: razaoCiclica.
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

// --------------------- Variaveis para a janela Watch ------------------------
volatile uint16_t modo       = 0;       // 0: 50 % | 1: 25/50/75 % | 2: senoidal | 3: manual
volatile uint16_t habilita   = 0;       // 1 = liga o inversor (etapa B)
volatile float    indice     = 0.5f;    // indice de modulacao do modo 2 (0 a 1)
volatile float    frequencia = 60.0f;   // frequencia da senoide do modo 2 [Hz]
volatile float    razaoManual[3] = {0.5f, 0.5f, 0.5f};   // modo 3: A, B, C (0,05 a 0,95)

volatile float    razaoCiclica[3];      // razoes ciclicas das fases A, B, C (so leitura)
static float      angulo = 0.0f;        // angulo da senoide [rad]

static const uint32_t modulo[3] = {EPWM1_BASE, EPWM2_BASE, EPWM3_BASE};   // fases A, B, C

__interrupt void AtualizaPWM(void);
static void ConfiguraPinos(void);
static void ConfiguraModuloPWM(uint32_t base);
static void EscreveRazao(uint32_t base, float razao);

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
