#include "controle.h"

// Buffers no bloco RAMGS0 (secao "ramgs0" do linker)
#pragma DATA_SECTION(r_buf, "ramgs0")
#pragma DATA_SECTION(u_buf, "ramgs0")
#pragma DATA_SECTION(y_buf, "ramgs0")
float r_buf[N_AMOSTRAS];
float u_buf[N_AMOSTRAS];
float y_buf[N_AMOSTRAS];

volatile uint16_t k = 0U;
volatile uint16_t ensaio_concluido = 0U;

// Parametros alteraveis pelo WATCH
volatile float    Kp            = 1.0f;   // ganho proporcional
volatile uint16_t usar_bias     = 1U;     // 1: u = U_OP + Kp*e   0: u = Kp*e
volatile uint16_t reiniciar     = 0U;     // 1: reinicia o ensaio
volatile uint16_t modo_continuo = 0U;     // 0: grava uma vez   1: buffer circular

// Referencia: trecho inicial, depois onda quadrada R_ALTO / R_BAIXO.
// No modo continuo o trecho inicial fica em R_BAIXO, para a forma de onda
// se repetir sem degraus intermediarios a cada volta do buffer.
static float referencia(uint16_t n)
{
    uint16_t fase;

    if (n < N_PRE)
    {
        return (modo_continuo != 0U) ? R_BAIXO : U_OP;
    }

    fase = ((n - N_PRE) / N_MEIO) % 2U;
    return (fase == 0U) ? R_ALTO : R_BAIXO;
}

// Chamada a cada Ts: recebe y(k) e devolve u(k)
float controle_passo(float y)
{
    float r, e, u;

    // Reinicio do ensaio pedido pelo WATCH
    if (reiniciar != 0U)
    {
        k = 0U;
        ensaio_concluido = 0U;
        reiniciar = 0U;
    }

    // Modo continuo: buffer circular, volta ao inicio ao encher
    if ((modo_continuo != 0U) && (k >= N_AMOSTRAS))
    {
        k = 0U;
        ensaio_concluido = 0U;
    }

    // Durante o ensaio segue a referencia; depois mantem r = U_OP
    r = (k < N_AMOSTRAS) ? referencia(k) : U_OP;

    // Lei de controle P
    e = r - y;
    u = Kp * e;
    if (usar_bias != 0U)
    {
        u += U_OP;
    }

    // Saturacao do atuador
    if (u > U_MAX) u = U_MAX;
    if (u < U_MIN) u = U_MIN;

    // Gravacao
    if (k < N_AMOSTRAS)
    {
        r_buf[k] = r;
        u_buf[k] = u;
        y_buf[k] = y;
        k++;
    }
    else
    {
        ensaio_concluido = 1U;
    }

    return u;
}
