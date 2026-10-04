#include <math.h>
#include "controle.h"

#define DOIS_PI 6.28318530718f

// Periodo de cada senoide em amostras: f = 1/(N*Ts) = 0,5 ... 50 Hz (Nyquist = 100 Hz)
static const uint16_t N_PERIODO[NF] = {400U, 200U, 100U, 50U, 40U, 30U, 25U, 20U,
                                       16U, 12U, 10U, 8U, 6U, 5U, 4U};

// y da varredura inteira na regiao RAMGS7..RAMGS11 (secao "ramgs7_11" do linker)
#pragma DATA_SECTION(y_buf, "ramgs7_11")
float y_buf[N_BUF];

float freq_hz[NF];
float ganho[NF];
float ganho_db[NF];
float fase_graus[NF];
float amp_u[NF];
float amp_y[NF];

volatile uint16_t k = 0U;
volatile uint16_t i_freq = 0U;
volatile uint16_t ensaio_concluido = 0U;
volatile uint16_t reiniciar = 0U;

// Estado
static uint16_t inicial;          // amostras restantes paradas em U_OP antes do ensaio
static uint16_t n_per;            // periodo da frequencia atual [amostras]
static uint16_t m;                // fase da senoide: 0..n_per-1
static uint16_t acomoda;          // amostras restantes de acomodacao nesta frequencia
static uint16_t mede;             // amostras restantes de integracao nesta frequencia
static float    su_s, su_c, sy_s, sy_c;   // somas de u e y contra sen e cos
static uint16_t n_int;            // amostras integradas

static void prepara_frequencia(uint16_t i)
{
    uint16_t periodos;

    n_per   = N_PERIODO[i];
    m       = 0U;
    acomoda = (n_per > N_ACOMODA_MIN) ? n_per : N_ACOMODA_MIN;
    periodos = (uint16_t)((N_MEDE_MIN + n_per - 1U) / n_per);   // ceil(N_MEDE_MIN/N)
    if (periodos < 2U)
    {
        periodos = 2U;
    }
    mede  = periodos * n_per;                                    // numero inteiro de periodos
    su_s = su_c = sy_s = sy_c = 0.0f;
    n_int = 0U;
    freq_hz[i] = 1.0f / ((float)n_per * TS_ENSAIO);
}

static void conclui_frequencia(uint16_t i)
{
    // Amplitudes (fasores) de u e y na frequencia: X = (2/n) * sum x * e^{-j theta}
    float k2  = 2.0f / (float)n_int;
    float ur  = k2 * su_s, ui = k2 * su_c;      // u = U_AMP sen(theta) -> fasor (ur, ui)
    float yr  = k2 * sy_s, yi = k2 * sy_c;
    float au  = sqrtf(ur * ur + ui * ui);
    float ay  = sqrtf(yr * yr + yi * yi);
    float fase = atan2f(yi, yr) - atan2f(ui, ur);

    while (fase > 3.14159265f)  { fase -= DOIS_PI; }
    while (fase < -3.14159265f) { fase += DOIS_PI; }

    amp_u[i]      = au;
    amp_y[i]      = ay;
    ganho[i]      = (au > 0.0f) ? (ay / au) : 0.0f;
    ganho_db[i]   = (ganho[i] > 0.0f) ? (20.0f * log10f(ganho[i])) : -200.0f;
    fase_graus[i] = fase * (180.0f / 3.14159265f);
}

float controle_valor_inicial(void)
{
    return U_OP;
}

void controle_inicia(void)
{
    uint16_t i;

    for (i = 0U; i < NF; i++)
    {
        freq_hz[i] = ganho[i] = ganho_db[i] = fase_graus[i] = amp_u[i] = amp_y[i] = 0.0f;
    }
    inicial = N_INICIAL;
    i_freq = 0U;
    ensaio_concluido = 0U;
    k = 0U;
    prepara_frequencia(0U);
}

// Chamada a cada Ts pela ISR do ADC: recebe y(k) e devolve u(k)
//
// u(k) e y(k) sao integrados com a MESMA referencia de fase theta(k). Como y(k) e lido antes
// de u(k) ir para o DAC, H = Y/U inclui a amostra de atraso do firmware e o segurador de
// ordem zero -- exatamente o que o modelo discreto G(z) (ARX, nk = 1) descreve.
float controle_passo(float y)
{
    float th, s, c, u;

    if (reiniciar != 0U)
    {
        reiniciar = 0U;
        controle_inicia();
    }

    if (ensaio_concluido != 0U)                // fim: volta ao ponto de operacao
    {
        return U_OP;
    }

    if (k < N_BUF)                             // grava y(k) em todo o ensaio
    {
        y_buf[k] = y;
        k++;
    }

    if (inicial > 0U)                          // espera inicial em U_OP
    {
        inicial--;
        return U_OP;
    }

    // u(k) = U_OP + U_AMP sen(theta(k)), theta(k) = 2 pi m / N
    th = DOIS_PI * (float)m / (float)n_per;
    s  = sinf(th);
    c  = cosf(th);
    u  = U_OP + U_AMP * s;

    if (acomoda > 0U)                          // transitorio: so aplica a senoide
    {
        acomoda--;
    }
    else                                       // integracao (numero inteiro de periodos)
    {
        sy_s += y * s;
        sy_c += y * c;
        su_s += u * s;
        su_c += u * c;
        n_int++;
        mede--;
        if (mede == 0U)
        {
            conclui_frequencia(i_freq);
            if (i_freq + 1U >= NF)
            {
                ensaio_concluido = 1U;
                return U_OP;
            }
            i_freq++;
            prepara_frequencia(i_freq);        // a nova senoide comeca na proxima amostra
            return u;
        }
    }

    m++;
    if (m >= n_per)
    {
        m = 0U;
    }
    return u;
}
