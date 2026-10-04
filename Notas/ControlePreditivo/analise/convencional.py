"""MPC convencional do filtro Sallen-Key — réplica em Python do firmware SK07MPCNumerico.

A cada amostra:
    1. prevê a saída Np passos à frente simulando o modelo ARX (nas variações de y e u),
       a partir das últimas medidas, para um plano de Nu movimentos futuros Δu;
    2. ajusta o plano pelo método do gradiente para reduzir
           J = Σ_{j=1..Np} [r(k) − ŷ(k+j)]² + λ Σ_{j=0..Nu−1} Δu(k+j)²;
    3. aplica só o primeiro movimento: u(k) = u(k−1) + Δu(k).

O algoritmo segue passo a passo o mpc.c do SK07, para que a simulação preveja o que a placa
vai fazer. Planta, referência, simulação e métricas vêm de mpc.py.
"""

import numpy as np

from mpc import A, B, C0, TS, U_MIN, U_MAX, SIGMA, LSB, referencia, metricas, u_equilibrio  # noqa: F401

a1, a2 = A
b1, b2 = B


def resposta_degrau(Np):
    """s_j, j = 1..Np: quanto a saída prevista sobe para um degrau unitário em u (sensibilidade)."""
    s = np.zeros(Np)
    y1 = y2 = 0.0
    u_ant = 0.0
    for j in range(Np):
        yn = a1 * y1 + a2 * y2 + b1 * 1.0 + b2 * u_ant
        u_ant = 1.0
        y2, y1 = y1, yn
        s[j] = yn
    return s


class MPCConvencional:
    """forma: 'variacoes' (a do firmware) ou 'absoluta' (só para a comparação na nota)."""

    def __init__(self, Np=60, Nu=4, lam=3.0, n_iter=50, tol=None, u_min=U_MIN, u_max=U_MAX,
                 forma="variacoes", partida_quente=True):
        """n_iter: iterações do gradiente por amostra (fixas, como no firmware);
        tol: só para análise — para antes de n_iter se |∇J|² < tol."""
        self.Np, self.Nu, self.lam, self.n_iter, self.tol = Np, Nu, lam, n_iter, tol
        self.u_min, self.u_max, self.forma = u_min, u_max, forma
        self.partida_quente = partida_quente
        self.s = resposta_degrau(Np)
        # passo do gradiente: 1/L, com L >= maior "curvatura" de J (soma de linhas, como no firmware)
        S = np.zeros((Np, Nu))
        for i in range(Nu):
            S[i:, i] = self.s[:Np - i]
        H = S.T @ S + lam * np.eye(Nu)
        self.passo = 1.0 / np.abs(H).sum(axis=1).max()
        self.iters = []

    def reinicia(self, y0, u0):
        self.y = [y0, y0, y0]            # y(k), y(k−1), y(k−2) medidos
        self.u = [u0, u0]                # u(k−1), u(k−2) aplicados
        self.dU = np.zeros(self.Nu)      # plano anterior (partida a quente)
        self.iters = []

    def preve(self, dU):
        """ŷ(k+1..k+Np) para o plano dU, simulando o ARX a partir das medidas."""
        Np, Nu = self.Np, self.Nu
        yp = np.zeros(Np)
        if self.forma == "variacoes":
            dyA = self.y[0] - self.y[1]
            dyB = self.y[1] - self.y[2]
            du_ant = self.u[0] - self.u[1]
            yy = self.y[0]
            for j in range(Np):
                du = dU[j] if j < Nu else 0.0
                dy = a1 * dyA + a2 * dyB + b1 * du + b2 * du_ant
                du_ant = du
                dyB, dyA = dyA, dy
                yy += dy
                yp[j] = yy
        else:                            # valores absolutos (precisa do termo constante C0)
            y1, y2 = self.y[0], self.y[1]
            uu, u_ant = self.u[0], self.u[0]
            for j in range(Np):
                if j < Nu:
                    uu = uu + dU[j]
                yn = a1 * y1 + a2 * y2 + b1 * uu + b2 * u_ant + C0
                u_ant = uu
                y2, y1 = y1, yn
                yp[j] = yn
        return yp

    def gradiente(self, dU, r):
        """∇J/2: −Σ_j s_{j−i} e_j + λ Δu_i, com e_j = r − ŷ_j."""
        e = r - self.preve(dU)
        g = np.array([-(self.s[:self.Np - i] @ e[i:]) for i in range(self.Nu)]) + self.lam * dU
        return g, e

    def passo_ctrl(self, r, y):
        self.y = [y] + self.y[:2]
        if self.partida_quente:
            x = np.r_[self.dU[1:], 0.0]  # partida a quente: plano anterior deslocado
        else:
            x = np.zeros(self.Nu)        # partida a frio: plano zerado (só para comparação)
        it = 0
        for it in range(1, self.n_iter + 1):
            g, _ = self.gradiente(x, r)
            x = x - self.passo * g
            if self.tol is not None and g @ g < self.tol:
                break
        self.iters.append(it)
        self.dU = x
        return self.u[0] + x[0]

    def aplicado(self, u):
        """u(k) aplicado (saturado): o plano guardado passa a começar do que foi aplicado."""
        self.dU[0] = u - self.u[0]
        self.u = [u, self.u[0]]

    def custo(self, dU, r):
        e = r - self.preve(dU)
        return e @ e + self.lam * dU @ dU

    def operacoes(self, it):
        """Multiplicações-acumulações (MAC) por amostra: it × (previsão + gradiente)."""
        return it * (4 * self.Np + self.Nu * self.Np + 2 * self.Nu)


class Planta:
    """Planta de simulação: o próprio ARX, com ganho opcionalmente diferente do modelo."""

    def __init__(self, y0, ganho=1.0):
        self.Bp = ganho * B
        self.c0 = C0
        u0 = (y0 * (1 - A.sum()) - self.c0) / self.Bp.sum()
        self.y = [y0, y0]
        self.u = [u0, u0]

    def saida(self):
        return self.y[0]

    def aplica(self, u):
        self.u = [u, self.u[0]]
        yn = A @ np.array(self.y) + self.Bp @ np.array(self.u) + self.c0
        self.y = [yn, self.y[0]]


def simula(ctrl, r, ruido=True, semente=1, ganho=1.0):
    """A cada k: lê y(k) (com ruído e quantização), calcula u(k), satura e aplica."""
    rng = np.random.default_rng(semente)
    N = len(r)
    planta = Planta(r[0], ganho)
    ctrl.reinicia(r[0], planta.u[0])
    y = np.zeros(N); u = np.zeros(N)
    for k in range(N):
        y[k] = planta.saida()
        ym = y[k]
        if ruido:
            ym = np.round((y[k] + rng.normal(0, SIGMA)) / LSB) * LSB
        v = ctrl.passo_ctrl(r[k], ym)
        u[k] = min(max(v, ctrl.u_min), ctrl.u_max)
        ctrl.aplicado(u[k])
        planta.aplica(u[k])
    return dict(t=np.arange(N) * TS, r=r, y=y, u=u, iters=np.array(ctrl.iters))
