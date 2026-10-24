// ============================================================================
//  VSI02ControleCorrente - corrente de uma carga RL trifasica com o inversor
//  GaN BOOSTXL-3PHGANINV (LAUNCHXL-F28379D, headers J1-J4)
//
//  Carga em estrela: R = 4,7 Ohm e L = 1 mH por fase.
//  Niveis (ref.nivel, comando "N n" pela UART):
//    1  PWM de 50 % nas tres fases (verifica PWM, tempo morto e offsets)
//    2  tensao em malha aberta, angulo em rampa: correntes abc e dq
//    3  malha de corrente em dq: PI + desacoplamento w L
//    4  identificacao: excitacao (PRBS, degrau, multisseno) em v_dq + gravacao
//
//  Sequencia segura:
//    - /PWM enable em 1 (buffer desligado) e saidas forcadas em nivel baixo
//      (OST) desde a configuracao; trip por sobrecorrente sempre ativo;
//    - offsets dos INA240 calibrados com o PWM desabilitado (precisa do
//      barramento ligado: o LM5017 da placa alimenta o INA240 e o buffer);
//    - o PWM so liga com o comando "H", sem falhas, com offsets validos e com
//      o barramento entre VDC_MIN e VDC_MAX.
// ============================================================================

#include <math.h>
#include "hal.h"
#include "controle.h"
#include "tarefas.h"

#define VDC_MIN        10.0f    // barramento minimo para habilitar [V]
#define VDC_MAX        30.0f    // barramento maximo [V] (comece com 12 V)
#define N_CALIBRACAO   4096U    // amostras na media dos offsets (~0,2 s a 20 kHz)

Supervisao sup;
static volatile float iLimite;  // limite de corrente atual [A]

__interrupt void LacoControle(void);
static void Supervisor(void);

void main(void)
{
    // ---- Inversor e protecao:
    const ParametrosHAL ph = {
        .fPWM      = 20e3f,     // frequencia de chaveamento [Hz]
        .nControle = 1U,        // periodos de PWM por amostra de controle: Ts = nControle/fPWM
        .tMorto    = 0.5e-6f,   // tempo morto [s] (como no SDK)
        .iLimite   = 3.0f,      // trip por sobrecorrente [A]
    };
    // ---- Carga e controle:
    const ParametrosControle pc = {
        .Ts     = (float)ph.nControle / ph.fPWM,   // 50 us
        .R      = 4.7f,         // [Ohm]
        .L      = 1.0e-3f,      // [H]
        .fc     = 500.0f,       // banda da malha de corrente [Hz]
        .vMax   = 6.0f,         // limite de |v_dq| [V] (6 V / 4,7 Ohm = 1,3 A)
        .VdcNom = 12.0f,        // barramento nominal [V]
    };

    iLimite = ph.iLimite;
    HAL_Configura(&ph, LacoControle);   // PWM desabilitado, protecao pronta
    Controle_Configura(&pc);            // ganhos do PI, tabela do multisseno
    Tarefas_Configura(pc.Ts);           // UART

    sup.calibrando = 1U;                // calibra os offsets nas primeiras amostras
    HAL_PedeCalibracao(N_CALIBRACAO);
    HAL_IniciaContadores();             // laco de controle comeca (PWM ainda desabilitado)
    while (sup.calibrando)
    {
    }

    for (;;)
    { // Laco de fundo: comunicacao e supervisao
        Tarefas_Comunicacao();
        Supervisor();
    }
}

// ---------------------------------------------------------------------------
// Laco de controle: chamado pelo fim da conversao do ADC (disparada pelo
// EPWM1 no zero do contador), a cada Ts
__interrupt void LacoControle(void)
{
    float d[3];

    HAL_Le(&sup.m);                     // correntes, tensoes e barramento

    if (sup.calibrando && HAL_AcumulaOffset())
    {
        sup.offsetOk = (HAL_OffsetsValidos() && sup.m.Vdc > VDC_MIN) ? 1U : 0U;
        HAL_LimiteCorrente(iLimite);    // limiares em torno dos offsets medidos
        sup.calibrando = 0U;
    }

    // Protecao por software (o trip por hardware ja desligou as saidas)
    if (HAL_Trip())
    {
        if (!(sup.falha & FALHA_TRIP)) sup.origem = HAL_OrigemTrip();
        sup.falha |= FALHA_TRIP;
    }
    if (sup.m.Vdc > VDC_MAX) sup.falha |= FALHA_VDC_ALTA;
    if (sup.habilitado)
    {
        if (fabsf(sup.m.ia) > iLimite || fabsf(sup.m.ib) > iLimite ||
            fabsf(sup.m.ic) > iLimite)  sup.falha |= FALHA_CORRENTE;
        if (sup.m.Vdc < VDC_MIN)        sup.falha |= FALHA_VDC_BAIXA;
        if (sup.falha)
        {
            HAL_DesabilitaPWM();
            sup.habilitado = 0U;
        }
    }

    Controle_Passo(&sup.m, sup.habilitado, d);   // nivel 1..4 -> razoes ciclicas
    HAL_AplicaRazoes(d);                         // valem a partir do proximo zero
    Tarefas_Registra();                          // gravacao sincrona

    sup.tLaco = HAL_TempoDesdeZero();            // [us] desde o zero do contador
    HAL_FimLaco();
}

// ---------------------------------------------------------------------------
// Supervisor (laco de fundo): atende os pedidos recebidos pela UART
static void Supervisor(void)
{
    if (pedido.desabilita)
    {
        pedido.desabilita = 0U;
        HAL_DesabilitaPWM();
        sup.habilitado = 0U;
    }
    if (pedido.iLimite > 0.0f)
    {
        iLimite = pedido.iLimite;
        pedido.iLimite = 0.0f;
        HAL_LimiteCorrente(iLimite);
    }
    if (pedido.limpa)
    {
        pedido.limpa = 0U;
        if (!sup.habilitado)
        {
            HAL_LimpaTrip();            // se a causa persistir, a falha volta
            sup.falha = 0U;
            sup.origem = 0U;
        }
    }
    if (pedido.calibra)
    {
        pedido.calibra = 0U;
        if (!sup.habilitado)
        {
            HAL_PedeCalibracao(N_CALIBRACAO);
            sup.calibrando = 1U;
        }
    }
    if (pedido.habilita)
    {
        pedido.habilita = 0U;
        if (!sup.habilitado && sup.falha == 0U && sup.offsetOk && !sup.calibrando &&
            sup.m.Vdc > VDC_MIN && sup.m.Vdc < VDC_MAX)
        {
            Controle_Reinicia();
            sup.habilitado = 1U;
            HAL_HabilitaPWM();
        }
    }
}
