"""Resposta em frequência a partir do ensaio SK06VarreduraSenoidal (seno em degraus).

O firmware grava só y(k) da varredura inteira (8730 amostras). O u(k) é determinístico e é
reconstruído aqui exatamente como o firmware o gera:
  - N_INICIAL amostras em U_OP;
  - para cada frequência i (período N_i amostras): acomodação A_i = max(N_i, N_ACOMODA_MIN)
    amostras e integração M_i = max(2, ceil(N_MEDE_MIN/N_i))·N_i amostras, com
    u = U_OP + U_AMP·sen(2π j/N_i), j = 0, 1, … contado desde o início da frequência.
A demodulação síncrona (lock-in) usa a mesma referência de fase em u(k) e y(k), sobre a janela
de integração (número inteiro de períodos) — o mesmo cálculo que a placa faz.

Uso:
  python varredura.py ../dados/DadosVarredura01.csv
"""

import sys
from pathlib import Path

import numpy as np

# Firmware SK06VarreduraSenoidal (controle.h / controle.c)
TS = 0.005
U_OP, U_AMP = 1.0, 0.3
N_INICIAL, N_ACOMODA_MIN, N_MEDE_MIN = 400, 100, 400
N_PERIODO = [400, 200, 100, 50, 40, 30, 25, 20, 16, 12, 10, 8, 6, 5, 4]


def agenda():
    """Lista de (N, início, fim_acomodação, fim) em índices de amostra, por frequência."""
    out, k = [], N_INICIAL
    for N in N_PERIODO:
        A = max(N, N_ACOMODA_MIN)
        M = max(2, -(-N_MEDE_MIN // N)) * N
        out.append((N, k, k + A, k + A + M))
        k += A + M
    return out


def n_total():
    return agenda()[-1][3]


def reconstroi_u():
    """u(k) exatamente como o firmware gera, para as n_total() amostras."""
    u = np.full(n_total(), U_OP)
    for N, k0, _, k1 in agenda():
        j = np.arange(k1 - k0)
        u[k0:k1] = U_OP + U_AMP * np.sin(2 * np.pi * j / N)
    return u


def le_varredura(caminho):
    """Lê o y_buf exportado pelo CCS usando a coluna Sample (índice no vetor): o gráfico do CCS
    exporta as ÚLTIMAS "Capacity" posições do vetor, que podem não começar em 0. Amostras
    ausentes ficam como NaN; índices além de n_total() são descartados."""
    import csv
    linhas = list(csv.reader(open(caminho)))[2:]
    idx = np.array([int(l[0]) for l in linhas if l and l[0].strip()])
    val = np.array([float(l[1]) for l in linhas if l and l[0].strip()])
    y = np.full(n_total(), np.nan)
    ok = idx < n_total()
    y[idx[ok]] = val[ok]
    return y


def bode(y, u=None):
    """Ganho e fase por frequência (lock-in sobre a janela de integração).
    Retorna dict com f, ganho, fase_graus, amp_u, amp_y, harm2 (amplitude do 2º harmônico de y)."""
    if u is None:
        u = reconstroi_u()
    f, g, ph, au, ay, h2 = [], [], [], [], [], []
    for N, k0, ki, k1 in agenda():
        j = np.arange(ki - k0, k1 - k0)
        e = np.exp(-1j * 2 * np.pi * j / N)
        U = 2 * np.mean(u[ki:k1] * e)
        Y = 2 * np.mean(y[ki:k1] * e)
        H = Y / U
        f.append(1 / (N * TS)); g.append(abs(H)); ph.append(np.degrees(np.angle(H)))
        au.append(abs(U)); ay.append(abs(Y))
        h2.append(abs(2 * np.mean(y[ki:k1] * e ** 2)) if N > 4 else np.nan)
    return dict(f=np.array(f), ganho=np.array(g), fase_graus=np.array(ph),
                amp_u=np.array(au), amp_y=np.array(ay), harm2=np.array(h2))


def main():
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    y = le_varredura(sys.argv[1])
    b = bode(y)
    print(f"{np.sum(~np.isnan(y))} de {n_total()} amostras presentes")
    for i in range(len(b["f"])):
        print(f"  {b['f'][i]:6.2f} Hz: |H| = {b['ganho'][i]:.4f}  fase = {b['fase_graus'][i]:7.2f}°")


if __name__ == "__main__":
    main()
