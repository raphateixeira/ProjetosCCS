"""Figuras e animacoes da apresentacao "Inversor em Malha Aberta" (VSI01).

Gera em ../imgs/:
  circuito.png        inversor trifasico com a carga RL em estrela
  braco.png           os tres estados de um braco (cima, baixo, tempo morto)
  anim_pwm.gif        contador, comparacao e saidas A/B formando o PWM
  tempo_morto.png     zoom numa borda: as duas saidas em nivel baixo por 100 ns
  modos01.png         o que o osciloscopio mostra nos modos 0 e 1
  modo2.png           modo 2: razoes ciclicas e o PWM da fase A em 5 ms/div
  anim_modo2.gif      modo 2: as tres razoes ciclicas girando
  etapaB.png          simulacao com a carga RL a 12 V (modo 2)
  montagem_*.png      montagem das placas (a partir das figuras da TI)

Uso: python3 figuras.py   (precisa de numpy, matplotlib e Pillow)
Os numeros impressos no fim sao os valores esperados na bancada (slides).
"""

from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter
from matplotlib.patches import Rectangle, FancyBboxPatch, Circle
from PIL import Image

AQUI = Path(__file__).resolve().parent
IMGS = AQUI.parent / "imgs"
TI = AQUI.parent.parent / "ControleCorrenteVSI"          # figuras recortadas dos PDFs da TI
IMGS.mkdir(exist_ok=True)

# Cores do tema (TemaRTx): fases A, B, C e destaques
COR_A, COR_B, COR_C = "#1B365D", "#B23A48", "#2E7D32"
COR_ACENTO, COR_AMARELO, COR_CINZA = "#4A6FA5", "#E8C547", "#6C757D"
plt.rcParams.update({"font.size": 15, "axes.titlesize": 16, "axes.labelsize": 15,
                     "font.family": "sans-serif", "axes.spines.top": False,
                     "axes.spines.right": False})

# Parametros do VSI01 (iguais ao main.c)
F_RELOGIO = 100e6          # Hz
F_PWM = 20e3               # Hz
TOPO = int(F_RELOGIO / (2 * F_PWM))      # 2500
T_MORTO = 100e-9           # s
VDC = 12.0                 # barramento da etapa B [V]
R, L = 4.7, 1e-3           # carga por fase


def salva(fig, nome):
    fig.savefig(IMGS / nome, dpi=110, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print("gravado", nome)


# ---------------------------------------------------------------------------
# PWM de um braco: contador triangular, saida A (alta se contador < CMPA) e B
def contador(t):
    """Contador up-down (0 -> TOPO -> 0) no instante t [s]."""
    fase = (t * F_PWM) % 1.0
    return np.where(fase < 0.5, 2 * fase * TOPO, 2 * (1 - fase) * TOPO)


def saidas(t, razao, morto=T_MORTO):
    """Saidas A e B com tempo morto (atraso na subida de cada uma)."""
    cmpa = razao * TOPO
    a_ideal = contador(t) < cmpa
    a_atr = contador(t - morto) < cmpa          # sinal atrasado de 'morto'
    a = a_ideal & a_atr                         # A sobe atrasada, desce na hora
    b = (~a_ideal) & (~a_atr)                   # B = complemento, sobe atrasada
    return a.astype(float), b.astype(float)


# ---------------------------------------------------------------------------
def figura_circuito():
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.set_xlim(0, 12); ax.set_ylim(0, 6.4); ax.axis("off")
    # barramento
    ax.plot([1, 7.6], [5.6, 5.6], color="k", lw=2.5)
    ax.plot([1, 7.6], [0.6, 0.6], color="k", lw=2.5)
    ax.plot([1, 1], [0.6, 2.6], color="k", lw=2); ax.plot([1, 1], [3.6, 5.6], color="k", lw=2)
    ax.plot([0.7, 1.3], [3.6, 3.6], color="k", lw=3); ax.plot([0.85, 1.15], [2.6, 2.6], color="k", lw=3)
    ax.plot([0.7, 1.3], [3.3, 3.3], color="k", lw=3); ax.plot([0.85, 1.15], [2.9, 2.9], color="k", lw=3)
    ax.text(0.2, 3.1, "$V_{dc}$\n12 V", ha="center", va="center")
    ax.text(1.2, 5.85, "+", fontsize=18); ax.text(1.2, 0.15, "GND (−)", fontsize=14)
    cores = [COR_A, COR_B, COR_C]
    xs = [3.0, 4.8, 6.6]
    for k, (x, cor, nome) in enumerate(zip(xs, cores, "ABC")):
        ax.plot([x, x], [5.6, 4.9], color="k", lw=2)
        ax.plot([x, x], [0.6, 1.3], color="k", lw=2)
        for yc, rot in [(4.35, f"{nome}+"), (1.85, f"{nome}−")]:
            ax.add_patch(FancyBboxPatch((x - 0.35, yc - 0.55), 0.7, 1.1,
                                        boxstyle="round,pad=0.02", fc="white", ec=cor, lw=2.5))
            ax.text(x, yc, rot, ha="center", va="center", color=cor, fontsize=15, weight="bold")
        ax.plot([x, x], [3.8, 2.4], color="k", lw=2)
        ys = [3.45, 3.1, 2.75][k]                     # saida de cada braco numa altura
        ax.add_patch(Circle((x, ys), 0.09, color=cor))
        ax.text(x - 0.15, ys + 0.12, nome, color=cor, ha="right", fontsize=16, weight="bold")
        # ligacao do ponto do meio do braco ate a carga (cruza os outros bracos sem conexao)
        yl = [4.6, 3.1, 1.6][k]
        xr = 7.9 - 0.2 * k
        ax.plot([x, xr, xr, 8.2], [ys, ys, yl, yl], color=cor, lw=2)
        # carga R + L
        ax.add_patch(Rectangle((8.2, yl - 0.18), 0.9, 0.36, fc="white", ec=cor, lw=2))
        ax.text(8.65, yl, "4,7 Ω", ha="center", va="center", fontsize=12)
        xx = np.linspace(9.2, 10.1, 200)
        ax.plot(xx, yl + 0.18 * np.abs(np.sin((xx - 9.2) / 0.9 * 4 * np.pi)), color=cor, lw=2)
        ax.text(9.65, yl - 0.45, "1 mH", ha="center", fontsize=12)
        ax.plot([10.1, 10.8], [yl, yl], color=cor, lw=2)
    ax.plot([10.8, 10.8], [1.6, 4.6], color="k", lw=2)
    ax.add_patch(Circle((10.8, 3.1), 0.08, color="k"))
    ax.text(11.0, 3.1, "neutro\n(n)", va="center", fontsize=14)
    ax.text(4.8, 6.15, "inversor (BOOSTXL-3PHGANINV)", ha="center", fontsize=15, color=COR_CINZA)
    ax.text(9.6, 5.35, "carga RL em estrela", ha="center", fontsize=15, color=COR_CINZA)
    salva(fig, "circuito.png")


def figura_braco():
    fig, axs = plt.subplots(1, 3, figsize=(13, 4.6))
    casos = [("chave de cima ligada", True, False, "$v = V_{dc}$ = 12 V"),
             ("chave de baixo ligada", False, True, "$v$ = 0 V"),
             ("tempo morto: as duas desligadas", False, False, "$v$ depende da corrente")]
    for ax, (tit, cima, baixo, rot) in zip(axs, casos):
        ax.set_xlim(0, 4); ax.set_ylim(0, 6); ax.axis("off")
        ax.plot([2, 2], [5.6, 4.8], "k", lw=2); ax.plot([2, 2], [0.4, 1.2], "k", lw=2)
        ax.text(2.15, 5.55, "+12 V", fontsize=14); ax.text(2.15, 0.3, "GND", fontsize=14)
        for yc, lig in [(4.1, cima), (1.9, baixo)]:
            cor = COR_C if lig else COR_CINZA
            ax.add_patch(FancyBboxPatch((1.55, yc - 0.65), 0.9, 1.3, boxstyle="round,pad=0.02",
                                        fc=(cor if lig else "white"), ec=cor, lw=2.5, alpha=0.9))
            ax.text(2, yc, "ligada" if lig else "desligada", ha="center", va="center",
                    color="white" if lig else COR_CINZA, fontsize=12, weight="bold")
        ax.plot([2, 2], [3.45, 2.55], "k", lw=2)
        ax.add_patch(Circle((2, 3.0), 0.09, color=COR_A)); ax.plot([2, 3.4], [3, 3], color=COR_A, lw=2)
        ax.text(3.45, 3.0, "fase", va="center", fontsize=14, color=COR_A)
        ax.set_title(tit, fontsize=15)
        ax.text(2, -0.3, rot, ha="center", fontsize=15)
    axs[0].text(0.1, 4.1, "saída A\n(EPWMxA)", fontsize=12, va="center", color=COR_CINZA)
    axs[0].text(0.1, 1.9, "saída B\n(EPWMxB)", fontsize=12, va="center", color=COR_CINZA)
    fig.text(0.5, -0.06, "Nunca as duas ligadas: seria um curto-circuito no barramento.",
             ha="center", fontsize=16, color=COR_B, weight="bold")
    salva(fig, "braco.png")


def animacao_pwm():
    razao = 0.35
    morto = 2.0e-6                     # exagerado para aparecer no desenho (o real e 100 ns)
    t = np.linspace(0, 2 / F_PWM, 2000)
    cnt = contador(t)
    a, b = saidas(t, razao, morto)
    tus = t * 1e6
    fig, axs = plt.subplots(3, 1, figsize=(11, 6.6), sharex=True,
                            gridspec_kw={"height_ratios": [2.2, 1, 1]})
    axs[0].plot(tus, cnt, color=COR_CINZA, lw=1, alpha=0.4)
    axs[0].axhline(razao * TOPO, color=COR_B, lw=2, ls="--")
    axs[0].text(99, razao * TOPO + 120, f"CMPA = {razao * TOPO:.0f}", color=COR_B, ha="right")
    axs[0].text(99, TOPO + 60, f"TOPO = {TOPO}", ha="right", color=COR_CINZA)
    axs[0].set_ylabel("contador"); axs[0].set_ylim(-100, 2700)
    linha_c, = axs[0].plot([], [], color=COR_A, lw=2.5)
    ponto, = axs[0].plot([], [], "o", color=COR_A, ms=9)
    linha_a, = axs[1].plot([], [], color=COR_A, lw=2.5, drawstyle="steps-post")
    linha_b, = axs[2].plot([], [], color=COR_B, lw=2.5, drawstyle="steps-post")
    for ax, nome in [(axs[1], "A (cima)"), (axs[2], "B (baixo)")]:
        ax.set_ylim(-0.2, 1.3); ax.set_yticks([0, 1]); ax.set_ylabel(nome, fontsize=13)
    axs[2].set_xlabel("tempo [µs]"); axs[2].set_xlim(0, 100)
    txt = axs[0].text(2, 2550, "", fontsize=14)
    fig.suptitle("Contador sobe e desce; A fica alta enquanto contador < CMPA "
                 "(tempo morto exagerado)", fontsize=14)
    fig.tight_layout()
    n_quadros = 80

    def quadro(i):
        k = int((i + 1) / n_quadros * (len(t) - 1))
        linha_c.set_data(tus[:k], cnt[:k]); ponto.set_data([tus[k]], [cnt[k]])
        linha_a.set_data(tus[:k], a[:k]); linha_b.set_data(tus[:k], b[:k])
        estado = ("chave de cima ligada" if a[k] else
                  "chave de baixo ligada" if b[k] else "tempo morto: as duas desligadas")
        txt.set_text(estado)
        return linha_c, ponto, linha_a, linha_b, txt

    anim = FuncAnimation(fig, quadro, frames=n_quadros, interval=80, blit=False)
    anim.save(IMGS / "anim_pwm.gif", writer=PillowWriter(fps=12), dpi=75)
    plt.close(fig)
    print("gravado anim_pwm.gif")


def figura_tempo_morto():
    t = np.linspace(-0.4e-6, 0.6e-6, 4000)
    t0 = 0.35 * TOPO / (2 * TOPO * F_PWM)     # descida de A no instante em que contador = CMPA
    a, b = saidas(t + t0, 0.35)
    fig, ax = plt.subplots(figsize=(11, 4))
    ax.plot(t * 1e9, a * 3.3 + 4.2, color=COR_A, lw=2.5, label="J4-40 (A, chave de cima)")
    ax.plot(t * 1e9, b * 3.3, color=COR_B, lw=2.5, label="J4-39 (B, chave de baixo)")
    ax.axvspan(0, 100, color=COR_AMARELO, alpha=0.4)
    ax.text(115, 5.6, "100 ns: as duas em 0", ha="left", fontsize=14)
    ax.set_xlabel("tempo [ns] (osciloscópio em 50 ns/div)"); ax.set_yticks([])
    ax.legend(loc="lower left", fontsize=13, frameon=False, bbox_to_anchor=(0, -0.45), ncol=2)
    ax.set_ylim(-0.5, 9)
    salva(fig, "tempo_morto.png")


def figura_modos01():
    t = np.linspace(0, 2 / F_PWM, 4000)
    fig, axs = plt.subplots(1, 2, figsize=(13, 4.8), sharey=True)
    for ax, razoes, tit in [(axs[0], (0.5, 0.5, 0.5), "modo 0: 50 % nas três fases"),
                            (axs[1], (0.25, 0.5, 0.75), "modo 1: 25 %, 50 % e 75 %")]:
        for k, (d, cor, nome) in enumerate(zip(razoes, (COR_A, COR_B, COR_C), "ABC")):
            a, _ = saidas(t, d)
            ax.plot(t * 1e6, a * 3.3 + (2 - k) * 4.5, color=cor, lw=2.2, drawstyle="steps-post")
            ax.text(-3, (2 - k) * 4.5 + 1.6, f"{nome}\n{d:.0%}".replace("%", " %"),
                    color=cor, ha="right", va="center", fontsize=14)
        ax.set_title(tit); ax.set_yticks([]); ax.set_xlabel("tempo [µs] (10 µs/div)")
        ax.set_xlim(0, 100)
    salva(fig, "modos01.png")


def figura_modo2():
    indice, f = 0.5, 60.0
    t = np.arange(0, 2 / f, 1 / (F_PWM * 200))
    ang = 2 * np.pi * f * np.floor(t * F_PWM) / F_PWM
    fig, axs = plt.subplots(2, 1, figsize=(12, 6.2), sharex=True)
    for k, (cor, nome) in enumerate(zip((COR_A, COR_B, COR_C), "ABC")):
        d = 0.5 + 0.5 * indice * np.sin(ang - k * 2 * np.pi / 3)
        axs[0].plot(t * 1e3, d, color=cor, lw=2.5, label=f"fase {nome}")
    axs[0].set_ylabel("razão cíclica"); axs[0].set_ylim(0, 1)
    axs[0].legend(ncol=3, frameon=False, loc="upper center", bbox_to_anchor=(0.5, 1.22))
    dA = 0.5 + 0.5 * indice * np.sin(ang)
    cnt = contador(t)
    pwmA = (cnt < dA * TOPO).astype(float)
    axs[1].fill_between(t * 1e3, 0, pwmA, step="post", color=COR_A, lw=0)
    axs[1].plot(t * 1e3, dA, color=COR_AMARELO, lw=3, label="média do pulso = razão cíclica")
    axs[1].set_ylabel("J4-40"); axs[1].set_yticks([0, 1])
    axs[1].set_xlabel("tempo [ms] (5 ms/div): 60 Hz, índice 0,5")
    axs[1].legend(frameon=False, loc="lower right", fontsize=13)
    salva(fig, "modo2.png")


def animacao_modo2():
    indice = 0.5
    angs = np.linspace(0, 2 * np.pi, 49)[:-1]
    x = np.linspace(0, 2 * np.pi, 300)
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4.8), gridspec_kw={"width_ratios": [1, 1.6]})
    cores = (COR_A, COR_B, COR_C)
    barras = ax1.bar(["A", "B", "C"], [0.5] * 3, color=cores)
    ax1.set_ylim(0, 1); ax1.axhline(0.5, color=COR_CINZA, ls="--", lw=1)
    ax1.set_ylabel("razão cíclica"); ax1.set_title("as três fases agora")
    for k, cor in enumerate(cores):
        ax2.plot(np.degrees(x), 0.5 + 0.5 * indice * np.sin(x - k * 2 * np.pi / 3), color=cor, lw=2.5)
    cursor = ax2.axvline(0, color="k", lw=1.5)
    ax2.set_xlim(0, 360); ax2.set_ylim(0, 1); ax2.set_xticks([0, 120, 240, 360])
    ax2.set_xlabel("ângulo [graus] (um ciclo = 1/60 s)"); ax2.set_title("120° entre as fases")
    fig.tight_layout()

    def quadro(i):
        a = angs[i]
        for k, br in enumerate(barras):
            br.set_height(0.5 + 0.5 * indice * np.sin(a - k * 2 * np.pi / 3))
        cursor.set_xdata([np.degrees(a)] * 2)
        return (*barras, cursor)

    anim = FuncAnimation(fig, quadro, frames=len(angs), interval=80)
    anim.save(IMGS / "anim_modo2.gif", writer=PillowWriter(fps=12), dpi=75)
    plt.close(fig)
    print("gravado anim_modo2.gif")


# ---------------------------------------------------------------------------
# Etapa B: simulacao chaveada da carga RL (12 V, modo 2, indice 0,5, 60 Hz)
def simula_carga(indice=0.5, f=60.0, ciclos=3, dt=0.2e-6):
    n = int(ciclos / f / dt)
    t = np.arange(n) * dt
    amostra = np.floor(t * F_PWM)                       # razao ciclica muda a cada periodo
    ang = 2 * np.pi * f * amostra / F_PWM
    cnt = contador(t)
    v_terminal = np.zeros((3, n))
    for k in range(3):
        d = 0.5 + 0.5 * indice * np.sin(ang - k * 2 * np.pi / 3)
        v_terminal[k] = VDC * (cnt < d * TOPO)          # tensao do terminal em relacao ao GND
    v_neutro = v_terminal.mean(axis=0)                  # carga equilibrada em estrela
    v_fase = v_terminal - v_neutro
    a = np.exp(-dt * R / L)
    i = np.zeros((3, n))
    for j in range(1, n):
        i[:, j] = i[:, j - 1] * a + v_fase[:, j - 1] / R * (1 - a)
    # tensao em J3-24: divisor 100k/4,22k com 33 nF (constante de tempo de Thevenin)
    tau_f = (100e3 * 4.22e3 / 104.22e3) * 33e-9
    af = np.exp(-dt / tau_f)
    vf = np.zeros(n)
    for j in range(1, n):
        vf[j] = vf[j - 1] * af + v_terminal[0, j - 1] / 24.7 * (1 - af)
    return t, v_terminal, i, vf


def figura_etapaB():
    t, vt, i, vf = simula_carga()
    m = t >= 1 / 60                                     # descarta o primeiro ciclo (transitorio)
    tm = (t[m] - t[m][0]) * 1e3
    fig, axs = plt.subplots(3, 1, figsize=(12, 7.6), sharex=True)
    axs[0].fill_between(tm, 0, vt[0, m], step="post", color=COR_A, lw=0)
    axs[0].set_ylabel("VA × GND\n[V]"); axs[0].set_yticks([0, 12])
    axs[1].plot(tm, vf[m], color=COR_A, lw=1.5)
    axs[1].set_ylabel("J3-24\n[V]")
    for k, (cor, nome) in enumerate(zip((COR_A, COR_B, COR_C), "ABC")):
        axs[2].plot(tm, i[k, m], color=cor, lw=2, label=f"$i_{nome}$")
    axs[2].set_ylabel("corrente\n[A]"); axs[2].legend(ncol=3, frameon=False, loc="lower center", bbox_to_anchor=(0.5, 0.97))
    axs[2].set_xlabel("tempo [ms] (5 ms/div)")
    sec = axs[2].secondary_yaxis("right", functions=(lambda x: 1.65 - 0.1 * x,
                                                     lambda v: (1.65 - v) / 0.1))
    sec.set_ylabel("J3-27 [V]")
    salva(fig, "etapaB.png")
    pico = i[0, m].max()
    print(f"simulacao: pico de iA = {pico:.3f} A; J3-27 entre {1.65 - 0.1 * pico:.3f} "
          f"e {1.65 + 0.1 * pico:.3f} V; J3-24 entre {vf[m].min():.3f} e {vf[m].max():.3f} V")


# ---------------------------------------------------------------------------
# Montagem: figuras da TI (SPRUI77 fig. 1 e SLUUBP1 fig. 4) com anotacoes
def figuras_montagem():
    # LaunchPad (desenho da TI, recortado em 220 dpi de docs/sprui77.pdf, pagina 3)
    lp = TI / "docs" / "sprui77.pdf"
    if lp.exists():
        import subprocess, tempfile
        with tempfile.TemporaryDirectory() as d:
            subprocess.run(["pdftoppm", "-r", "220", "-png", "-f", "3", "-l", "3",
                            str(lp), f"{d}/p"], check=True)
            im = Image.open(next(Path(d).glob("p*.png")))
            im.crop((340, 250, 1680, 1110)).save(IMGS / "ti_launchpad.png", optimize=True)
            print("gravado ti_launchpad.png")

    # BoosterPack (foto da TI) girado 90 graus no sentido anti-horario: e assim
    # que ele fica sobre a LaunchPad (J1/J3 a esquerda, pino 1 = 3V3 em cima)
    foto = Image.open(TI / "imgs" / "ti_placa_boostxl.png")
    W, H = foto.size
    girada = foto.rotate(90, expand=True)

    def g(x, y):            # coordenada da foto original -> foto girada
        return y, W - x

    fig, ax = plt.subplots(figsize=(7.5, 11))
    ax.imshow(girada); ax.axis("off")
    notas = [((975, 70), "pino 1 de J1 = 3V3\n(lado da USB da LaunchPad)", (-330, -60), COR_B),
             ((855, 90), "J1 (fora) e J3 (dentro)", (-420, 120), COR_A),
             ((855, 575), "J4 (dentro) e J2 (fora)", (210, 120), COR_A),
             ((674, 85), "jumper J6: retirar\n(confira a serigrafia)", (-410, 330), COR_B),
             ((405, 575), "+48V e GND:\nfonte do barramento", (210, -80), COR_C),
             ((600, 575), "VC, VB, VA:\ncarga", (210, 40), COR_C)]
    for (x, y), texto, (dx, dy), cor in notas:
        px, py = g(x, y)
        ax.annotate(texto, xy=(px, py), xytext=(px + dx, py + dy), fontsize=13, color=cor,
                    weight="bold", arrowprops=dict(arrowstyle="->", color=cor, lw=2),
                    bbox=dict(fc="white", ec=cor, alpha=0.9))
    ax.set_title("BOOSTXL visto de cima, na posição de encaixe", fontsize=15)
    salva(fig, "montagem_boostxl.png")


def figura_alinhamento():
    """Esquema (fora de escala) do encaixe: J1..J4 do BoosterPack sobre J1..J4 da LaunchPad."""
    fig, ax = plt.subplots(figsize=(8, 8.6))
    ax.set_xlim(-1, 9); ax.set_ylim(1.4, 13.2); ax.axis("off"); ax.set_aspect("equal")
    ax.add_patch(FancyBboxPatch((0.5, 0), 5, 12.3, boxstyle="round,pad=0.05", fc="#F4C7C7",
                                ec=COR_B, lw=2))
    ax.add_patch(Rectangle((2.5, 12.1), 1, 0.6, fc=COR_CINZA))
    ax.text(3, 12.95, "USB", ha="center", fontsize=13)
    ax.text(3, 1.75, "LaunchPad F28379D (continua para baixo)", ha="center", fontsize=13,
            color=COR_B, weight="bold")
    # colunas de headers: J1, J3 (esquerda) e J4, J2 (direita), pino 1 em cima
    for x, nome in [(1.0, "J1"), (1.45, "J3"), (4.55, "J4"), (5.0, "J2")]:
        for k in range(10):
            ax.add_patch(Circle((x, 10.6 - 0.3 * k), 0.09, color="k"))
        ax.text(x, 10.95, nome, ha="center", fontsize=12, weight="bold")
    for x, nome in [(1.0, "J5"), (1.45, "J7"), (4.55, "J8"), (5.0, "J6")]:
        for k in range(10):
            ax.add_patch(Circle((x, 5.0 - 0.3 * k), 0.07, color=COR_CINZA))
        ax.text(x, 5.35, nome, ha="center", fontsize=11, color=COR_CINZA)
    ax.add_patch(Circle((1.0, 10.6), 0.18, fill=False, ec=COR_B, lw=2.5))
    ax.annotate("pino 1 = 3V3", xy=(1.0, 10.6), xytext=(-0.9, 11.9), fontsize=13, color=COR_B,
                arrowprops=dict(arrowstyle="->", color=COR_B, lw=2))
    # contorno do BoosterPack
    # proporcoes tiradas da foto da TI: a placa comeca na altura do pino 1 e
    # se estende ~1,8" abaixo dos conectores (por cima de J5-J8)
    ax.add_patch(FancyBboxPatch((0.58, 2.5), 4.73, 8.4, boxstyle="round,pad=0.05", fill=False,
                                ec=COR_A, lw=3, ls="--"))
    ax.text(3, 6.5, "BOOSTXL-3PHGANINV\npor cima", ha="center",
            fontsize=14, color=COR_A, weight="bold")
    ax.annotate("os pinos de J1–J4 da LaunchPad\nentram nos conectores fêmea\nda face de baixo do BOOSTXL",
                xy=(5.0, 9.0), xytext=(5.7, 8.6), fontsize=12,
                arrowprops=dict(arrowstyle="->", color="k"))
    ax.annotate("as soldas de baixo do BOOSTXL\nficam sobre J5–J8:\nisolar (fita Kapton)",
                xy=(4.8, 3.6), xytext=(5.7, 2.4), fontsize=12, color=COR_B,
                arrowprops=dict(arrowstyle="->", color=COR_B))
    salva(fig, "montagem_esquema.png")


def figura_pinos_launchpad():
    """Serigrafia da LaunchPad (SPRUI77C, fig. 18) girada com a USB para cima e
    os pinos do PWM marcados. Na figura da TI a placa esta deitada, com o
    XDS100v2 (lado da USB) a esquerda."""
    pdf = TI / "docs" / "sprui77.pdf"
    if not pdf.exists():
        return
    import subprocess, tempfile
    Image.MAX_IMAGE_PIXELS = None
    with tempfile.TemporaryDirectory() as d:
        subprocess.run(["pdftoppm", "-r", "600", "-png", "-f", "18", "-l", "18",
                        str(pdf), f"{d}/p"], check=True)
        pagina = Image.open(next(Path(d).glob("p*.png")))
        serig = pagina.crop((463, 4934, 2507, 5915))          # figura 18 (vista de cima)
    x0, y0, x1, y1 = 275, 20, 836, 867                        # regiao de J1-J4 e do XDS100v2
    rec = serig.crop((x0, y0, x1, y1))
    hc = y1 - y0
    girada = rec.rotate(-90, expand=True)                     # USB para cima
    xc = 0                                                    # a imagem inteira tem os conectores
    girada = girada.crop((xc, 0, 800, girada.size[1]))

    def g(x, y):     # rotulo na figura da TI (pixels) -> figura girada e cortada
        return hc - 1 - (y - y0) - xc, x - x0

    fig, ax = plt.subplots(figsize=(12, 8.6))
    ax.imshow(girada); ax.axis("off")
    k = 1.02         # escala entre a figura vista e a gerada
    W = girada.size[0]
    direita = [((463, 75), "J2-20  GND: terra das pontas", "k"),
               ((463, 200), "J4-40  A, chave de cima", COR_A),
               ((498, 200), "J4-39  A, chave de baixo", COR_A),
               ((533, 200), "J4-38  B, chave de cima", COR_B),
               ((569, 200), "J4-37  B, chave de baixo", COR_B),
               ((604, 200), "J4-36  C, chave de cima", COR_C),
               ((639, 200), "J4-35  C, chave de baixo", COR_C)]
    esquerda = [((463, 810), "J1-1  3V3: não encostar", COR_CINZA),
                ((463, 672), "J3-21  5V: não encostar", COR_CINZA)]
    for lista, xt, ha in [(direita, W + 40, "left"), (esquerda, -40, "right")]:
        for j, ((x, y), texto, cor) in enumerate(lista):
            px, py = g(x * k, y * k)
            yt = 60 + j * 62 if ha == "left" else 60 + j * 80
            ax.add_patch(Circle((px, py), 15, fill=False, ec=cor, lw=2.5))
            ax.annotate(texto, xy=(px, py), xytext=(xt, yt), fontsize=14, color=cor, ha=ha,
                        weight="bold", va="center",
                        arrowprops=dict(arrowstyle="->", color=cor, lw=1.6),
                        bbox=dict(fc="white", ec=cor, alpha=0.95))
    ax.set_xlim(-420, W + 430)
    ax.set_ylim(girada.size[1] + 10, -60)
    ax.text(W / 2, -35, "▲ lado da USB (XDS100v2)", ha="center", fontsize=14, weight="bold")
    ax.text(W / 2, girada.size[1] + 45, "o número impresso fica ao lado do pino",
            ha="center", fontsize=13, color=COR_CINZA)
    salva(fig, "pinos_launchpad.png")


# ---------------------------------------------------------------------------
# Filtro RC para ver a senoide (R = 10 kOhm, C = 100 nF: corte em 159 Hz)
R_FILTRO, C_FILTRO = 10e3, 100e-9


def desenha_rc(ax, x, y_topo, rotulo, cor):
    """Um filtro RC: resistor horizontal saindo de (x, y_topo), capacitor e
    simbolo de terra embaixo (cada RC com o seu proprio terra)."""
    ax.plot([x, x + 0.4], [y_topo, y_topo], color=cor, lw=2)
    ax.add_patch(Rectangle((x + 0.4, y_topo - 0.15), 1.0, 0.3, fc="white", ec=cor, lw=2))
    ax.text(x + 0.9, y_topo + 0.28, "10 kΩ", ha="center", fontsize=12)
    ax.plot([x + 1.4, x + 2.2], [y_topo, y_topo], color=cor, lw=2)
    ax.add_patch(Circle((x + 2.2, y_topo), 0.07, color=cor))
    ax.plot([x + 2.2, x + 2.2], [y_topo, y_topo - 0.4], color=cor, lw=2)
    ax.plot([x + 1.95, x + 2.45], [y_topo - 0.4, y_topo - 0.4], color=cor, lw=3)
    ax.plot([x + 1.95, x + 2.45], [y_topo - 0.58, y_topo - 0.58], color=cor, lw=3)
    ax.text(x + 2.6, y_topo - 0.49, "100 nF", va="center", fontsize=12)
    ax.plot([x + 2.2, x + 2.2], [y_topo - 0.58, y_topo - 0.85], color="k", lw=2)
    for j, meia in enumerate((0.25, 0.16, 0.07)):          # simbolo de terra
        yy = y_topo - 0.85 - 0.08 * j
        ax.plot([x + 2.2 - meia, x + 2.2 + meia], [yy, yy], color="k", lw=2)
    if rotulo:
        ax.annotate(rotulo, xy=(x + 2.2, y_topo), xytext=(x + 2.7, y_topo + 0.3), fontsize=12,
                    color=cor, weight="bold", arrowprops=dict(arrowstyle="->", color=cor))


def figura_rc_esquema():
    fig, axs = plt.subplots(1, 2, figsize=(14, 5.4))
    paineis = [("Etapa A: na LaunchPad (0 a 3,3 V)",
                [("J4-40 (A)", COR_A, 3.6, "ponta CH1"), ("J4-38 (B)", COR_B, 1.9, "ponta CH2")],
                "terra = J2-20 (GND): garras de terra aqui", "k"),
               ("Etapa B: nos bornes do inversor (0 a 12 V), sem a carga RL",
                [("VA", COR_A, 4.1, "ponta CH1"), ("VB", COR_B, 2.6, "ponta CH2"),
                 ("VC", COR_C, 1.1, "")],
                "terra = borne GND: garras de terra só aqui", COR_B)]
    for ax, (tit, linhas, nota, cor_nota) in zip(axs, paineis):
        ax.set_xlim(0, 7); ax.set_ylim(-0.5, 4.8); ax.axis("off")
        ax.set_title(tit, fontsize=15)
        for pino, cor, yt, rot in linhas:
            ax.text(0.1, yt, pino, va="center", fontsize=13, color=cor, weight="bold")
            desenha_rc(ax, 1.4, yt, rot, cor)
        ax.text(0.1, -0.3, nota, fontsize=13, color=cor_nota, weight="bold")
    salva(fig, "rc_esquema.png")


def simula_rc(vpico, indice=0.5, f=60.0, ciclos=3, dt=0.2e-6):
    """Saida do RC para as tres fases (pulsos de 0 a vpico, modo 2)."""
    n = int(ciclos / f / dt)
    t = np.arange(n) * dt
    ang = 2 * np.pi * f * np.floor(t * F_PWM) / F_PWM
    cnt = contador(t)
    a = np.exp(-dt / (R_FILTRO * C_FILTRO))
    pulsos = np.zeros((3, n)); vc = np.zeros((3, n))
    for k in range(3):
        d = 0.5 + 0.5 * indice * np.sin(ang - k * 2 * np.pi / 3)
        pulsos[k] = vpico * (cnt < d * TOPO)
    vc[:, 0] = vpico / 2
    for j in range(1, n):
        vc[:, j] = vc[:, j - 1] * a + pulsos[:, j - 1] * (1 - a)
    return t, pulsos, vc


def figura_rc_simulacao():
    for vpico, nome, tit in [(3.3, "rc_etapaA.png", "Etapa A: J4-40 e saída do RC (índice 0,5, 60 Hz)"),
                             (12.0, "rc_etapaB.png", "Etapa B: saídas dos três RC e a diferença A − B")]:
        t, p, vc = simula_rc(vpico)
        m = t >= 1 / 60
        tm = (t[m] - t[m][0]) * 1e3
        if vpico < 5:
            fig, axs = plt.subplots(2, 1, figsize=(12, 6.2), sharex=True)
            axs[0].fill_between(tm, 0, p[0, m], step="post", color=COR_A, lw=0)
            axs[0].set_ylabel("J4-40 [V]"); axs[0].set_yticks([0, 3.3])
            axs[1].plot(tm, vc[0, m], color=COR_A, lw=1.5, label="CH1: depois do RC (fase A)")
            axs[1].plot(tm, vc[1, m], color=COR_B, lw=1.5, label="CH2: depois do RC (fase B)")
            axs[1].set_ylabel("[V]"); axs[1].legend(frameon=False, ncol=2, loc="upper center",
                                                    bbox_to_anchor=(0.5, 1.2))
            print(f"RC etapa A: {vc[0, m].min():.2f} a {vc[0, m].max():.2f} V")
        else:
            fig, axs = plt.subplots(2, 1, figsize=(12, 6.2), sharex=True)
            for k, (cor, nome_f) in enumerate(zip((COR_A, COR_B, COR_C), "ABC")):
                axs[0].plot(tm, vc[k, m], color=cor, lw=1.5, label=f"V{nome_f} depois do RC")
            axs[0].set_ylabel("[V]"); axs[0].legend(frameon=False, ncol=3, loc="upper center",
                                                    bbox_to_anchor=(0.5, 1.25))
            dif = vc[0, m] - vc[1, m]
            axs[1].plot(tm, dif, color=COR_ACENTO, lw=1.5)
            axs[1].set_ylabel("A − B [V]")
            print(f"RC etapa B: VA {vc[0, m].min():.2f} a {vc[0, m].max():.2f} V; "
                  f"A-B {dif.min():.2f} a {dif.max():.2f} V")
        axs[1].set_xlabel("tempo [ms] (5 ms/div)")
        fig.suptitle(tit, fontsize=15, y=1.02)
        salva(fig, nome)


# ---------------------------------------------------------------------------
def numeros_esperados():
    z = np.hypot(R, 2 * np.pi * 60 * L)
    print("\nvalores esperados (12 V, carga 4,7 ohm + 1 mH):")
    print(f"  TOPO = {TOPO}, contagens de tempo morto = {T_MORTO * F_RELOGIO:.0f}")
    print(f"  modo 1: tensoes de fase -3, 0, +3 V -> iA = {-3 / R:.3f} A, iC = {3 / R:.3f} A;"
          f" J3-27 = {1.65 + 0.1 * 3 / R:.3f} V, J3-29 = {1.65 - 0.1 * 3 / R:.3f} V;"
          f" potencia = {2 * (3 / R) ** 2 * R:.2f} W -> fonte {2 * (3 / R) ** 2 * R / VDC:.2f} A")
    for m in (0.5, 0.8):
        vp = 0.5 * m * VDC
        ip = vp / z
        p = 1.5 * ip ** 2 * R
        print(f"  modo 2, indice {m}: fase {vp:.2f} V de pico, |Z| = {z:.3f}, i = {ip:.3f} A de pico;"
              f" potencia {p:.2f} W -> fonte {p / VDC:.2f} A; J3-27 = 1,65 +- {0.1 * ip:.3f} V")


if __name__ == "__main__":
    figura_circuito()
    figura_braco()
    animacao_pwm()
    figura_tempo_morto()
    figura_modos01()
    figura_modo2()
    animacao_modo2()
    figura_etapaB()
    figuras_montagem()
    figura_alinhamento()
    figura_pinos_launchpad()
    figura_rc_esquema()
    figura_rc_simulacao()
    numeros_esperados()
