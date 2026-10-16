// ============================================================================
//  SK07MPCNumerico - MPC convencional (filtro Sallen-Key)
//  Modelo ARX nas variacoes, previsao por simulacao, custo
//  J = sum (r - y^)^2 + lambda sum du^2 minimizado pelo gradiente a cada amostra
//  (nIter iteracoes por amostra).
//  Gravados (k = 0 .. 799, "Capacity" = 800): rV, uV, yV [V], tV [us], nV
//  Desempenho (Expressions): desempenho.calculoMax, lacoMax, ocupacaoMax, estouros
// ============================================================================

#include "placa.h"
#include "ensaio.h"
#include "mpc.h"

__interrupt void LacoAmostragem(void);

void main(void)
{
    // ---- Parametros do ensaio:
    const ParametrosEnsaio e = {
        .Ts       = 1e-3f,    // Periodo de amostragem [s]: 1 ms
        .rBaixo   = 0.8f,     // referencia: onda quadrada 0,8 <-> 1,2 V
        .rAlto    = 1.2f,
        .Tq       = 0.4f,     // periodo [s]: 200 amostras em cada nivel
        .tInicio  = 0.1f,     // tempo em rBaixo antes da primeira subida [s]
        .tAcomoda = 1.0f,     // malha fechada antes de gravar [s]
    };

    // ---- Parametros do MPC:
    const ParametrosMPC p = {
        .modelo = {           // ARX(2,2,1), Ts = 1 ms (identificacao do Sallen-Key, DadosPRBS1ms.csv)
            .a1 = 1.60918827f, .a2 = -0.62250084f,
            .b1 = 0.01096776f, .b2 = 0.00237673f,
        },
        .Np       = 60,       // horizonte de previsao: 60 amostras = 60 ms
        .Nu       = 4,        // movimentos planejados: du(k) .. du(k+3)
        .lambda   = 3.0f,     // peso de du: t_s ~ 50 ms, sem sobressinal
        .nIter    = 50,       // iteracoes do gradiente por amostra
        .uMin     = 0.0f,     // limites do atuador [V]: LM358 em 3,3 V
        .uMax     = 1.7f,
        .uInicial = 1.0f,     // u na partida [V]
    };

    ConfiguraF28379D(e.Ts, LacoAmostragem);         // F79: clock, ADC, DAC, timers
    ConfiguraEnsaio(&e);                            // Ensaio: referencia e tempos
    ConfiguraMPC(&p, e.Ts);                         // MPC: resposta ao degrau, passo
    DAC2u(p.uInicial);                              // partida
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
    uint32_t t0 = Cronometro();    // inicio do laco [ciclos]

    float y = ADC2y();             // y(k) <- conversor AD (ADCINA3)
    float r = Referencia(k);       // r(k): onda quadrada
    float u = MPC(r, y);           // u(k): previsao + minimizacao pelo gradiente
    DAC2u(u);                      // u(k) -> conversor DA (DACOUTA)
    Grava(r, u, y, t0 - Cronometro(), desempenho.iteracoes);   // tV[k]: tempo ate aqui

    FimLaco(t0);                   // tempo do laco, ocupacao de Ts, estouros
    FimAmostragem();
}
