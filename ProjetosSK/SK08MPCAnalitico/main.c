// ============================================================================
//  SK08MPCAnalitico - MPC sem restricoes, solucao analitica (filtro Sallen-Key)
//  Lei calculada antes do laco: du(k) = Ky r~(k) - Kmpc x^(k); no laco, so o
//  observador e quatro multiplicacoes.
//  Gravados (k = 0 .. 799, "Capacity" = 800): rV, uV, yV [V], tV [us], nV
//  Desempenho (Expressions): desempenho.inicializacao, predicaoMax, otimizacaoMax,
//  ocupacaoMax, estouros; Ky, Kmpc
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
        .modelo = {           // ARX(2,2,1), Ts = 1 ms (Nota 02, DadosPRBS1ms.csv)
            .a1 = 1.60918827f, .a2 = -0.62250084f,
            .b1 = 0.01096776f, .b2 = 0.00237673f,
            .uOp = 1.0f,       .yOp = 0.99671789f,
        },
        .Np      = 60,        // horizonte de predicao: 60 ms
        .Nu      = 4,         // horizonte de controle: 4 movimentos
        .lambda  = 3.0f,      // peso de du: t_s ~ 50 ms, sem sobressinal
        .alfaObs = 0.9f,      // polos do observador (constante de tempo ~ 10 ms)
        .uMin   = 0.0f,       // limites do atuador [V]: LM358 em 3,3 V
        .uMax   = 1.7f,
    };

    ConfiguraF28379D(e.Ts, LacoAmostragem);         // F79: clock, ADC, DAC, timers
    ConfiguraEnsaio(&e);                            // Ensaio: referencia e tempos
    ConfiguraMPC(&p, e.Ts);                         // MPC: modelo, matrizes, observador (mede o tempo)
    DAC2u(p.modelo.uOp);                            // partida no ponto de operacao
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
    float u = MPC(r, y);           // u(k): observador + du(k) = Ky r~ - Kmpc x^
    DAC2u(u);                      // u(k) -> conversor DA (DACOUTA)
    Grava(r, u, y, t0 - Cronometro(), desempenho.iteracoes);   // tV[k]: tempo ate aqui

    FimLaco(t0);                   // tempo do laco, ocupacao de Ts, estouros
    FimAmostragem();
}
