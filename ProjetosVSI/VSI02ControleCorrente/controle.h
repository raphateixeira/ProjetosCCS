#ifndef CONTROLE_H
#define CONTROLE_H

// ============================================================================
//  Controle da corrente da carga RL trifasica (estrela, sem neutro)
//
//  Transformadas (invariantes em amplitude):
//    Clarke:  ialfa = (2 ia - ib - ic)/3,   ibeta = (ib - ic)/sqrt(3)
//    Park:    id =  ialfa cos(th) + ibeta sin(th)
//             iq = -ialfa sin(th) + ibeta cos(th)
//  e as inversas para as tensoes. Angulo th = 2 pi f t (frequencia fixa).
//
//  Modelo da carga em dq (R, L por fase, w = 2 pi f):
//    vd = R id + L did/dt - w L iq,     vq = R iq + L diq/dt + w L id
//
//  Niveis:
//    1  razao ciclica fixa de 50 % nas tres fases (nenhuma tensao na carga)
//    2  tensao em malha aberta: vd = V, vq = 0, angulo em rampa (f, V ajustaveis)
//    3  malha de corrente em dq: PI + desacoplamento w L
//    4  identificacao: v_dq = ponto de operacao + excitacao (PRBS, degrau ou
//       multisseno), em malha aberta; o registro grava v_dq e i_dq juntos
// ============================================================================

#include <stdint.h>
#include "hal.h"

#define NIVEL_PWM50          1
#define NIVEL_MALHA_ABERTA   2
#define NIVEL_MALHA_CORRENTE 3
#define NIVEL_IDENTIFICACAO  4

// Modulacao: SVPWM (injecao da media entre maximo e minimo, ate Vdc/sqrt(3))
// ou senoidal (ate Vdc/2)
#define MOD_SENOIDAL  0
#define MOD_SVPWM     1
#define MODULACAO     MOD_SVPWM

#define EXC_PRBS        0
#define EXC_DEGRAU      1     // onda quadrada 0 -> 1 -> 0 (degraus repetidos)
#define EXC_MULTISSENO  2

#define EIXO_D   0
#define EIXO_Q   1
#define EIXO_DQ  2            // os dois eixos, com sequencias diferentes

#define N_MULTI  1000U        // periodo do multisseno [amostras]: f0 = 1/(N_MULTI Ts)

typedef struct
{
    float Ts;        // periodo de amostragem do controle [s]
    float R;         // resistencia por fase da carga [Ohm]
    float L;         // indutancia por fase da carga [H]
    float fc;        // banda da malha de corrente [Hz]: kp = 2 pi fc L, ki = 2 pi fc R
    float vMax;      // limite da tensao de fase |v_dq| [V] (alem do limite da modulacao)
    float VdcNom;    // tensao nominal do barramento [V] (usada se a medida falhar)
} ParametrosControle;

// Referencias e ajustes (podem mudar pela UART ou pela janela Expressions)
typedef struct
{
    volatile uint16_t nivel;      // 1 a 4
    volatile float f;             // frequencia do angulo [Hz] (0 = eixos parados)
    volatile float V;             // nivel 2: amplitude da tensao de fase (vd) [V]
    volatile float idRef, iqRef;  // nivel 3: referencias de corrente [A]
    volatile float vd0, vq0;      // nivel 4: ponto de operacao [V]
    volatile uint16_t tipoExc;    // nivel 4: EXC_PRBS, EXC_DEGRAU, EXC_MULTISSENO
    volatile float ampExc;        // nivel 4: amplitude da excitacao [V]
    volatile uint16_t eixoExc;    // nivel 4: EIXO_D, EIXO_Q, EIXO_DQ
    volatile uint16_t nbPRBS;     // amostras por bit da PRBS
    volatile uint16_t nDegrau;    // meio periodo da onda quadrada [amostras]
} Referencias;

// Variaveis internas (para o registro e para a janela Expressions)
typedef struct
{
    float th;                // angulo [rad]
    float ialfa, ibeta;      // correntes em alfa-beta [A]
    float id, iq;            // correntes em dq [A]
    float vd, vq;            // tensoes aplicadas em dq [V] (depois da saturacao)
    float va, vb, vc;        // tensoes de fase aplicadas [V]
    float ed, eq;            // excitacao atual (sem a amplitude): -1..1
    float Vdc;               // barramento filtrado [V]
    uint32_t k;              // amostra da excitacao (zerada por Controle_ReiniciaExcitacao)
} Estado;

extern Referencias ref;
extern Estado est;

void Controle_Configura(const ParametrosControle *p);

// Zera os integradores (chamar antes de habilitar o PWM)
void Controle_Reinicia(void);

// Recomeca a excitacao do nivel 4 (k = 0, PRBS no estado inicial)
void Controle_ReiniciaExcitacao(void);

// Um passo do controle: medidas -> razoes ciclicas dos tres bracos
void Controle_Passo(const Medidas *m, uint16_t habilitado, float d[3]);

#endif
