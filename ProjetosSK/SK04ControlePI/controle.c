#include "controle.h"

// Buffers no bloco RAMGS0 (4 x 380 floats = 3040 palavras)
#pragma DATA_SECTION(r_buf, "ramgs0")
#pragma DATA_SECTION(u_buf, "ramgs0")
#pragma DATA_SECTION(y_buf, "ramgs0")
#pragma DATA_SECTION(i_buf, "ramgs0")
float r_buf[N_AMOSTRAS];
float u_buf[N_AMOSTRAS];
float y_buf[N_AMOSTRAS];
float i_buf[N_AMOSTRAS];

volatile uint16_t k = 0U;
volatile uint16_t ensaio_concluido = 0U;

// Parametros alteraveis pelo WATCH
volatile float    Kc            = 1.5f;     // ganho do PI: C(z) = Kc (z - z0)/(z - 1)
volatile float    z0            = 0.8156f;  // zero do PI (cancela o polo do modelo)
volatile uint16_t anti_windup   = 0U;       // 0: desligado   1: integracao condicional
volatile float    r_centro      = 1.0f;     // setpoint (centro da onda) [V]
volatile float    r_amp         = 0.2f;     // amplitude da onda quadrada [V]
volatile uint16_t n_meio        = 60U;      // amostras por nivel: f = 1/(2*n_meio*Ts)
volatile uint16_t reiniciar     = 0U;       // 1: reinicia o ensaio
volatile uint16_t modo_continuo = 0U;       // 0: grava uma vez   1: osciloscopio

// Estados internos
static float    integ = U_OP;   // integrador (comeca no ponto de operacao)
static uint32_t n_ref = 0UL;    // contador da referencia no modo continuo

// Onda quadrada: comeca no nivel alto, troca a cada n_meio amostras
static float onda_quadrada(uint32_t n, uint16_t meio)
{
    uint32_t fase = (n / meio) % 2UL;
    return (fase == 0UL) ? (r_centro + r_amp) : (r_centro - r_amp);
}

// Chamada a cada Ts: recebe y(k) e devolve u(k)
float controle_passo(float y)
{
    float r, e, u, v, integ_novo, Kp, KiTs;
    uint16_t meio = (n_meio > 0U) ? n_meio : 1U;   // protege contra n_meio = 0

    // Reinicio do ensaio pedido pelo WATCH: zera buffer e integrador
    if (reiniciar != 0U)
    {
        k = 0U;
        ensaio_concluido = 0U;
        n_ref = 0UL;
        integ = r_centro;
        reiniciar = 0U;
    }

    // Referencia
    if (modo_continuo != 0U)
    {
        r = onda_quadrada(n_ref, meio);

        // Gatilho: com o buffer cheio, recomeca so no inicio de um periodo
        if ((k >= N_AMOSTRAS) && ((n_ref % (2UL * meio)) == 0UL))
        {
            k = 0U;
            ensaio_concluido = 0U;
        }
        n_ref++;
    }
    else
    {
        if (k < N_PRE)
        {
            r = r_centro;
        }
        else if (k < N_AMOSTRAS)
        {
            r = onda_quadrada((uint32_t)(k - N_PRE), meio);
        }
        else
        {
            r = r_centro;
        }
    }

    // Ganhos na forma paralela a partir de Kc e z0
    //   C(z) = Kc (z - z0)/(z - 1)  ->  Kp = Kc*z0,  Ki*Ts = Kc*(1 - z0)
    Kp   = Kc * z0;
    KiTs = Kc * (1.0f - z0);

    // Lei PI posicional: u = Kp*e + I,  I(k) = I(k-1) + Ki*Ts*e(k)
    e = r - y;
    integ_novo = integ + KiTs * e;
    v = Kp * e + integ_novo;          // acao desejada (antes da saturacao)

    // Anti-windup por integracao condicional:
    // nao integra se o atuador esta saturado e o erro empurra mais para a saturacao
    if ((anti_windup != 0U) &&
        (((v > U_MAX) && (e > 0.0f)) || ((v < U_MIN) && (e < 0.0f))))
    {
        v = Kp * e + integ;           // mantem o integrador congelado
    }
    else
    {
        integ = integ_novo;
    }

    // Saturacao do atuador
    u = v;
    if (u > U_MAX) u = U_MAX;
    if (u < U_MIN) u = U_MIN;

    // Gravacao
    if (k < N_AMOSTRAS)
    {
        r_buf[k] = r;
        u_buf[k] = u;
        y_buf[k] = y;
        i_buf[k] = integ;
        k++;
    }
    else
    {
        ensaio_concluido = 1U;
    }

    return u;
}
