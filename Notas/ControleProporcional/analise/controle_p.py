"""Análise dos ensaios de controle proporcional (projeto CCS SK03ControleP).

Cada ensaio é um CSV exportado pelo CCS com r_buf, u_buf e y_buf (dados/DadosControleP*.csv).
O ganho Kp e a polarização u0 são estimados dos próprios dados (u = u0 + Kp·(r − y) fora da
saturação), de modo que novos ensaios entram sem editar a análise.

Uso:
  python controle_p.py ../dados/DadosControleP01.csv [--salvar ../imgs/DadosControleP01.png]
"""

import argparse
import sys
from pathlib import Path

import numpy as np

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI.parents[1] / "IdentificacaoSallenKeyPRBS" / "analise"))
sys.path.insert(0, str(AQUI.parents[1] / "ControleSallenKey" / "analise"))

from plota_degrau import le_csv_ccs          # noqa: E402  (leitura do CSV do CCS)
import controle_sim as cs                    # noqa: E402  (planta ARX, simulação, métricas)

# Firmware SK03ControleP (controle.h)
N_PRE, N_MEIO = 20, 60
TS = cs.TS


def le_ensaio(caminho):
    d = le_csv_ccs(caminho)
    r, u, y = d["r_buf"], d["u_buf"], d["y_buf"]
    return dict(nome=Path(caminho).stem, t=np.arange(len(r)) * TS, r=r, u=u, y=y)


def estima_kp(ens):
    """u0 e Kp por mínimos quadrados, só com amostras fora da saturação."""
    r, u, y = ens["r"], ens["u"], ens["y"]
    ok = (u > cs.U_MIN + 1e-6) & (u < cs.U_MAX - 1e-6)
    X = np.column_stack([np.ones(ok.sum()), (r - y)[ok]])
    (u0, kp), *_ = np.linalg.lstsq(X, u[ok], rcond=None)
    return u0, kp


def patamares(r, k_ini=N_PRE, n=N_MEIO):
    """Lista de (k0, k1, nível) para cada patamar da onda quadrada."""
    return [(k0, min(k0 + n, len(r)), r[k0]) for k0 in range(k_ini, len(r), n)]


# ---------------------------------------------------------------------------
# Teoria
# ---------------------------------------------------------------------------

def y_regime(kp, r, u0=cs.U_OP):
    """Regime permanente de y = a1 y + Σb u + c0 com u = u0 + Kp (r − y)."""
    bs = cs.B.sum()
    return (cs.C0 + bs * (u0 + kp * r)) / (1 - cs.A1 + bs * kp)


def erro_regime(kp, r, u0=cs.U_OP):
    return r - y_regime(kp, r, u0)


def polos_mf(kp):
    """Raízes de z³ − (a1 − Kp b1) z² + Kp b2 z + Kp b3 (malha fechada com P)."""
    b1, b2, b3 = cs.B
    return np.roots([1.0, -(cs.A1 - kp * b1), kp * b2, kp * b3])


def kp_critico():
    """Menor Kp com algum polo de malha fechada sobre o círculo unitário."""
    kps = np.linspace(0.01, 30, 30000)
    for kp in kps:
        if np.abs(polos_mf(kp)).max() >= 1:
            return kp
    return np.nan


# ---------------------------------------------------------------------------
# Medido × previsto
# ---------------------------------------------------------------------------

def previsto(ens, kp, u0=cs.U_OP, ruido=False):
    """Simulação da Nota 03 (ARX + atraso + saturação) com a referência do ensaio."""
    return cs.simula(cs.P(kp, u0), ens["r"], ruido=ruido)


def fit(y, yh):
    return 100 * (1 - np.linalg.norm(y - yh) / np.linalg.norm(y - y.mean()))


def media_ciclos(sinal, r, sentido):
    """Média dos trechos que começam em cada degrau de subida (+1) ou descida (−1)."""
    trechos = []
    for k0, k1, nivel in patamares(r):
        if k0 == N_PRE and sentido < 0:
            continue
        sobe = nivel > r[k0 - 1]
        if (sentido > 0) == sobe and k1 - k0 == N_MEIO:
            trechos.append(sinal[k0 - 1:k1])           # inclui a amostra anterior ao degrau
    return np.mean(trechos, axis=0) if trechos else None, len(trechos)


def acomodacao(y, faixa=0.02):
    """t_s (2 %) e sobressinal de um trecho que começa uma amostra antes do degrau."""
    yi, yf = y[0], y[-15:].mean()
    amp = yf - yi
    fora = np.where(np.abs(y[1:] - yf) > faixa * abs(amp))[0]
    ts = (fora[-1] + 1) * TS if len(fora) else 0.0
    pico = (y[1:].max() - yf) / amp if amp > 0 else (yf - y[1:].min()) / -amp
    return ts, 100 * max(0.0, pico)


def main():
    import matplotlib.pyplot as plt
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--salvar")
    a = ap.parse_args()
    ens = le_ensaio(a.csv)
    u0, kp = estima_kp(ens)
    sim = previsto(ens, kp, u0)
    print(f"{ens['nome']}: {len(ens['r'])} amostras · Kp = {kp:.4f} · u0 = {u0:.4f} · "
          f"FIT medido × previsto = {fit(ens['y'], sim['y']):.1f} %")
    for k0, k1, nivel in patamares(ens["r"]):
        ym = ens["y"][k0 + 30:k1].mean()
        print(f"  r = {nivel:.2f} V: y = {ym:.4f} V, e = {1e3 * (nivel - ym):+.1f} mV "
              f"(teoria {1e3 * erro_regime(kp, nivel, u0):+.1f} mV)")
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(10, 5), sharex=True)
    a1.step(ens["t"], ens["r"], where="post", color="k", ls="--", lw=1, label="r")
    a1.plot(ens["t"], ens["y"], color="#C0392B", lw=1.2, label="y medido")
    a1.plot(ens["t"], sim["y"], color="#1F3A5F", lw=1, ls=":", label="y previsto")
    a2.step(ens["t"], ens["u"], where="post", color="#1F3A5F", lw=1.2, label="u")
    a1.legend(); a1.set_ylabel("V"); a2.set_ylabel("u (V)"); a2.set_xlabel("t (s)")
    a1.set_title(f"{ens['nome']} — controle P, Kp = {kp:.2f}")
    fig.tight_layout()
    if a.salvar:
        Path(a.salvar).parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(a.salvar, dpi=150)
    else:
        plt.show()


if __name__ == "__main__":
    main()
