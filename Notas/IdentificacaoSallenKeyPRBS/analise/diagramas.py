"""Diagramas dos ensaios do filtro Sallen-Key com a LaunchPad LAUNCHXL-F28379D.

Gera três figuras em ../imgs/:
  diagrama_degrau.png         — ensaio ao degrau (SK01Degrau)
  diagrama_prbs.png           — ensaio PRBS (SK02PRBS)
  diagrama_identificacao.png  — fluxo da identificação ARX

Imagens de ../imgs/fontes/:
  launchpad-f28379d.png, ccs-debug.jpg  — Texas Instruments (ti.com)
  pocketlab-foto.jpg, sallenkey-circuito.png — TCC de Raissa S. T. Paiva (UFPA, 2025)

Uso:
  python diagramas.py
"""

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

import identifica_arx as ia
import plota_degrau as pdg
import plota_prbs as pp

AQUI = Path(__file__).resolve().parent
FONTES = AQUI.parent / "imgs" / "fontes"
SAIDA = AQUI.parent / "imgs"
DADOS = AQUI.parent / "dados"

AZUL, VERMELHO, VERDE, CINZA = "#1F3A5F", "#C0392B", "#2E8B57", "#5F6B7A"
FUNDO_BLOCO = "#EEF2F7"


# ---------------------------------------------------------------------------
# Primitivas
# ---------------------------------------------------------------------------

def nova_figura():
    fig = plt.figure(figsize=(16, 9))
    ax = fig.add_axes((0, 0, 1, 1))
    ax.set_xlim(0, 1); ax.set_ylim(0, 1); ax.axis("off")
    return fig, ax


def imagem(fig, caminho, x, y, largura, rotulo=None, borda=None):
    """Coloca uma imagem com o canto inferior esquerdo em (x, y), largura em fração da figura."""
    img = plt.imread(caminho)
    h_px, w_px = img.shape[:2]
    W, H = fig.get_size_inches()
    altura = largura * W / H * h_px / w_px
    a = fig.add_axes((x, y, largura, altura))
    a.imshow(img); a.set_xticks([]); a.set_yticks([])
    for s in a.spines.values():
        s.set_visible(borda is not None)
        if borda:
            s.set_edgecolor(borda); s.set_linewidth(1.5)
    if rotulo:
        fig.text(x + largura / 2, y - 0.012, rotulo, ha="center", va="top", fontsize=12, color=CINZA)
    return (x, y, largura, altura)


def bloco(ax, x, y, w, h, titulo, texto="", cor=AZUL, fs=12):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.004,rounding_size=0.01",
                                fc=FUNDO_BLOCO, ec=cor, lw=1.8))
    if texto:
        ax.text(x + w / 2, y + h - 0.012, titulo, ha="center", va="top", fontsize=fs + 1,
                fontweight="bold", color=cor)
        ax.text(x + w / 2, y + h / 2 - 0.012, texto, ha="center", va="center", fontsize=fs, color="#222",
                linespacing=1.35)
    else:
        ax.text(x + w / 2, y + h / 2, titulo, ha="center", va="center", fontsize=fs + 1,
                fontweight="bold", color=cor)
    return (x, y, w, h)


def seta(ax, p0, p1, cor=CINZA, texto=None, lado=0.0, fs=11, estilo="-|>", lw=2.0, conexao="arc3", **kw):
    ax.add_patch(FancyArrowPatch(p0, p1, arrowstyle=estilo, mutation_scale=18, color=cor, lw=lw,
                                 connectionstyle=conexao, **kw))
    if texto:
        xm, ym = (p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2
        ax.text(xm, ym + lado, texto, ha="center", va="center", fontsize=fs, color=cor,
                bbox=dict(fc="white", ec="none", pad=1.5))


def mini_sinal(fig, x, y, w, h, t, s, cor, titulo, degrau=True):
    a = fig.add_axes((x, y, w, h))
    if degrau:
        a.step(t, s, where="post", color=cor, lw=1.4)
    else:
        a.plot(t, s, color=cor, lw=1.4)
    a.set_xticks([]); a.set_yticks([])
    for sp in a.spines.values():
        sp.set_edgecolor(cor); sp.set_linewidth(1)
    a.set_title(titulo, fontsize=11, color=cor, pad=3)
    a.set_facecolor("white")
    return a


def titulo(ax, texto, sub):
    ax.text(0.02, 0.965, texto, fontsize=22, fontweight="bold", color=AZUL, va="top")
    ax.text(0.02, 0.915, sub, fontsize=13, color=CINZA, va="top")


def creditos(ax):
    ax.text(0.99, 0.008, "Imagens: Texas Instruments (LaunchPad, CCS); Paiva (2025), TCC UFPA (Pocket-Lab, circuito)",
            ha="right", va="bottom", fontsize=9, color=CINZA)


# ---------------------------------------------------------------------------
# Diagrama de ensaio (degrau ou PRBS)
# ---------------------------------------------------------------------------

def diagrama_ensaio(tipo):
    fig, ax = nova_figura()
    if tipo == "degrau":
        d = pdg.le_csv_ccs(DADOS / "DadosRespostaDegrau.csv")
        u, y = d["uV"], d["yV"]
        t = np.arange(len(u)) * pdg.TS
        trocas = np.where(np.abs(np.diff(u)) > 1e-6)[0] + 1
        meio = int(np.min(np.diff(trocas))) * pdg.TS
        titulo(ax, "Ensaio ao degrau — projeto CCS SK01Degrau",
               f"Onda quadrada {u.min():.1f} ↔ {u.max():.1f} V · {meio:.2f} s por nível · Ts = 5 ms · "
               f"{len(u)} amostras · saída: DadosRespostaDegrau.csv".replace(".", ",").replace("DadosRespostaDegrau,csv", "DadosRespostaDegrau.csv"))
        lei = "u(k) = u0 + A q(k),  q = ±1"
        arq, script = "DadosResposta\nDegrau.csv", "plota_degrau.py"
        janela = slice(0, len(t))
        fw = dict(arquivos="main.c (parâmetros) · placa.c · ensaio.c", isr="LacoAmostragem (a cada Ts)",
                  laco="y = ADC2y()  →  u = Degrau(k)  →  Grava(u, y)  →  DAC2u(u)", vetores="uV, yV",
                  ts="Ts = 5 ms")
    else:
        t, u, y = pp.le_prbs(DADOS / "DadosPRBS1ms.csv")
        titulo(ax, "Ensaio PRBS — projeto CCS SK02PRBS",
               f"PRBS 1,0 ± 0,4 V · LFSR 8 bits (255 bits) · Tb = 5 Ts = 5 ms · Ts = 1 ms · {len(u)} amostras · "
               "saída: DadosPRBS1ms.csv")
        lei = "LFSR 8 bits, novo bit a cada 5 amostras\nu = 1,0 ± 0,4 V"
        arq, script = "DadosPRBS1ms.csv", "plota_prbs.py"
        janela = slice(0, 400)
        fw = dict(arquivos="main.c (parâmetros) · placa.c · ensaio.c", isr="LacoAmostragem (a cada Ts)",
                  laco="y = ADC2y()  →  u = PRBS(k)  →  Grava(u, y)  →  DAC2u(u)", vetores="uV, yV",
                  ts="Ts = 1 ms")

    # --- linha de cima: hardware -----------------------------------------
    ccs = imagem(fig, FONTES / "ccs-debug.jpg", 0.02, 0.565, 0.24, borda=CINZA)
    ax.text(ccs[0] + ccs[2] / 2, ccs[1] + ccs[3] + 0.015, "Computador · Code Composer Studio",
            ha="center", fontsize=13, fontweight="bold", color=AZUL)
    ax.text(ccs[0] + ccs[2] / 2, ccs[1] - 0.012,
            f"compila, grava e depura o firmware;\nlê {fw['vetores']} na memória",
            ha="center", va="top", fontsize=11, color=CINZA)

    lp = imagem(fig, FONTES / "launchpad-f28379d.png", 0.335, 0.55, 0.27)
    ax.text(lp[0] + lp[2] / 2, lp[1] + lp[3] + 0.015, "LaunchPad LAUNCHXL-F28379D",
            ha="center", fontsize=13, fontweight="bold", color=AZUL)
    seta(ax, (ccs[0] + ccs[2] + 0.006, 0.69), (lp[0] + 0.004, 0.69), cor=AZUL, estilo="<|-|>")
    ax.text((ccs[0] + ccs[2] + lp[0]) / 2, 0.715, "USB\nXDS100v2", ha="center", va="bottom", fontsize=10.5,
            color=AZUL)

    foto = imagem(fig, FONTES / "pocketlab-foto.jpg", 0.795, 0.56, 0.105)
    ax.text(foto[0] + foto[2] / 2, foto[1] + foto[3] + 0.015, "Filtro Sallen-Key (Pocket-Lab)",
            ha="center", fontsize=13, fontweight="bold", color=AZUL)

    # --- linha de baixo: firmware -------------------------------------------
    ax.text(0.465, 0.475, f"Firmware (C + driverlib): {fw['arquivos']}",
            ha="center", fontsize=12, color=CINZA, style="italic")
    ax.plot([lp[0] + lp[2] / 2] * 2, [lp[1] - 0.005, 0.50], color=CINZA, lw=1, ls=":")
    bx, bw, bh = 0.315, 0.145, 0.105
    b_tim = bloco(ax, bx, 0.345, bw, bh, "CPU Timer 0", fw["ts"] + "\nTINT0 dispara o ADC", fs=11)
    b_adc = bloco(ax, bx + bw + 0.01, 0.345, bw, bh, "ADC-A · SOC0", "ADCINA3 (J3.26)\n12 bits · 0–3,0 V",
                  cor=VERMELHO, fs=11)
    b_isr = bloco(ax, bx, 0.175, 2 * bw + 0.01, 0.135, fw["isr"],
                  fw["laco"] + "\n" + lei.replace("\n", "  ·  "), fs=10.5)
    b_dac = bloco(ax, bx + bw + 0.01, 0.035, bw, 0.105, "DAC-A", "DACOUTA (J3.30)\n12 bits · 0–3,0 V",
                  fs=11)
    seta(ax, (b_tim[0] + bw + 0.001, 0.3975), (b_adc[0] - 0.001, 0.3975), cor=CINZA)
    seta(ax, (b_adc[0] + bw / 2, b_adc[1]), (b_adc[0] + bw / 2, b_isr[1] + b_isr[3]), cor=CINZA)
    seta(ax, (b_dac[0] + bw / 2, b_isr[1]), (b_dac[0] + bw / 2, b_dac[1] + b_dac[3]), cor=CINZA)

    # --- planta: circuito ---------------------------------------------------
    circ = imagem(fig, FONTES / "sallenkey-circuito.png", 0.745, 0.17, 0.235, borda=CINZA)
    ax.text(circ[0] + circ[2] / 2, circ[1] - 0.012,
            "R₁ = 1 kΩ · R₂ = 10 kΩ · C₁ = C₂ = 3,3 µF\npassa-baixas, ganho unitário",
            ha="center", va="top", fontsize=11, color=CINZA)
    ax.plot([foto[0] + foto[2] / 2] * 2, [foto[1] - 0.005, circ[1] + circ[3] + 0.005], color=CINZA, lw=1, ls=":")

    # sinais: u (DAC → Vi) e y (Vo → ADC), com os dados medidos
    xs, ws, hs = 0.63, 0.095, 0.085
    mini_sinal(fig, xs, 0.045, ws, hs, t[janela], u[janela], AZUL, "u (DAC → Vᵢ)")
    mini_sinal(fig, xs, 0.355, ws, hs, t[janela], y[janela], VERMELHO, "y (V₀ → ADC)", degrau=False)
    vi = (circ[0] - 0.004, circ[1] + 0.33 * circ[3])                 # borda esquerda, altura de Vᵢ
    vo = (circ[0] + 0.93 * circ[2], circ[1] + circ[3] + 0.006)        # acima do terminal V₀
    seta(ax, (b_dac[0] + bw + 0.002, 0.0875), (xs - 0.003, 0.0875), cor=AZUL, lw=2.5)
    seta(ax, (xs + ws + 0.003, 0.0875), vi, cor=AZUL, lw=2.5, conexao="arc3,rad=0.35")
    seta(ax, vo, (xs + ws + 0.003, 0.3975), cor=VERMELHO, lw=2.5, conexao="arc3,rad=0.55")
    seta(ax, (xs - 0.003, 0.3975), (b_adc[0] + bw + 0.002, 0.3975), cor=VERMELHO, lw=2.5)

    # --- dados e análise ------------------------------------------------------
    b_arq = bloco(ax, 0.02, 0.175, 0.125, 0.135, arq, fw["vetores"] + "\nexportados\npelo CCS", cor=VERDE, fs=10.5)
    b_py = bloco(ax, 0.16, 0.175, 0.125, 0.135, "Python", f"analise/\n{script}\ngráficos e métricas",
                 cor=VERDE, fs=10.5)
    seta(ax, (b_arq[0] + 0.0625, ccs[1] - 0.075), (b_arq[0] + 0.0625, b_arq[1] + b_arq[3] + 0.003), cor=VERDE)
    ax.text(b_arq[0] + 0.0625 + 0.01, (ccs[1] - 0.075 + b_arq[1] + b_arq[3]) / 2, "exportar CSV",
            ha="left", va="center", fontsize=10.5, color=VERDE)
    seta(ax, (b_arq[0] + b_arq[2] + 0.001, 0.2425), (b_py[0] - 0.001, 0.2425), cor=VERDE)
    ax.text(0.1525, 0.13, "Notas/IdentificacaoSallenKeyPRBS/dados/", ha="center", fontsize=10, color=CINZA)

    creditos(ax)
    return fig


# ---------------------------------------------------------------------------
# Diagrama da identificação
# ---------------------------------------------------------------------------

def diagrama_identificacao():
    fig, ax = nova_figura()
    titulo(ax, "Identificação ARX — do ensaio PRBS ao modelo validado",
           "mínimos quadrados · 18 estruturas ARX(na, nb, nk) · validação por simulação livre, resíduos e "
           "ensaio ao degrau")

    d = ia.prepara(DADOS / "DadosPRBS1ms.csv")
    res = ia.varre(d)
    r_ = {r["nome"]: r for r in res}
    esc = ia.escolhe(res)
    m = esc["modelo"]

    # origem dos dados
    imagem(fig, FONTES / "launchpad-f28379d.png", 0.02, 0.735, 0.14)
    imagem(fig, FONTES / "pocketlab-foto.jpg", 0.165, 0.73, 0.06)
    ax.text(0.125, 0.715, "ensaio PRBS (SK02PRBS)", ha="center", va="top", fontsize=11, color=CINZA)

    # 1. dados
    b1 = bloco(ax, 0.02, 0.43, 0.205, 0.18, "1 · Dados",
               f"DadosPRBS1ms.csv · {len(d.t)} amostras\nũ = u − 1,0 V · ỹ = y − y_op\n70 % identificação\n"
               "30 % validação", cor=VERDE, fs=11)
    a = fig.add_axes((0.03, 0.23, 0.185, 0.16))
    a.plot(d.t, d.y, color=VERMELHO, lw=0.8)
    a.axvspan(d.t[0], d.t[d.n_ident], color=AZUL, alpha=0.12)
    a.axvspan(d.t[d.n_ident], d.t[-1], color=VERMELHO, alpha=0.12)
    a.set_xticks([]); a.set_yticks([]); a.set_title("ident. | valid.", fontsize=10, color=CINZA, pad=2)
    seta(ax, (0.125, 0.68), (0.125, b1[1] + b1[3] + 0.005), cor=VERDE)

    # 2. estrutura + mínimos quadrados
    b2 = bloco(ax, 0.265, 0.43, 0.215, 0.18, "2 · Estimação",
               "ỹ(k) = Σ aᵢ ỹ(k−i) + Σ bⱼ ũ(k−nₖ−j+1)\nθ̂ = (ΦᵀΦ)⁻¹ Φᵀ y\n"
               "na ∈ {1, 2} · nb ∈ {1, 2, 3}\nnk ∈ {0, 1, 2}  →  18 modelos", fs=11)
    seta(ax, (b1[0] + b1[2], 0.52), (b2[0], 0.52), cor=CINZA)

    # 3. avaliação: mapa de FIT (nk = 1)
    b3 = bloco(ax, 0.52, 0.43, 0.215, 0.18, "3 · Avaliação (validação)",
               "FIT da simulação livre\nAIC · resíduos r_ee e r_ue\n(banda ±1,96/√N)", fs=11)
    seta(ax, (b2[0] + b2[2], 0.52), (b3[0], 0.52), cor=CINZA)
    a = fig.add_axes((0.56, 0.135, 0.15, 0.255))
    linhas = [(na, nb) for na in ia.GRADE_NA for nb in ia.GRADE_NB]
    M = np.array([[r_[f"ARX({na},{nb},{nk})"]["FIT_sim_val"] if r_[f"ARX({na},{nb},{nk})"]["estavel"] else np.nan
                   for nk in ia.GRADE_NK] for na, nb in linhas])
    a.imshow(np.clip(M, 60, 100), cmap="Blues", vmin=60, vmax=100, aspect="auto")
    for i_, (na, nb) in enumerate(linhas):
        for j_, nk in enumerate(ia.GRADE_NK):
            r = r_[f"ARX({na},{nb},{nk})"]
            if not r["estavel"]:
                txt, cor = "instável", "black"
            else:
                txt = f"{r['FIT_sim_val']:.1f}".replace(".", ",")
                cor = "white" if r["FIT_sim_val"] > 88 else "black"
            a.text(j_, i_, txt, ha="center", va="center", fontsize=8.5, color=cor,
                   fontweight="bold" if r["nome"] == esc["nome"] else "normal")
    a.set_xticks(range(3), [f"nk={k}" for k in ia.GRADE_NK], fontsize=8.5)
    a.set_yticks(range(len(linhas)), [f"({na},{nb})" for na, nb in linhas], fontsize=8.5)
    a.set_ylabel("(na, nb)", fontsize=9)
    a.set_title("FIT da simulação livre na validação (%)", fontsize=9.5, color=CINZA, pad=3)

    # 4. escolha
    b4 = bloco(ax, 0.775, 0.43, 0.205, 0.18, "4 · Escolha",
               "FIT ≥ melhor − 1 ponto\nnk = 1 (firmware)\nmenor AIC\n"
               f"→ {esc['nome']}", cor=VERMELHO, fs=11)
    seta(ax, (b3[0] + b3[2], 0.52), (b4[0], 0.52), cor=CINZA)

    # modelo escolhido
    tau1, tau2 = m.constantes_de_tempo(d.Ts)
    coef = (f"ỹ(k) = {m.a[0]:.4f} ỹ(k−1) − {-m.a[1]:.4f} ỹ(k−2)\n"
            f"        + {m.b[0]:.4f} ũ(k−1) + {m.b[1]:.4f} ũ(k−2)").replace(".", ",")
    ax.text(0.8775, 0.385, coef, ha="center", va="top", fontsize=10.5, family="DejaVu Sans Mono", color="#222")
    ax.text(0.8775, 0.325, f"τ₁ = {tau1 * 1e3:.1f} ms · τ₂ = {tau2 * 1e3:.1f} ms · K = {m.ganho_estatico:.3f}"
            .replace(".", ","), ha="center", va="top", fontsize=12, color=AZUL, fontweight="bold")
    ax.text(0.8775, 0.285, f"FIT validação = {esc['FIT_sim_val']:.1f} %".replace(".", ","),
            ha="center", va="top", fontsize=12, color=AZUL)

    # 5. validação cruzada com o degrau
    dd = pdg.le_csv_ccs(DADOS / "DadosRespostaDegrau.csv")
    yD, uD = dd["yV"], dd["uV"]
    tD = np.arange(len(yD)) * pdg.TS
    yS = ia.simula_tf(ia.reamostra(m, d.Ts, pdg.TS), uD - uD[0]) + yD[:pdg.K_DEGRAU].mean()   # ensaio em 5 ms
    fitD = ia.fit(yD[pdg.K_DEGRAU:], yS[pdg.K_DEGRAU:])
    a = fig.add_axes((0.79, 0.06, 0.18, 0.15))
    a.plot(tD, yD, ".", color=VERMELHO, ms=2.5)
    a.plot(tD, yS, color=AZUL, lw=1.5)
    a.set_xlim(0.15, 0.6); a.set_xticks([]); a.set_yticks([])
    a.set_title(f"5 · validação cruzada no degrau: FIT {fitD:.1f} %".replace(".", ","), fontsize=10,
                color=AZUL, pad=2)

    # 1 polo × 2 polos; ARX × OE
    fit1 = max(r["FIT_sim_val"] for r in res if r["na"] == 1 and r["estavel"])
    oe = ia.estima_oe(m.na, m.nb, m.nk, d)
    t1o, t2o = oe.constantes_de_tempo(d.Ts)
    ax.text(0.375, 0.37,
            f"1 polo: FIT ≤ {fit1:.1f} %  ·  2 polos: até {max(r['FIT_sim_val'] for r in res):.1f} %\n"
            "→ com Ts = 1 ms o polo rápido aparece".replace(".", ","),
            ha="center", va="top", fontsize=11, color="#222", linespacing=1.5)
    ax.text(0.375, 0.24,
            f"Resíduos ARX: r_ee(1) ≈ −0,6 (ruído filtrado por A(q))\n"
            f"OE{m.na, m.nb, m.nk}: τ = {t1o * 1e3:.1f} e {t2o * 1e3:.1f} ms → confirma".replace(".", ",")
            .replace("(2, 2, 1)", "(2,2,1)"),
            ha="center", va="top", fontsize=11, color=VERMELHO, style="italic", linespacing=1.5)

    creditos(ax)
    return fig


def main():
    SAIDA.mkdir(exist_ok=True)
    for nome, fig in (("diagrama_degrau", diagrama_ensaio("degrau")),
                      ("diagrama_prbs", diagrama_ensaio("prbs")),
                      ("diagrama_identificacao", diagrama_identificacao())):
        fig.savefig(SAIDA / f"{nome}.png", dpi=150, facecolor="white")
        plt.close(fig)
        print(f"salvo: imgs/{nome}.png")


if __name__ == "__main__":
    main()
