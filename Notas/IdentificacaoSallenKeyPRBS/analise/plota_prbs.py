"""Gráfico do ensaio PRBS do filtro Sallen-Key (projeto CCS SK02PRBS).

Lê yV e uV (ou y_buf e u_buf, firmware antigo) do CSV exportado pelo CCS e plota
a entrada PRBS e a resposta medida (ensaio completo e trecho ampliado).

Uso:
  python plota_prbs.py ../dados/DadosPRBS1ms.csv [--salvar ../imgs/DadosPRBS1ms.png]
"""

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from plota_degrau import le_csv_ccs

# Parâmetros do firmware (ProjetosSK/SK02PRBS/main.c): ensaio DadosPRBS1ms.csv
TS = 0.001          # s
N_PRE = 100         # amostras paradas no ponto de operação (tPre = 0,1 s)
N_BIT = 5           # amostras por bit (Tb = 5 ms)
N_PRBS = 255        # bits da PRBS (LFSR de 8 bits)
U_OP, U_AMP = 1.0, 0.4   # V

# Ensaio anterior (DadosPRBS01.csv): Ts = 5 ms, 20 amostras paradas, Tb = 3 Ts = 15 ms
TS_ANTIGO, N_PRE_ANTIGO, N_BIT_ANTIGO = 0.005, 20, 3

AZUL, VERMELHO = "#1F3A5F", "#C0392B"


def le_prbs(caminho, ts=TS):
    d = le_csv_ccs(caminho)
    y, u = d["yV"], d["uV"]
    return np.arange(len(y)) * ts, u, y


def figura_prbs(t, u, y, zoom=(0.0, 0.5), n_pre=N_PRE):
    fig, eixos = plt.subplots(2, 2, figsize=(15, 6.5), sharey="row",
                              gridspec_kw={"width_ratios": [2, 1]})
    for col, (a, b) in enumerate([(t[0], t[-1]), zoom]):
        ax_u, ax_y = eixos[0, col], eixos[1, col]
        ax_u.step(t, u, where="post", color=AZUL, lw=1.2)
        ax_y.plot(t, y, "-", color=VERMELHO, lw=1.2)
        if col == 1:
            ax_y.plot(t, y, ".", color=VERMELHO, ms=3)
        for ax in (ax_u, ax_y):
            ax.set_xlim(a, b)
            ax.grid(alpha=0.3)
            ax.axvline(n_pre * (t[1] - t[0]), color="0.5", ls=":", lw=1)
        ax_y.set_xlabel("t (s)")
    eixos[0, 0].set_ylabel("u (V)")
    eixos[1, 0].set_ylabel("y (V)")
    eixos[0, 0].set_title(f"Ensaio completo ({len(t)} amostras, $T_s$ = {(t[1] - t[0]) * 1e3:.0f} ms)")
    eixos[0, 1].set_title(f"Trecho ampliado ({zoom[0]:g}–{zoom[1]:g} s)")
    fig.tight_layout()
    return fig


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", help="CSV do CCS com yV e uV")
    ap.add_argument("--salvar", help="salva a figura (png/pdf) em vez de abrir a janela")
    args = ap.parse_args()

    t, u, y = le_prbs(args.csv)
    print(f"{len(t)} amostras; u ∈ {{{u.min():.2f}; {u.max():.2f}}} V; "
          f"y ∈ [{y.min():.3f}; {y.max():.3f}] V; média de y após o início da PRBS = {y[N_PRE:].mean():.3f} V")
    fig = figura_prbs(t, u, y)

    if args.salvar:
        Path(args.salvar).parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(args.salvar, dpi=150)
        print(f"figura salva em {args.salvar}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
