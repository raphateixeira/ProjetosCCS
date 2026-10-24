#ifndef HAL_H
#define HAL_H

// ============================================================================
//  HAL: perifericos da LAUNCHXL-F28379D usados com o BOOSTXL-3PHGANINV (J1-J4)
//
//  Ligacoes (MotorControl SDK, boostxl_3phganinv/f2837x, conferidas no guia
//  SLUUBP1, no TIDA-00909 e no SPRUI77):
//
//    Fase A -> EPWM1A/1B (GPIO0/1)   corrente iA -> ADCINC2   tensao vA -> ADCINC3
//    Fase B -> EPWM2A/2B (GPIO2/3)   corrente iB -> ADCINB2   tensao vB -> ADCINB3
//    Fase C -> EPWM3A/3B (GPIO4/5)   corrente iC -> ADCINA2   tensao vC -> ADCINA3
//    Barramento Vdc -> ADCIN14 (lido pela ADCD)
//    /PWM enable -> GPIO124, ATIVO EM NIVEL BAIXO (0 = buffer ligado, 1 = desligado)
//    /PCB OT alert -> GPIO24 (sobretemperatura, ativo baixo)
//    UART -> SCIA, GPIO42 (TX) / GPIO43 (RX), pela USB da LaunchPad (XDS100)
//
//  PWM simetrico (contador sobe e desce, EPWMCLK = 100 MHz), saida A em nivel
//  alto enquanto o contador < CMPA: razao ciclica d = CMPA/TBPRD; saida B =
//  complemento de A com tempo morto. O EPWM1 dispara as conversoes (SOCA) no
//  zero do contador, a cada nControle periodos; o fim da conversao (ADCA INT1)
//  chama o laco de controle: Ts = nControle/fPWM.
//
//  Protecao: CMPSS6 (iA), CMPSS3 (iB), CMPSS1 (iC) e o alerta de temperatura
//  -> XBAR TRIP4 -> DCAEVT1 -> OST (one-shot) nos tres ePWM: saidas A e B em
//  nivel baixo ate o software limpar a falha. A parada do emulador (CBC6)
//  tambem desliga as saidas.
// ============================================================================

#include "driverlib.h"
#include "device.h"

#define EPWMCLK_HZ    100e6f     // relogio dos ePWM: SYSCLK/2 = 100 MHz
#define VREF_ADC      3.0f       // referencia do ADC na LaunchPad (REF5030, SPRUI77)
#define VDDA_CMPSS    3.3f       // referencia do DAC dos comparadores (VDDA)
#define K_INA240      0.1f       // INA240A1 (20 V/V) sobre shunt de 5 mOhm: 0,1 V/A
#define K_DIVISOR     24.70f     // divisor de tensao 100k/4,22k (TIDA-00909): V = 24,7 vADC
#define N_MEIO_NOM    2253.0f    // 1,65 V em contagens (referencia de 3,0 V)

// Sinal das correntes: o INA240 tem IN+ e IN- trocados no BOOSTXL (corrente
// "invertida", TIDA-00909 fig. 10). Com -1, corrente positiva = do inversor
// para a carga. CONFERIR no nivel 2 (vA > 0 em regime => iA > 0).
#define SINAL_CORRENTE  (-1.0f)

// Alerta de sobretemperatura no caminho do trip por hardware (1) ou nao (0)
#define OT_NO_TRIP    1

typedef struct
{
    float    fPWM;        // frequencia de chaveamento [Hz]
    uint16_t nControle;   // periodos de PWM por amostra de controle (1 a 15)
    float    tMorto;      // tempo morto [s]
    float    iLimite;     // limite de corrente da protecao (CMPSS) [A]
} ParametrosHAL;

typedef struct
{
    float ia, ib, ic;     // correntes de fase [A]
    float va, vb, vc;     // tensoes de fase em relacao ao negativo [V] (filtro de 1 kHz na placa)
    float Vdc;            // tensao do barramento [V]
} Medidas;

// Inicializa o microcontrolador e os perifericos. Ao final: habilitacao em 1
// (buffer desligado), saidas forcadas em nivel baixo (OST), contadores parados.
void HAL_Configura(const ParametrosHAL *p, void (*lacoControle)(void));

// Liga os contadores dos tres ePWM ao mesmo tempo e a interrupcao do controle
void HAL_IniciaContadores(void);

// Le os ADCs e converte para A e V (chamada no laco de controle)
void HAL_Le(Medidas *m);

// Calibracao do offset dos INA240 (no laco de controle, com o PWM desabilitado):
// HAL_PedeCalibracao() zera os acumuladores; HAL_AcumulaOffset() soma uma
// amostra e devolve 1 quando terminou (nAmostras somadas).
void     HAL_PedeCalibracao(uint16_t nAmostras);
uint16_t HAL_AcumulaOffset(void);
uint16_t HAL_OffsetsValidos(void);        // 1 se os tres offsets estao perto de 1,65 V
extern float offsetA, offsetB, offsetC;   // offsets medidos [contagens]

// Limite de corrente dos comparadores [A], em torno do ponto medio medido
void HAL_LimiteCorrente(float iLim);

// Razoes ciclicas dos bracos A, B, C (0 a 1)
void HAL_AplicaRazoes(const float d[3]);

// Habilitacao: limpa o trip e poe /PWM enable em 0. Desabilitacao: forca o
// OST (saidas em nivel baixo) e poe /PWM enable em 1.
void HAL_HabilitaPWM(void);
void HAL_DesabilitaPWM(void);

// 1 se houve trip (OST) em algum ePWM
uint16_t HAL_Trip(void);
// Bits da origem do trip: 1 = iA, 2 = iB, 4 = iC (comparadores), 8 = temperatura
uint16_t HAL_OrigemTrip(void);
// Limpa os latches dos comparadores e as flags de trip (sem habilitar)
void HAL_LimpaTrip(void);

// 1 se o alerta de sobretemperatura (GPIO24, ativo baixo) esta ativo
static inline uint16_t HAL_SobreTemperatura(void)
{
    return (GPIO_readPin(24U) == 0U) ? 1U : 0U;
}

// Tempo desde o inicio do periodo de PWM [us] (contador do ePWM1 subindo)
static inline float HAL_TempoDesdeZero(void)
{
    uint16_t c = EPWM_getTimeBaseCounterValue(EPWM1_BASE);
    return (EPWM_getTimeBaseCounterDirection(EPWM1_BASE) == EPWM_TIME_BASE_STATUS_COUNT_UP)
           ? (float)c * 0.01f : -1.0f;   // 10 ns por contagem; -1 se ja passou do meio periodo
}

// Fim do laco de controle: limpa o pedido do ADCA e libera o grupo 1
static inline void HAL_FimLaco(void)
{
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    if (ADC_getInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1))
    {
        ADC_clearInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    }
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

// UART (SCIA) a "baud" bits/s, 8N1, com FIFO
void HAL_ConfiguraSCI(uint32_t baud);

#endif
