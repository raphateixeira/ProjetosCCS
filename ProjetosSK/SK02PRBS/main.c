// ============================================================================
//  SK02PRBS - ensaio PRBS do filtro Sallen-Key
//  Gravados: uV[k] = u(k), yV[k] = y(k), k = 0 .. 1999   ("Capacity" = 2000)
//  Ts = 5 ms e Tb = 20 ms (Nb = 4): 2000 amostras = 10 s, quase 2 periodos da PRBS
// ============================================================================

#include "placa.h"
#include "ensaio.h"

__interrupt void LacoAmostragem(void);

void main(void)
{
    // ---- Parametros do ensaio:
    const ParametrosPRBS p = {
        .Ts       = 5e-3f,    // Periodo de amostragem [s]: 5 ms (polos lento e rapido: wTs = 0,18 e 0,6)
        .u0       = 1.0f,     // nivel medio de u [V]
        .A        = 0.4f,     // amplitude [V]: u em 0,6 ou 1,4 V
        .Tb       = 20e-3f,   // duracao do bit [s]: Nb = 4 amostras; faixa 0,44/Tb = 22 Hz, pulso maior 8 Tb = 160 ms;
                              //   periodo da PRBS 255 Tb = 5,1 s
        .tPre     = 0.1f,     // tempo em u0 antes da PRBS [s]
        .tAcomoda = 2.0f,     // tempo em u0 antes de gravar [s]
        .semente  = 0x01U,    // estado inicial do LFSR (1 .. 255)
    };

    ConfiguraF28379D(p.Ts, LacoAmostragem);         // F79: clock, ADC, DAC, timer
    ConfiguraEnsaioPRBS(&p);                        // Ensaio: niveis, bit e tempos
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
    float u = PRBS(k);       // u(k) = u0 + A b(k)
    Grava(u, y);             // uV[k] = u(k), yV[k] = y(k); avanca k
    DAC2u(u);                // u(k) -> conversor DA (DACOUTA)

    FimAmostragem();
}
