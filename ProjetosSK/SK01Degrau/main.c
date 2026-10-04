// ============================================================================
//  SK01Degrau - ensaio ao degrau (onda quadrada) do filtro Sallen-Key
//  Gravados: uV[k] = u(k), yV[k] = y(k), k = 0 .. 1999   ("Capacity" = 2000)
// ============================================================================

#include "placa.h"
#include "ensaio.h"

__interrupt void LacoAmostragem(void);

void main(void)
{
    // ---- Parametros do ensaio:
    const ParametrosDegrau p = {
        .Ts       = 1e-3f,    // Periodo de amostragem [s]: 1 ms
        .u0       = 1.0f,     // nivel medio de u [V]
        .A        = 0.4f,     // amplitude [V]: u [0,6 e 1,4] V (degrau de 0,8 V)
        .Tq       = 0.5f,     // periodo da onda quadrada [s]: 0,25 s em cada nivel
        .tAtraso  = 0.2f,     // tempo em u0 - A antes da subida [s]
        .tAcomoda = 2.0f,     // tempo em u0 - A antes de gravar [s]
    };

    ConfiguraF28379D(p.Ts, LacoAmostragem);         // F79: clock, ADC, DAC, timer
    ConfiguraEnsaioDegrau(&p);                      // Ensaio: niveis e tempos
    IniciaAmostragem();                             // Timer: amostra a cada Ts

    for (;;)
    { // Rotina de fundo (comunicacao serial etc.)
    }
}

// Laco de amostragem: rotina de interrupcao (ISR) chamada pelo hardware a cada Ts
// (fim da conversao do ADC). Cada chamada e uma iteracao k do laco:
// Le y(k) -> calcula u(k), aplica u(k).
__interrupt void LacoAmostragem(void)
{
    float y = ADC2y();       // y(k) <- conversor AD (ADCINA3)
    float u = Degrau(k);     // u(k) = u0 + A q(k)
    Grava(u, y);             // uV[k] = u(k), yV[k] = y(k); avanca k
    DAC2u(u);                // u(k) -> conversor DA (DACOUTA)

    FimAmostragem();
}
