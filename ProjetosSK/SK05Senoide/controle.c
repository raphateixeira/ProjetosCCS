#include <math.h>
#include "controle.h"

#define DOIS_PI 6.28318530718f

// u em RAMGS0 e y em RAMGS1 (secoes "ramgs0"/"ramgs1" do linker)
#pragma DATA_SECTION(u_buf, "ramgs0")
#pragma DATA_SECTION(y_buf, "ramgs1")
float u_buf[N_AMOSTRAS];
float y_buf[N_AMOSTRAS];

volatile uint16_t k = 0U;
volatile uint16_t ensaio_concluido = 0U;
volatile uint16_t reiniciar = 0U;

static uint16_t acomoda;
static uint16_t m;                // fase da senoide: 0 .. N_PERIODO-1

float controle_valor_inicial(void)
{
    return U_OP;
}

void controle_inicia(void)
{
    k = 0U;
    ensaio_concluido = 0U;
    acomoda = N_ACOMODA;
    m = 0U;
}

// Chamada a cada Ts pela ISR do ADC: recebe y(k) e devolve u(k)
float controle_passo(float y)
{
    float u;

    if (reiniciar != 0U)
    {
        reiniciar = 0U;
        controle_inicia();
    }

    if (ensaio_concluido != 0U)            // fim: volta ao ponto de operacao
    {
        return U_OP;
    }

    u = U_OP + U_AMP * sinf(DOIS_PI * (float)m / (float)N_PERIODO);
    m++;
    if (m >= N_PERIODO)
    {
        m = 0U;
    }

    if (acomoda > 0U)                      // acomodacao: senoide rodando, sem gravar
    {
        acomoda--;
        return u;
    }

    u_buf[k] = u;                          // ensaio
    y_buf[k] = y;
    k++;
    if (k >= N_AMOSTRAS)
    {
        ensaio_concluido = 1U;
    }
    return u;
}
