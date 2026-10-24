"""Tensoes das fases gravadas pelo VSI01 (vetores tensaoA, tensaoB, tensaoC).

O CCS exporta os vetores da janela Watch num CSV com um bloco por variavel:

    tensaoA,,,tensaoB,,,tensaoC,
    Sample,Value,,Sample,Value,,Sample,Value
    0,6.01,,0,6.03,,0,5.98
    ...

Este script le esse arquivo, monta o eixo de tempo (uma amostra a cada
50 us x DECIMACAO), plota as tres tensoes e a diferenca A - B, e grava um CSV
organizado com as colunas t_ms, vA, vB, vC.

Uso: python3 plota_tensoes.py ../dados/TensoesVSI01.csv [decimacao]
"""

import sys
from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt

PERIODO_PWM = 50e-6        # s (20 kHz), igual ao main.c


def le_csv_ccs(caminho):
    """CSV do CCS: 1a linha com os nomes das variaveis, 2a com Sample/Value."""
    linhas = [l.rstrip("\n").split(",") for l in Path(caminho).read_text().splitlines()]
    nomes = {i: n.strip() for i, n in enumerate(linhas[0]) if n.strip()}
    vetores = {}
    for i, nome in nomes.items():  # a coluna Value fica logo a direita do nome
        vetores[nome] = np.array([float(l[i + 1]) for l in linhas[2:]
                                  if len(l) > i + 1 and l[i + 1]])
    return vetores


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    arquivo = Path(sys.argv[1])
    decimacao = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    v = le_csv_ccs(arquivo)
    vA, vB, vC = v["tensaoA"], v["tensaoB"], v["tensaoC"]
    n = min(len(vA), len(vB), len(vC))
    vA, vB, vC = vA[:n], vB[:n], vC[:n]
    t_ms = np.arange(n) * PERIODO_PWM * decimacao * 1e3

    saida = arquivo.with_name(arquivo.stem + "_organizado.csv")
    np.savetxt(saida, np.column_stack([t_ms, vA, vB, vC]), delimiter=",",
               header="t_ms,vA,vB,vC", comments="", fmt="%.4f")
    print(f"{n} amostras; gravado {saida}")
    for nome, x in (("vA", vA), ("vB", vB), ("vC", vC)):
        print(f"  {nome}: media {x.mean():.2f} V, de {x.min():.2f} a {x.max():.2f} V")

    fig, axs = plt.subplots(2, 1, figsize=(11, 6.5), sharex=True)
    for x, cor, nome in ((vA, "#1B365D", "vA"), (vB, "#B23A48", "vB"), (vC, "#2E7D32", "vC")):
        axs[0].plot(t_ms, x, color=cor, lw=1.5, label=nome)
    axs[0].set_ylabel("tensão da fase × GND [V]")
    axs[0].legend(ncol=3, frameon=False)
    axs[1].plot(t_ms, vA - vB, color="#4A6FA5", lw=1.5)
    axs[1].set_ylabel("vA − vB [V]")
    axs[1].set_xlabel("tempo [ms]")
    fig.tight_layout()
    figura = arquivo.with_suffix(".png")
    fig.savefig(figura, dpi=110)
    print(f"figura: {figura}")
    plt.show()


if __name__ == "__main__":
    main()
