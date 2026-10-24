#ifndef TAREFAS_H
#define TAREFAS_H

// ============================================================================
//  Tarefas secundarias: registro de dados (no laco de controle) e comunicacao
//  com o PC pela UART (no laco de fundo)
//
//  Registro: quatro canais de N_REG amostras (RAMGS0..RAMGS3), um a cada nDec
//  amostras de controle, a partir do comando G:
//    modo 0: vd, vq (aplicadas), id, iq       modo 1: ia, ib, ic, th
//
//  Protocolo (texto, 115200 8N1, um comando por linha; resposta "OK ..." ou "ERRO ..."):
//    S             estado: nivel hab falha origem Vdc id iq ia ib ic offsetOk tLaco n
//    H / D         habilita / desabilita o PWM
//    L             limpa as falhas (com o PWM desabilitado)
//    C             calibra os offsets dos INA240 (PWM desabilitado, barramento ligado)
//    N n           nivel 1..4 (so com o PWM desabilitado)
//    F f           frequencia do angulo [Hz]
//    V v           nivel 2: amplitude vd [V]
//    R id iq       nivel 3: referencias de corrente [A]
//    P vd0 vq0     nivel 4: ponto de operacao [V]
//    E t a e       nivel 4: excitacao t (0 PRBS, 1 degrau, 2 multisseno), amplitude a [V], eixo e (0 d, 1 q, 2 dq)
//    I i           limite de corrente da protecao [A] (ate I_LIMITE_MAX)
//    M m dec       registro: modo m (0 dq, 1 abc), decimacao dec
//    G             inicia a gravacao (e recomeca a excitacao)
//    B             baixa o registro: "B N modo dec Ts", N linhas "x0,x1,x2,x3", "FIM"
// ============================================================================

#include <stdint.h>
#include "hal.h"

#define N_REG         2000U   // amostras por canal: 2000 floats = 4000 palavras (um bloco RAMGS)
#define I_LIMITE_MAX  8.0f    // maior limite de corrente aceito pela UART [A] (OCP da placa: 12 A)

// Bits de falha (sup.falha)
#define FALHA_TRIP        1U  // trip por hardware (comparadores ou temperatura)
#define FALHA_CORRENTE    2U  // |i| acima do limite (verificacao por software)
#define FALHA_VDC_ALTA    4U
#define FALHA_VDC_BAIXA   8U  // barramento caiu com o PWM habilitado

// Estado do supervisor (definido no main.c)
typedef struct
{
    volatile uint16_t habilitado;   // 1 = PWM habilitado
    volatile uint16_t falha;        // bits FALHA_*
    volatile uint16_t origem;       // HAL_OrigemTrip() no momento da falha
    volatile uint16_t offsetOk;     // 1 = offsets calibrados com o barramento ligado
    volatile uint16_t calibrando;   // 1 durante a calibracao
    volatile float    tLaco;        // duracao do laco de controle [us]
    Medidas m;                      // ultimas medidas
} Supervisao;

extern Supervisao sup;

// Pedidos do PC ao supervisor do main.c (atendidos no laco de fundo)
typedef struct
{
    volatile uint16_t habilita, desabilita, limpa, calibra;
    volatile float    iLimite;        // > 0: novo limite pedido [A]
} Pedidos;

extern Pedidos pedido;

extern float reg0[N_REG], reg1[N_REG], reg2[N_REG], reg3[N_REG];
extern volatile uint16_t nReg;          // amostras gravadas
extern volatile uint16_t gravando;      // 1 durante a gravacao

void Tarefas_Configura(float Ts);

// No laco de controle: grava uma amostra (se gravando)
void Tarefas_Registra(void);

// No laco de fundo: le a UART e executa os comandos completos
void Tarefas_Comunicacao(void);

#endif
