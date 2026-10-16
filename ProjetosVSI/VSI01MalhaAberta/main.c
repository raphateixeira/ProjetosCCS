// ============================================================================
//  VSI01MalhaAberta - inversor trifasico (BOOSTXL-3PHGANINV) em malha aberta
//
//  Gera tres tensoes senoidais defasadas de 120 graus nas fases A, B e C:
//
//      va = V sin(2 pi f t),  vb = V sin(2 pi f t - 120),  vc = V sin(2 pi f t + 120)
//
//  medidas em relacao ao ponto medio do barramento CC (Vcc/2). A amplitude V
//  sobe de 0 a Vp em tRampa segundos (partida suave) e depois fica constante.
//
//  Gravados (depois da rampa): vaV[n], vbV[n], vcV[n], n = 0 .. 1999
//  ("Capacity" = 2000; com fPWM = 20 kHz sao 100 ms = 6 ciclos de 60 Hz)
// ============================================================================

#include "placa.h"
#include "senoides.h"

__interrupt void LacoPWM(void);

void main(void)
{
    // ---- Parametros do inversor:
    const ParametrosInversor p = {
        .fPWM   = 20e3f,      // frequencia de chaveamento [Hz]: 20 kHz (Ts = 50 us)
        .tMorto = 0.5e-6f,    // tempo morto entre as chaves de um braco [s]: 0,5 us
        .Vcc    = 24.0f,      // tensao do barramento CC [V] (fonte ligada ao BOOSTXL)
        .Vp     = 8.0f,       // amplitude das senoides de fase [V] (no maximo Vcc/2)
        .f      = 60.0f,      // frequencia das senoides [Hz]
        .tRampa = 1.0f,       // tempo de subida da amplitude, de 0 a Vp [s]
    };

    ConfiguraF28379D(p.fPWM, p.tMorto, p.Vcc, LacoPWM);  // F79: clock, ePWM 1-3
    ConfiguraSenoides(&p);                                // senoides: f, Vp, rampa
    IniciaPWM();                                          // inversor comeca a chavear

    for (;;)
    { // Rotina de fundo (comunicacao serial etc.)
    }
}

// Laco do PWM: rotina de interrupcao (ISR) chamada pelo hardware no inicio de
// cada periodo de chaveamento (a cada Ts = 1/fPWM). Cada chamada e uma amostra n:
// calcula va, vb, vc -> aplica as razoes ciclicas dos tres bracos.
__interrupt void LacoPWM(void)
{
    float va, vb, vc;

    Senoides(&va, &vb, &vc);    // tensoes de fase desejadas no instante n [V]
    Grava(va, vb, vc);          // vaV[n], vbV[n], vcV[n]; avanca n
    PWM2v(va, vb, vc);          // tensoes -> razoes ciclicas dos bracos A, B, C

    FimLacoPWM();
}
