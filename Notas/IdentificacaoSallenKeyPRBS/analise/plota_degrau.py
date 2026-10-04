"""Gráfico do ensaio ao degrau do filtro Sallen-Key (projeto CCS SK01Degrau).

Lê y_buf (e, opcionalmente, u_buf) exportados do CCS e compara com a resposta
do modelo analítico H(s).

Formatos aceitos (detectados pela extensão/conteúdo):
  .dat  — Memory Browser → Save Memory, formato "32-bit Hex - TI Style"
          (cabeçalho "1651 ..." e uma palavra 0xXXXXXXXX por linha = float32);
  .json — exportação da janela Expressions, se contiver os valores do vetor;
  .csv  — exportação de variáveis do CCS: blocos lado a lado
          ("y_buf,,,u_buf," / "Sample,Value,,Sample,Value");
  .txt  — um valor por linha (ou separados por vírgula/espaço).

Uso:
  python plota_degrau.py ../dados/DadosDegrau.csv [--salvar ../imgs/DadosDegrau.png]
  python plota_degrau.py ../dados/y_buf.dat [--u ../dados/u_buf.dat]
"""

import argparse
import json
import re
import struct
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

# Parâmetros do firmware (ProjetosSK/SK01Degrau/perifericos.h e controle.h)
TS = 0.005          # s
N_AMOSTRAS = 300
K_DEGRAU = 40
U_BAIXO, U_ALTO = 0.6, 1.4   # V

# Filtro Sallen-Key (pocket-lab do TCC FiltroSK)
R1, R2, C1, C2 = 1e3, 10e3, 3.3e-6, 3.3e-6

NUM = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")


def le_dat_ti(caminho):
    linhas = Path(caminho).read_text().split()
    palavras = [p for p in linhas[1:] if p.lower().startswith("0x")]  # pula o cabeçalho 1651
    return np.array([struct.unpack(">f", struct.pack(">I", int(p, 16)))[0] for p in palavras])


def le_json(caminho, nome="y_buf"):
    dados = json.loads(Path(caminho).read_text())
    itens = dados if isinstance(dados, list) else [dados]
    for item in itens:
        if item.get("e") != nome:
            continue
        if "children" in item:
            return np.array([float(c["v"]) for c in item["children"]])
        if "v" in item:
            return np.array([float(x) for x in NUM.findall(str(item["v"]))])
    raise ValueError(
        f"'{nome}' sem valores em {caminho}: a janela Expressions exporta só os nomes. "
        "Use Memory Browser → Save Memory (ver docstring)."
    )


def le_csv_ccs(caminho):
    """CSV do CCS: 1ª linha com os nomes das variáveis, 2ª com Sample/Value."""
    linhas = [l.rstrip("\n").split(",") for l in Path(caminho).read_text().splitlines()]
    nomes = {i: n.strip() for i, n in enumerate(linhas[0]) if n.strip()}
    vetores = {}
    for i, nome in nomes.items():  # a coluna Value fica logo à direita do nome
        vetores[nome] = np.array([float(l[i + 1]) for l in linhas[2:] if len(l) > i + 1 and l[i + 1]])
    # nomes dos firmwares novos (uV, yV) e antigos (u_buf, y_buf): os dois ficam disponíveis
    for novo, antigo in (("uV", "u_buf"), ("yV", "y_buf"), ("rV", "r_buf")):
        if novo in vetores and antigo not in vetores:
            vetores[antigo] = vetores[novo]
        elif antigo in vetores and novo not in vetores:
            vetores[novo] = vetores[antigo]
    return vetores


def le_vetor(caminho, nome="y_buf"):
    ext = Path(caminho).suffix.lower()
    if ext == ".csv" and Path(caminho).read_text().startswith(("y_buf", "u_buf", "yV", "uV")):
        return le_csv_ccs(caminho)[nome]
    if ext == ".dat":
        return le_dat_ti(caminho)
    if ext == ".json":
        return le_json(caminho, nome)
    return np.array([float(x) for x in NUM.findall(Path(caminho).read_text())])


def resposta_analitica(t, u):
    """Integra H(s) = w0^2/(s^2 + 2 xi w0 s + w0^2) com u constante em cada Ts (RK4)."""
    w0 = 1 / np.sqrt(R1 * R2 * C1 * C2)
    a1 = 1 / (R1 * C1) + 1 / (R2 * C2)
    f = lambda x, uk: np.array([x[1], w0**2 * (uk - x[0]) - a1 * x[1]])
    x = np.array([u[0], 0.0])
    n_sub = 50
    h = (t[1] - t[0]) / n_sub
    y = np.empty_like(t)
    for k, uk in enumerate(u):
        y[k] = x[0]
        for _ in range(n_sub):
            k1 = f(x, uk); k2 = f(x + h / 2 * k1, uk)
            k3 = f(x + h / 2 * k2, uk); k4 = f(x + h * k3, uk)
            x = x + h / 6 * (k1 + 2 * k2 + 2 * k3 + k4)
    return y


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("y", help="arquivo com y_buf (no CSV do CCS, u_buf também é lido)")
    ap.add_argument("--u", help="arquivo com u_buf (se omitido, reconstrói o degrau do firmware)")
    ap.add_argument("--salvar", help="salva a figura (png/pdf) em vez de abrir a janela")
    args = ap.parse_args()

    y = le_vetor(args.y, "y_buf")[:N_AMOSTRAS]
    if args.u:
        u = le_vetor(args.u, "u_buf")[: len(y)]
    elif Path(args.y).suffix.lower() == ".csv" and "u_buf" in Path(args.y).read_text().splitlines()[0]:
        u = le_vetor(args.y, "u_buf")[: len(y)]
    else:
        u = np.where(np.arange(len(y)) < K_DEGRAU, U_BAIXO, U_ALTO)
    t = np.arange(len(y)) * TS
    y_mod = resposta_analitica(t, u)
    r2 = 1 - np.sum((y - y_mod) ** 2) / np.sum((y - y.mean()) ** 2)
    print(f"R² (medido x modelo analítico) = {r2:.4f}")

    fig, ax = plt.subplots(figsize=(10, 5))
    ax.step(t, u, where="post", color="#1F3A5F", lw=1.5, label="u (DAC)")
    ax.plot(t, y, ".", color="#C0392B", ms=4, label="y medido (ADC)")
    ax.plot(t, y_mod, "--", color="0.3", lw=1.2, label=f"y modelo analítico ($R^2$ = {r2:.3f})")
    ax.set_xlabel("t (s)")
    ax.set_ylabel("tensão (V)")
    ax.set_title(f"Ensaio ao degrau — Sallen-Key ($T_s$ = {TS*1e3:.0f} ms, {len(y)} amostras)")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()

    if args.salvar:
        Path(args.salvar).parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(args.salvar, dpi=150)
        print(f"figura salva em {args.salvar}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
