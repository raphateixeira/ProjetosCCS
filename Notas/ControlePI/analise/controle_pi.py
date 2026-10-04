"""Análise dos ensaios de controle PI (projeto CCS SK04ControlePI).

Lei do firmware (controle.c):
    C(z) = Kc (z − z0)/(z − 1)  →  Kp = Kc·z0,  Ki·Ts = Kc·(1 − z0)
    I(k) = I(k−1) + Ki·Ts·e(k),   v(k) = Kp·e(k) + I(k),   u(k) = sat(v(k))
    anti-windup (integração condicional): se v > u_max e e > 0 (ou v < u_min e e < 0),
    o integrador não é atualizado e v = Kp·e(k) + I(k−1).

Arquivos (dados/):
    DadosControlePI01.csv     — onda quadrada 0,8 ↔ 1,2 V (sem saturação), gravação única
    DadosPIsaturacaoAW0.csv   — 0,6 ↔ 1,8 V (1,8 V inatingível), anti-windup desligado, modo contínuo
    DadosPIsaturacaoAW1.csv   — idem, anti-windup ligado
"""

import sys
from pathlib import Path

import numpy as np

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI.parents[1] / "IdentificacaoSallenKeyPRBS" / "analise"))
sys.path.insert(0, str(AQUI.parents[1] / "ControleSallenKey" / "analise"))

from plota_degrau import le_csv_ccs          # noqa: E402
import controle_sim as cs                    # noqa: E402

TS = cs.TS
KC, Z0 = 1.5, 0.8156                         # firmware SK04ControlePI
Y_MAX = cs.K_EST * cs.U_MAX + cs.C0 / (1 - cs.A1)   # maior saída alcançável (u = 1,7 V)


def ganhos(kc=KC, z0=Z0):
    return kc * z0, kc * (1 - z0)            # Kp, Ki·Ts


def le_ensaio(caminho):
    d = le_csv_ccs(caminho)
    r, u, y = d["r_buf"], d["u_buf"], d["y_buf"]
    return dict(nome=Path(caminho).stem, t=np.arange(len(r)) * TS, r=r, u=u, y=y)


def identifica_pi(ens):
    """Kc e z0 por mínimos quadrados em Δu(k) = Kc e(k) − Kc z0 e(k−1), só onde u(k) e
    u(k−1) não saturam. Vale para o PI sem anti-windup (lei linear)."""
    r, u, y = ens["r"], ens["u"], ens["y"]
    e = r - y
    livre = (u > cs.U_MIN + 1e-6) & (u < cs.U_MAX - 1e-6)
    ok = livre[1:] & livre[:-1]
    X = np.column_stack([e[1:][ok], e[:-1][ok]])
    (c0, c1), *_ = np.linalg.lstsq(X, np.diff(u)[ok], rcond=None)
    res = np.diff(u)[ok] - X @ np.array([c0, c1])
    return c0, -c1 / c0, np.abs(res).max()


class PIFirmware(cs.Controlador):
    """Réplica do controle_passo() do SK04ControlePI (para simular a placa)."""

    def __init__(self, kc=KC, z0=Z0, anti_windup=False):
        self.Kp, self.KiTs = ganhos(kc, z0)
        self.aw = anti_windup

    def reinicia(self, r0, u0):
        self.integ = u0

    def passo(self, r, k, y):
        e = r[k] - y
        novo = self.integ + self.KiTs * e
        v = self.Kp * e + novo
        if self.aw and ((v > cs.U_MAX and e > 0) or (v < cs.U_MIN and e < 0)):
            v = self.Kp * e + self.integ
        else:
            self.integ = novo
        self.v = v
        return v


def reconstroi(ens, kc=KC, z0=Z0, anti_windup=False):
    """Reconstrói o integrador I(k) e o comando calculado v(k) aplicando a lei do firmware
    aos r e y gravados. O estado inicial do integrador vem da primeira amostra não saturada
    (I = u − Kp·e); antes dela, só é possível voltar no tempo sem anti-windup."""
    r, u, y = ens["r"], ens["u"], ens["y"]
    Kp, KiTs = ganhos(kc, z0)
    e = r - y
    N = len(r)
    integ = np.full(N, np.nan); v = np.full(N, np.nan)
    livre = np.where((u > cs.U_MIN + 1e-6) & (u < cs.U_MAX - 1e-6))[0]
    k1 = livre[0]
    integ[k1] = u[k1] - Kp * e[k1]
    v[k1] = u[k1]
    for k in range(k1 + 1, N):
        novo = integ[k - 1] + KiTs * e[k]
        vk = Kp * e[k] + novo
        if anti_windup and ((vk > cs.U_MAX and e[k] > 0) or (vk < cs.U_MIN and e[k] < 0)):
            integ[k] = integ[k - 1]; v[k] = Kp * e[k] + integ[k - 1]
        else:
            integ[k] = novo; v[k] = vk
    if not anti_windup:
        for k in range(k1 - 1, -1, -1):
            integ[k] = integ[k + 1] - KiTs * e[k + 1]
            v[k] = Kp * e[k] + integ[k]
    return integ, v


def previsto(ens, kc=KC, z0=Z0, anti_windup=False, periodico=False, ruido=False):
    """Simulação (ARX + atraso + saturação) com a referência do ensaio. Em modo contínuo
    (periodico=True), simula dois períodos antes para chegar ao regime periódico."""
    r = ens["r"]
    rr = np.concatenate([r, r, r]) if periodico else r
    s = cs.simula(PIFirmware(kc, z0, anti_windup), rr, ruido=ruido)
    if periodico:
        n = len(r)
        s = {k: (v[-n:] if isinstance(v, np.ndarray) and len(v) == 3 * n else v) for k, v in s.items()}
        s["t"] = np.arange(n) * TS
    return s


def fit(y, yh):
    return 100 * (1 - np.linalg.norm(y - yh) / np.linalg.norm(y - y.mean()))


def preso_na_saturacao(u, k0):
    """Tempo (s) que u permanece em u_max a partir de k0."""
    k = k0
    while k < len(u) and u[k] >= cs.U_MAX - 1e-6:
        k += 1
    return (k - k0) * TS
