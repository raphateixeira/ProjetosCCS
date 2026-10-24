#include "tarefas.h"
#include "controle.h"

#define TAM_LINHA  48U

// Registro: um canal por bloco RAMGS (secoes "ramgs0..3" do linker)
#pragma DATA_SECTION(reg0, "ramgs0")
#pragma DATA_SECTION(reg1, "ramgs1")
#pragma DATA_SECTION(reg2, "ramgs2")
#pragma DATA_SECTION(reg3, "ramgs3")
float reg0[N_REG], reg1[N_REG], reg2[N_REG], reg3[N_REG];

volatile uint16_t nReg = 0U;
volatile uint16_t gravando = 0U;
Pedidos pedido;

static volatile uint16_t modoReg = 0U;     // 0: dq, 1: abc
static volatile uint16_t nDec = 1U;        // decimacao do registro
static uint16_t contDec = 0U;
static float Ts;

static char linha[TAM_LINHA];
static uint16_t nLinha = 0U;

void Tarefas_Configura(float ts)
{
    Ts = ts;
    HAL_ConfiguraSCI(115200UL);
}

// ---------------------------------------------------------------------------
void Tarefas_Registra(void)
{
    if (!gravando)
    {
        return;
    }
    if (++contDec < nDec)
    {
        return;
    }
    contDec = 0U;

    if (modoReg == 0U)
    {
        reg0[nReg] = est.vd;
        reg1[nReg] = est.vq;
        reg2[nReg] = est.id;
        reg3[nReg] = est.iq;
    }
    else
    {
        reg0[nReg] = sup.m.ia;
        reg1[nReg] = sup.m.ib;
        reg2[nReg] = sup.m.ic;
        reg3[nReg] = est.th;
    }
    if (++nReg >= N_REG)
    {
        gravando = 0U;
    }
}

// ---------------------------------------------------------------------------
// Envio (bloqueante, no laco de fundo; o controle continua nas interrupcoes)
static void EnviaTexto(const char *t)
{
    while (*t)
    {
        SCI_writeCharBlockingFIFO(SCIA_BASE, (uint16_t)*t++);
    }
}

static void EnviaInteiro(int32_t x)
{
    char b[12];
    int16_t i = 0;

    if (x < 0)
    {
        SCI_writeCharBlockingFIFO(SCIA_BASE, '-');
        x = -x;
    }
    do
    {
        b[i++] = (char)('0' + (x % 10L));
        x /= 10L;
    } while (x > 0L && i < 11);
    while (i > 0)
    {
        SCI_writeCharBlockingFIFO(SCIA_BASE, (uint16_t)b[--i]);
    }
}

// Numero com 4 casas decimais (sem printf: economiza memoria)
static void EnviaNumero(float x)
{
    int32_t inteiro, frac;
    int16_t i;
    char b[4];

    if (x < 0.0f)
    {
        SCI_writeCharBlockingFIFO(SCIA_BASE, '-');
        x = -x;
    }
    inteiro = (int32_t)x;
    frac = (int32_t)((x - (float)inteiro) * 10000.0f + 0.5f);
    if (frac >= 10000L)
    {
        inteiro++;
        frac -= 10000L;
    }
    EnviaInteiro(inteiro);
    SCI_writeCharBlockingFIFO(SCIA_BASE, '.');
    for (i = 3; i >= 0; i--)
    {
        b[i] = (char)('0' + (frac % 10L));
        frac /= 10L;
    }
    for (i = 0; i < 4; i++)
    {
        SCI_writeCharBlockingFIFO(SCIA_BASE, (uint16_t)b[i]);
    }
}

// Le um numero (com sinal e ponto decimal) a partir de *p; devolve 1 se leu
static uint16_t LeNumero(const char **p, float *x)
{
    const char *s = *p;
    float v = 0.0f, escala = 0.1f;
    uint16_t neg = 0U, achou = 0U;

    while (*s == ' ')
    {
        s++;
    }
    if (*s == '-' || *s == '+')
    {
        neg = (*s == '-');
        s++;
    }
    while (*s >= '0' && *s <= '9')
    {
        v = 10.0f * v + (float)(*s - '0');
        s++;
        achou = 1U;
    }
    if (*s == '.' || *s == ',')
    {
        s++;
        while (*s >= '0' && *s <= '9')
        {
            v += escala * (float)(*s - '0');
            escala *= 0.1f;
            s++;
            achou = 1U;
        }
    }
    *p = s;
    *x = neg ? -v : v;
    return achou;
}

static void Baixa(void)
{
    const float *c[4] = {reg0, reg1, reg2, reg3};
    uint16_t i, j, n = nReg;

    EnviaTexto("B ");
    EnviaInteiro(n);
    EnviaTexto(" ");
    EnviaInteiro(modoReg);
    EnviaTexto(" ");
    EnviaInteiro(nDec);
    EnviaTexto(" ");
    EnviaNumero(Ts * 1e6f);          // Ts em us
    EnviaTexto("\n");
    for (i = 0U; i < n; i++)
    {
        for (j = 0U; j < 4U; j++)
        {
            EnviaNumero(c[j][i]);
            EnviaTexto(j < 3U ? "," : "\n");
        }
    }
    EnviaTexto("FIM\n");
}

static void EnviaEstado(void)
{
    const float v[] = {sup.m.Vdc, est.id, est.iq, sup.m.ia, sup.m.ib, sup.m.ic};
    uint16_t i;

    EnviaTexto("OK S ");
    EnviaInteiro(ref.nivel);        EnviaTexto(" ");
    EnviaInteiro(sup.habilitado);   EnviaTexto(" ");
    EnviaInteiro(sup.falha);        EnviaTexto(" ");
    EnviaInteiro(sup.origem);       EnviaTexto(" ");
    for (i = 0U; i < 6U; i++)
    {
        EnviaNumero(v[i]);
        EnviaTexto(" ");
    }
    EnviaInteiro(sup.offsetOk);     EnviaTexto(" ");
    EnviaNumero(sup.tLaco);         EnviaTexto(" ");
    EnviaInteiro(nReg);             EnviaTexto("\n");
}

// ---------------------------------------------------------------------------
static void Executa(const char *s)
{
    float a = 0.0f, b = 0.0f, c = 0.0f;
    char cmd = *s++;

    switch (cmd)
    {
    case 'S': case '?':
        EnviaEstado();
        return;
    case 'H':
        pedido.habilita = 1U;       // o supervisor responde se nao puder habilitar
        break;
    case 'D':
        pedido.desabilita = 1U;
        break;
    case 'L':
        pedido.limpa = 1U;
        break;
    case 'C':
        if (sup.habilitado) { EnviaTexto("ERRO desabilite o PWM\n"); return; }
        pedido.calibra = 1U;
        break;
    case 'N':
        if (!LeNumero(&s, &a) || a < 1.0f || a > 4.0f) { EnviaTexto("ERRO nivel 1..4\n"); return; }
        if (sup.habilitado) { EnviaTexto("ERRO desabilite o PWM\n"); return; }
        ref.nivel = (uint16_t)a;
        break;
    case 'F':
        if (!LeNumero(&s, &a)) { EnviaTexto("ERRO F f\n"); return; }
        ref.f = a;
        break;
    case 'V':
        if (!LeNumero(&s, &a)) { EnviaTexto("ERRO V v\n"); return; }
        ref.V = a;
        break;
    case 'R':
        if (!LeNumero(&s, &a) || !LeNumero(&s, &b)) { EnviaTexto("ERRO R id iq\n"); return; }
        ref.idRef = a;
        ref.iqRef = b;
        break;
    case 'P':
        if (!LeNumero(&s, &a) || !LeNumero(&s, &b)) { EnviaTexto("ERRO P vd0 vq0\n"); return; }
        ref.vd0 = a;
        ref.vq0 = b;
        break;
    case 'E':
        if (!LeNumero(&s, &a) || !LeNumero(&s, &b) || !LeNumero(&s, &c) ||
            a < 0.0f || a > 2.0f || c < 0.0f || c > 2.0f)
        {
            EnviaTexto("ERRO E tipo(0..2) amp eixo(0..2)\n");
            return;
        }
        ref.tipoExc = (uint16_t)a;
        ref.ampExc = b;
        ref.eixoExc = (uint16_t)c;
        break;
    case 'I':
        if (!LeNumero(&s, &a) || a <= 0.0f || a > I_LIMITE_MAX) { EnviaTexto("ERRO I 0..8\n"); return; }
        pedido.iLimite = a;
        break;
    case 'M':
        if (!LeNumero(&s, &a) || !LeNumero(&s, &b) || a > 1.0f || b < 1.0f) { EnviaTexto("ERRO M modo dec\n"); return; }
        if (gravando) { EnviaTexto("ERRO gravando\n"); return; }
        modoReg = (uint16_t)a;
        nDec = (uint16_t)b;
        break;
    case 'G':
        gravando = 0U;
        nReg = 0U;
        contDec = 0U;
        Controle_ReiniciaExcitacao();
        gravando = 1U;
        break;
    case 'B':
        if (gravando) { EnviaTexto("ERRO gravando\n"); return; }
        Baixa();
        return;
    default:
        EnviaTexto("ERRO comando\n");
        return;
    }
    EnviaTexto("OK\n");
}

void Tarefas_Comunicacao(void)
{
    if (SCI_getRxStatus(SCIA_BASE) & SCI_RXSTATUS_ERROR)
    {
        SCI_performSoftwareReset(SCIA_BASE);      // erro de recepcao: recomeca
        nLinha = 0U;
    }
    while (SCI_getRxFIFOStatus(SCIA_BASE) != SCI_FIFO_RX0)
    {
        char ch = (char)(SCI_readCharNonBlocking(SCIA_BASE) & 0xFFU);
        if (ch == '\r' || ch == '\n')
        {
            if (nLinha > 0U)
            {
                linha[nLinha] = '\0';
                Executa(linha);
                nLinha = 0U;
            }
        }
        else if (nLinha < TAM_LINHA - 1U)
        {
            linha[nLinha++] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : ch;  // maiusculas
        }
    }
}
