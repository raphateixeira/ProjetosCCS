// ============================================================================
//  BK01Degrau - ensaio ao degrau (onda quadrada) do conversor buck em malha aberta
//
//  A placa comanda a tensao media u aplicada ao filtro LC pela razao ciclica
//  do PWM (d = u/Vin) e le a tensao de saida y no ADC (por um divisor resistivo).
//  Sequencia: rampa de 0 a vBaixo (partida suave) -> acomodacao em vBaixo ->
//  gravacao com u em onda quadrada entre vBaixo e vAlto (10 V <-> 12 V).
//
//  Gravados: uV[k] = u(k), yV[k] = y(k), k = 0 .. 1999   ("Capacity" = 2000)
// ============================================================================

#include "placa.h"
#include "ensaio.h"

__interrupt void LacoAmostragem(void);

void main(void)
{
    // ---- Parametros do conversor (placa + estagio de potencia):
    const ParametrosConversor c = {
        .fPWM    = 20e3f,     // frequencia de chaveamento [Hz]: 20 kHz
        .Ts      = 50e-6f,    // periodo de amostragem [s]: 50 us (multiplo de 1/fPWM, ate 15/fPWM)
        .Vin     = 30.0f,     // tensao de entrada do buck [V]
        .uMax    = 15.0f,     // maior tensao comandada [V] (protecao): d <= uMax/Vin = 0,5
        .kSensor = 0.1f,      // ganho do divisor de y: 27 k / 3 k -> 0 .. 30 V em 0 .. 3 V
    };

    // ---- Parametros do ensaio:
    const ParametrosDegrau p = {
        .Ts       = c.Ts,
        .vBaixo   = 10.0f,    // nivel baixo de u [V] (ponto de operacao): d = 1/3
        .vAlto    = 12.0f,    // nivel alto de u [V]: d = 0,4 (degrau de 2 V)
        .Tq       = 20e-3f,   // periodo da onda quadrada [s]: 10 ms em cada nivel
        .tAtraso  = 5e-3f,    // tempo em vBaixo antes da primeira subida [s]
        .tRampa   = 0.5f,     // partida suave: u sobe de 0 a vBaixo em tRampa [s]
        .tAcomoda = 0.5f,     // tempo em vBaixo depois da rampa, antes de gravar [s]
    };

    ConfiguraF28379D(&c, LacoAmostragem);           // F79: clock, ePWM1, ADC
    ConfiguraEnsaioDegrau(&p);                      // Ensaio: niveis e tempos
    IniciaAmostragem();                             // PWM: chaveia e amostra a cada Ts

    for (;;)
    { // Rotina de fundo (comunicacao serial etc.)
    }
}

// Laco de amostragem: rotina de interrupcao (ISR) chamada pelo hardware a cada Ts
// (fim da conversao do ADC, disparada pelo ePWM1). Cada chamada e uma iteracao k:
// Le y(k) -> calcula u(k), aplica u(k).
__interrupt void LacoAmostragem(void)
{
    float y = ADC2y();       // y(k) <- conversor AD (ADCINA3)
    float u = Degrau(k);     // u(k): rampa, vBaixo ou onda quadrada
    Grava(u, y);             // uV[k] = u(k), yV[k] = y(k); avanca k
    PWM2u(u);                // u(k) -> razao ciclica d = u/Vin (EPWM1A)

    FimAmostragem();
}
