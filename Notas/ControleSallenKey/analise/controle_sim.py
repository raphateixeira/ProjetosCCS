"""Projeto e simulação dos controladores do planejamento dos ensaios de controle (filtro Sallen-Key + LAUNCHXL-F28379D).

Planta de simulação: ARX(1,3,1) identificado na nota de identificação do Sallen-Key, em variáveis absolutas,
com o atraso de uma amostra do firmware, saturação do DAC e ruído/quantização do ADC.

Controladores: P, PI (com e sem anti-windup), PID (ideal e com filtro/derivada na
medida), RST genérico (com e sem anti-windup por polinômio observador), GPC/MPC
(sem e com restrições, com e sem antecipação da referência — QP resolvido por Hildreth).
"""

import time
from dataclasses import dataclass

import numpy as np

# ---------------------------------------------------------------------------
# Planta (identificação do Sallen-Key) e condições do ensaio
# ---------------------------------------------------------------------------

TS = 0.005                                  # s
A1 = 0.81557315                             # ỹ(k) = A1 ỹ(k-1) + Σ b_j ũ(k-j)
B = np.array([0.11499529, 0.05852026, 0.00780168])
U_OP, Y_OP = 1.0, 0.99798530                # ponto de operação da identificação
K_EST = B.sum() / (1 - A1)                  # ganho estático ≈ 0,983
C0 = Y_OP * (1 - A1) - B.sum() * U_OP       # termo constante do modelo em variáveis absolutas
U_MIN, U_MAX = 0.0, 1.7                     # saturação do atuador (LM358 em 3,3 V)
SIGMA = 0.0019                              # ruído do ADC medido (V)
LSB = 3.0 / 4095                            # quantização do ADC (V)


def u_equilibrio(y):
    """Entrada constante que mantém a saída em y (regime permanente)."""
    return (y * (1 - A1) - C0) / B.sum()


class Planta:
    """y(k) = A1 y(k-1) + b1 u(k-1) + b2 u(k-2) + b3 u(k-3) + C0."""

    def __init__(self, y0):
        u0 = u_equilibrio(y0)
        self.y = y0
        self.u_hist = [u0, u0, u0]          # u(k-1), u(k-2), u(k-3)

    def saida(self):
        return self.y

    def aplica(self, u):
        """Aplica u(k) e avança para k+1."""
        self.u_hist = [u] + self.u_hist[:2]
        self.y = A1 * self.y + B @ np.array(self.u_hist) + C0


# ---------------------------------------------------------------------------
# Referências
# ---------------------------------------------------------------------------

def ref_quadrada(N=220, k0=20, meio=50, baixo=0.8, alto=1.2):
    """Onda quadrada entre `baixo` e `alto`, começando em `baixo`; troca a cada `meio` amostras após k0."""
    r = np.full(N, baixo)
    for k in range(k0, N):
        r[k] = alto if ((k - k0) // meio) % 2 == 0 else baixo
    return r


def ref_saturacao(N=240, k1=20, k2=140, r0=0.8, r1=1.75, r2=1.0):
    """Degrau para um nível inatingível (y_max ≈ 1,686 V com u = 1,7 V) e retorno: força saturação longa."""
    r = np.full(N, r0)
    r[k1:k2] = r1
    r[k2:] = r2
    return r


# ---------------------------------------------------------------------------
# Simulação em malha fechada
# ---------------------------------------------------------------------------

def sat(u):
    return min(max(u, U_MIN), U_MAX)


def simula(ctrl, r, ruido=True, semente=1):
    """Simula a malha: a cada k, lê y(k) (com ruído), calcula u(k), aplica (saturado)."""
    rng = np.random.default_rng(semente)
    N = len(r)
    planta = Planta(r[0])
    ctrl.reinicia(r[0], u_equilibrio(r[0]))
    y = np.zeros(N); ym = np.zeros(N); u = np.zeros(N); v = np.zeros(N); tc = np.zeros(N)
    for k in range(N):
        y[k] = planta.saida()
        ym[k] = y[k] + (rng.normal(0, SIGMA) if ruido else 0.0)
        if ruido:
            ym[k] = np.round(ym[k] / LSB) * LSB
        t0 = time.perf_counter()
        v[k] = ctrl.passo(r, k, ym[k])           # comando calculado (antes da saturação)
        tc[k] = time.perf_counter() - t0
        u[k] = sat(v[k])
        ctrl.informa_aplicado(u[k])
        planta.aplica(u[k])
    return dict(t=np.arange(N) * TS, r=r, y=y, ym=ym, u=u, v=v, tc=tc,
                iters=getattr(ctrl, "iters", None))


# ---------------------------------------------------------------------------
# Métricas
# ---------------------------------------------------------------------------

def metricas_degrau(sim, k0, k1, faixa=0.02):
    """Métricas do degrau de referência em k0 (janela até k1): acomodação (2 %), sobressinal,
    erro em regime — usando a saída sem ruído."""
    y, r = sim["y"], sim["r"]
    yi = y[k0 - 1]
    yf = y[k1 - 10:k1].mean()
    passo = r[k0] - r[k0 - 1]
    amp = yf - yi
    fora = np.where(np.abs(y[k0:k1] - yf) > faixa * abs(passo))[0]
    ts = (fora[-1] + 1) * TS if len(fora) else 0.0
    trecho = y[k0:k1]
    os_ = 100 * max(0.0, (trecho.max() - yf) / amp if amp > 0 else (yf - trecho.min()) / -amp)
    ess = r[k1 - 1] - yf
    return dict(ts=ts, os=os_, ess=ess)


def metricas(sim, k0=20, k1=70):
    m = metricas_degrau(sim, k0, k1)
    e = sim["r"] - sim["y"]
    du = np.diff(sim["u"])
    m.update(IAE=np.sum(np.abs(e)) * TS,
             TV=np.sum(np.abs(du)),
             std_u=sim["u"][k1 - 20:k1].std(),          # flutuação de u em regime (ruído)
             tc_max=sim["tc"].max())
    return m


# ---------------------------------------------------------------------------
# Controladores clássicos
# ---------------------------------------------------------------------------

class Controlador:
    iters = None

    def reinicia(self, r0, u0):
        pass

    def informa_aplicado(self, u):
        pass


class P(Controlador):
    """u(k) = u0 + Kp·e(k), com u0 = U_OP (polarização no ponto de operação)."""

    def __init__(self, Kp, u0=U_OP):
        self.Kp, self.u0 = Kp, u0

    def passo(self, r, k, y):
        return self.u0 + self.Kp * (r[k] - y)


class PID(Controlador):
    """PID em forma de posição, com opções didáticas.

    u(k) = Kp·e(k) + I(k) + D(k)
      I(k+1) = I(k) + Ki·Ts·e(k)                       (integrador retangular: u(k) usa I(k))
      D ideal:      Kd·[e(k) − e(k−1)]/Ts
      D filtrado:   D(k) = Tf/(Tf+Ts)·D(k−1) + Kd/(Tf+Ts)·[x(k) − x(k−1)]   (Tf = Td/N ou dado)
      x = e (derivada do erro) ou x = −y (derivada só na medida)
    anti-windup: None | 'condicional' | 'retrocalculo' (back-calculation, constante Tt)
    """

    def __init__(self, Kp, Ki=0.0, Kd=0.0, N=None, Tf=None, d_medida=False, aw=None, Tt=None):
        self.Kp, self.Ki, self.Kd = Kp, Ki, Kd
        self.Tf = Tf if Tf is not None else ((Kd / Kp) / N if N else None)
        self.d_medida, self.aw = d_medida, aw
        self.Tt = Tt if Tt is not None else (Kp / Ki if Ki else 1.0)

    def reinicia(self, r0, u0):
        self.I = u0                      # partida sem solavanco: e = 0 ⇒ u = I = u0
        self.D = 0.0
        self.x_ant = None
        self.v = u0

    def passo(self, r, k, y):
        e = r[k] - y
        x = -y if self.d_medida else e
        if self.x_ant is None:
            self.x_ant = x
        if self.Kd:
            if self.Tf:
                Tf = self.Tf
                self.D = Tf / (Tf + TS) * self.D + self.Kd / (Tf + TS) * (x - self.x_ant)
            else:
                self.D = self.Kd * (x - self.x_ant) / TS
        self.x_ant = x
        self.e = e
        self.v = self.Kp * e + self.I + self.D
        return self.v

    def informa_aplicado(self, u):
        e, v = self.e, self.v
        if self.aw == "condicional":
            satura_e_piora = (v > U_MAX and e > 0) or (v < U_MIN and e < 0)
            if not satura_e_piora:
                self.I += self.Ki * TS * e
        elif self.aw == "retrocalculo":
            self.I += self.Ki * TS * e + TS / self.Tt * (u - v)
        else:
            self.I += self.Ki * TS * e



# ---------------------------------------------------------------------------
# RST genérico:  S(q⁻¹) u(k) = T(q⁻¹) r(k) − R(q⁻¹) y(k)
# ---------------------------------------------------------------------------

class RST(Controlador):
    """Lei RST com S mônico.

    Sem anti-windup (Ao=None): usa o comando NÃO saturado no passado,
        v(k) = Σ t_i r(k−i) − Σ r_i y(k−i) − Σ_{i≥1} s_i v(k−i).
    Com anti-windup por polinômio observador Ao (mônico, grau de S):
        Ao v = T r − R y + (Ao − S) u_sat,   u = sat(v)
    (sem saturação, equivale exatamente a S u = T r − R y).
    """

    def __init__(self, R, S, T, Ao=None):
        self.R, self.S, self.T = map(lambda p: np.atleast_1d(np.asarray(p, float)), (R, S, T))
        assert abs(self.S[0] - 1) < 1e-12, "S deve ser mônico"
        self.Ao = None if Ao is None else np.asarray(Ao, float)

    def reinicia(self, r0, u0):
        n = max(len(self.R), len(self.S), len(self.T), 1 if self.Ao is None else len(self.Ao)) + 1
        self.yh = [r0] * n                     # y(k), y(k-1), ...  (equilíbrio: y = r0)
        self.rh = [r0] * n
        self.vh = [u0] * n                     # v(k-1), v(k-2), ...
        self.uh = [u0] * n                     # u_sat(k-1), ...

    def passo(self, r, k, y):
        self.yh = [y] + self.yh[:-1]
        self.rh = [r[k]] + self.rh[:-1]
        Tr = self.T @ np.array(self.rh[:len(self.T)])
        Ry = self.R @ np.array(self.yh[:len(self.R)])
        if self.Ao is None:
            v = Tr - Ry - self.S[1:] @ np.array(self.vh[:len(self.S) - 1])
        else:
            nS, nA = len(self.S), len(self.Ao)
            n = max(nS, nA)
            Sp = np.pad(self.S, (0, n - nS)); Ap = np.pad(self.Ao, (0, n - nA))
            v = Tr - Ry + (Ap[1:] - Sp[1:]) @ np.array(self.uh[:n - 1]) - Ap[1:] @ np.array(self.vh[:n - 1])
        self.v = v
        return v

    def informa_aplicado(self, u):
        self.vh = [self.v] + self.vh[:-1]
        self.uh = [u] + self.uh[:-1]


def pid_para_rst(Kp, Ki, Kd):
    """PID ideal (erro) na forma RST: S = 1 − q⁻¹, R = T = r0 + r1 q⁻¹ + r2 q⁻².

    Com u(k) = Kp e(k) + I(k) + Kd [e(k) − e(k−1)]/Ts e I(k) = I(k−1) + Ki Ts e(k−1):
    Δu(k) = (Kp + Kd/Ts) e(k) + (Ki Ts − Kp − 2Kd/Ts) e(k−1) + (Kd/Ts) e(k−2)."""
    kd = Kd / TS
    Rp = np.array([Kp + kd, Ki * TS - Kp - 2 * kd, kd])
    if not Kd:
        Rp = Rp[:2]
    return Rp, np.array([1.0, -1.0]), Rp.copy()


# ---------------------------------------------------------------------------
# Alocação de polos (equação diofantina)
# ---------------------------------------------------------------------------

def polimul(*ps):
    out = np.array([1.0])
    for p in ps:
        out = np.convolve(out, p)
    return out


def diofantina(A, B, P, nS, nR):
    """Resolve A·S + B·R = P (polinômios em q⁻¹), S mônico de grau nS, R de grau nR."""
    nP = len(A) - 1 + nS
    assert len(B) - 1 + nR <= nP
    P = np.pad(P, (0, nP + 1 - len(P)))
    # incógnitas: s1..s_nS, r0..r_nR
    M = np.zeros((nP + 1, nS + nR + 1))
    for j in range(1, nS + 1):
        M[j:j + len(A), j - 1] += A
    for j in range(nR + 1):
        M[j:j + len(B), nS + j] += B
    rhs = P - np.pad(A, (0, nP + 1 - len(A)))
    sol = np.linalg.solve(M[1:], rhs[1:])
    S = np.r_[1.0, sol[:nS]]
    R = sol[nS:]
    return S, R


def projeto_rst_alocacao(p_dom, alfa_obs, n_obs=3):
    """RST com integrador: (A·Δ)·S' + B·R = P,  P = (1 − p_dom q⁻¹)·(1 − α q⁻¹)^n_obs.
    T = β·Pf (β ajusta ganho unitário): o rastreamento depende só de Pd; Pf (observador) só
    da regulação."""
    A = np.array([1.0, -A1])
    Bq = np.r_[0.0, B]                                  # q⁻¹·(b1 + b2 q⁻¹ + b3 q⁻²)
    Delta = np.array([1.0, -1.0])
    Pd = np.array([1.0, -p_dom])
    Pf = polimul(*[np.array([1.0, -alfa_obs])] * n_obs)
    P = polimul(Pd, Pf)
    AD = polimul(A, Delta)
    Sl, R = diofantina(AD, Bq, P, nS=len(Bq) - 2, nR=len(AD) - 2)
    S = polimul(Sl, Delta)
    beta = Pd.sum() / Bq.sum()
    T = beta * Pf
    return dict(R=R, S=S, T=T, P=P, Pd=Pd, Pf=Pf, Sl=Sl, beta=beta)


# ---------------------------------------------------------------------------
# GPC / MPC (modelo CARIMA: (1 − A1 q⁻¹)Δ y = B(q⁻¹) Δu(k−1) + e/… )
# ---------------------------------------------------------------------------

def _prediz(y0, y1, du_pass, du_fut, Np):
    """Predições ŷ(k+1..k+Np) do modelo incremental Ã y = B Δu(·−1), Ã = (1 − A1 q⁻¹)(1 − q⁻¹).
    y0 = y(k), y1 = y(k−1); du_pass = [Δu(k−1), Δu(k−2)]; du_fut = Δu(k..)."""
    a1, a2 = 1 + A1, -A1
    du = list(du_pass[::-1]) + list(du_fut)            # Δu(k−2), Δu(k−1), Δu(k), ...
    yy = [y1, y0]
    out = []
    for j in range(1, Np + 1):
        idx = 1 + j                                      # posição de Δu(k+j−1) em du
        dus = [du[idx - 1 - m] if 0 <= idx - 1 - m < len(du) else 0.0 for m in range(3)]
        yn = a1 * yy[-1] + a2 * yy[-2] + B @ np.array(dus)
        yy.append(yn); out.append(yn)
    return np.array(out)


def matrizes_gpc(Np, Nu):
    """G (resposta forçada), Fy e Fu (resposta livre) por superposição."""
    G = np.zeros((Np, Nu))
    for i in range(Nu):
        e = np.zeros(Nu); e[i] = 1
        G[:, i] = _prediz(0, 0, [0, 0], e, Np)
    Fy = np.column_stack([_prediz(1, 0, [0, 0], [], Np), _prediz(0, 1, [0, 0], [], Np)])
    Fu = np.column_stack([_prediz(0, 0, [1, 0], [], Np), _prediz(0, 0, [0, 1], [], Np)])
    return G, Fy, Fu


class MPC(Controlador):
    """GPC/MPC: min Σ (ŷ − w)² + λ Σ Δu²  sobre Np e Nu.

    restrito=False → GPC (solução analítica, equivalente a um RST);
    restrito=True  → QP com U_MIN ≤ u ≤ U_MAX e |Δu| ≤ dumax, resolvido por Hildreth;
    preview=True   → w(k+j) = r(k+j) (referência futura conhecida); senão w = r(k);
    alfa_ref       → trajetória de referência w(k+j) = (1 − αʲ)·r + αʲ·y(k) (suaviza o rastreamento);
    alfa_obs       → observador de estado com polos em α (papel do polinômio C do GPC): filtra o
                     ruído de medida antes da predição; None = usa y medido (observador deadbeat)."""

    def __init__(self, Np=20, Nu=3, lam=1.0, alfa_ref=0.0, restrito=False, dumax=None, preview=False,
                 alfa_obs=None, max_iter=200, tol=1e-7):
        self.Np, self.Nu, self.lam, self.alfa_ref = Np, Nu, lam, alfa_ref
        self.alfa_obs = alfa_obs
        if alfa_obs is not None:
            # observador corrente para s = [y(k), y(k−1)]:  s(k+1) = As s(k) + (termos conhecidos de Δu)
            As = np.array([[1 + A1, -A1], [1.0, 0.0]])
            Cs = np.array([[1.0, 0.0]])
            # polos de (I − L Cs) As em {α, α}: Ackermann com C' = Cs·As
            Cp = Cs @ As
            Obs = np.vstack([Cp, Cp @ As])
            phi = As @ As - 2 * alfa_obs * As + alfa_obs ** 2 * np.eye(2)
            self.L = (phi @ np.linalg.inv(Obs) @ np.array([[0.0], [1.0]])).ravel()
            self.As = As
        self.restrito, self.dumax, self.preview = restrito, dumax, preview
        self.max_iter, self.tol = max_iter, tol
        self.G, self.Fy, self.Fu = matrizes_gpc(Np, Nu)
        self.H = self.G.T @ self.G + lam * np.eye(Nu)
        self.Hinv = np.linalg.inv(self.H)
        self.K = (self.Hinv @ self.G.T)                 # ΔU* = K (w − f)
        L = np.tril(np.ones((Nu, Nu)))
        I = np.eye(Nu)
        if dumax is None:
            blocos = [L, -L]
        else:
            # a restrição de u(k) e a de Δu(k) são paralelas (degeneram o dual e atrasam o
            # Hildreth): o 1º movimento fica só com Δu(k), com o limite mais apertado dos dois
            blocos = [L[1:], -L[1:], I, -I]
        self.M = np.vstack(blocos)
        self.Pd = self.M @ self.Hinv @ self.M.T         # matriz do problema dual (fixa)
        self.iters = []

    def reinicia(self, r0, u0):
        self.y1 = r0
        self.du1 = self.du2 = self.du3 = 0.0
        self.u_ant = u0
        self.iters = []
        self.s_pred = np.array([r0, r0])          # ŝ(k|k−1)
        self.lam_dual = np.zeros(self.M.shape[0]) # multiplicadores (partida a quente)

    def _w(self, r, k, y):
        if self.preview:
            idx = np.minimum(np.arange(k + 1, k + 1 + self.Np), len(r) - 1)
            alvo = r[idx]
        else:
            alvo = np.full(self.Np, r[k])
        aj = self.alfa_ref ** np.arange(1, self.Np + 1)
        return (1 - aj) * alvo + aj * y

    def passo(self, r, k, y):
        if self.alfa_obs is not None:
            s_est = self.s_pred + self.L * (y - self.s_pred[0])      # ŝ(k|k)
            y0, y1 = s_est
            self.s_est = s_est
        else:
            y0, y1 = y, self.y1
        f = self.Fy @ np.array([y0, y1]) + self.Fu @ np.array([self.du1, self.du2])
        w = self._w(r, k, y0)
        self.y1 = y
        if not self.restrito:
            dU = self.K @ (w - f)
            it = 0
        else:
            g = self.G.T @ (f - w)                       # gradiente: ½ΔUᵀHΔU + gᵀΔU
            if self.dumax is None:
                gam = [U_MAX - self.u_ant] * self.Nu + [self.u_ant - U_MIN] * self.Nu
            else:
                n1 = self.Nu - 1
                sobe = [min(self.dumax, U_MAX - self.u_ant)] + [self.dumax] * n1
                desce = [min(self.dumax, self.u_ant - U_MIN)] + [self.dumax] * n1
                gam = [U_MAX - self.u_ant] * n1 + [self.u_ant - U_MIN] * n1 + sobe + desce
            gam = np.array(gam)
            dU, it = self._hildreth(g, gam)
            # garantia: o 1º movimento (o único aplicado) respeita os limites mesmo se o
            # Hildreth parar antes de convergir
            lo, hi = U_MIN - self.u_ant, U_MAX - self.u_ant
            if self.dumax is not None:
                lo, hi = max(lo, -self.dumax), min(hi, self.dumax)
            dU[0] = min(max(dU[0], lo), hi)
        self.iters.append(it)
        self.v = self.u_ant + dU[0]
        return self.v

    def _hildreth(self, g, gam):
        x0 = -self.Hinv @ g
        if np.all(self.M @ x0 <= gam + 1e-12):
            self.lam_dual[:] = 0.0
            return x0, 0
        d = gam + self.M @ self.Hinv @ g
        lam = self.lam_dual.copy()                          # partida a quente
        for it in range(1, self.max_iter + 1):
            lam_ant = lam.copy()
            for i in range(len(lam)):
                wi = -(d[i] + self.Pd[i] @ lam - self.Pd[i, i] * lam[i]) / self.Pd[i, i]
                lam[i] = max(0.0, wi)
            if np.sum((lam - lam_ant) ** 2) < self.tol:
                break
        self.lam_dual = lam
        return -self.Hinv @ (g + self.M.T @ lam), it

    def informa_aplicado(self, u):
        du = u - self.u_ant
        if self.alfa_obs is not None:                              # ŝ(k+1|k)
            yn = self.As[0] @ self.s_est + B @ np.array([du, self.du1, self.du2])
            self.s_pred = np.array([yn, self.s_est[0]])
        self.du3, self.du2, self.du1 = self.du2, self.du1, du
        self.u_ant = u

    def como_rst(self):
        """Lei GPC sem restrições (w constante) como RST: S Δ... ver nota."""
        k1 = self.K[0]
        aj = self.alfa_ref ** np.arange(1, self.Np + 1)
        hy = k1 @ self.Fy - np.array([k1 @ aj, 0.0])     # coeficientes de y(k), y(k−1)
        hu = k1 @ self.Fu                                # coeficientes de Δu(k−1), Δu(k−2)
        t0 = k1 @ (1 - aj)
        Sl = np.r_[1.0, hu]                              # Δu(k) + hu1 Δu(k−1) + hu2 Δu(k−2)
        S = polimul(Sl, np.array([1.0, -1.0]))
        R = hy
        T = np.array([t0])
        return R, S, T

    def como_rst_com_C(self, C):
        """Mesma lei de rastreamento do GPC, com os polos de regulação multiplicados pelo
        polinômio C (observador): A·S + B·R = Pc·C,  T = T0·C."""
        R0, S0, T0 = self.como_rst()
        A = np.array([1.0, -A1]); Bq = np.r_[0.0, B]
        n = max(len(A) + len(S0), len(Bq) + len(R0)) - 1
        Pc = np.pad(np.convolve(A, S0), (0, n - len(A) - len(S0) + 1)) + \
             np.pad(np.convolve(Bq, R0), (0, n - len(Bq) - len(R0) + 1))
        Pc = np.trim_zeros(Pc, "b")
        P = polimul(Pc, C)
        AD = polimul(A, np.array([1.0, -1.0]))
        Sl, R = diofantina(AD, Bq, P, nS=len(P) - len(AD), nR=len(AD) - 2)
        return R, polimul(Sl, np.array([1.0, -1.0])), T0 * np.asarray(C, float), Pc

    def custo_ciclos(self, iter_max, ciclos_por_mac=5):
        """Estimativa grosseira de ciclos de CPU (C28x + FPU) por amostra."""
        Np, Nu, m = self.Np, self.Nu, self.M.shape[0]
        macs = 5 * Np + Nu * Np + m * Nu + Nu * Nu + iter_max * m * m + Nu * m
        return macs * ciclos_por_mac


# ---------------------------------------------------------------------------
# Utilidades de projeto
# ---------------------------------------------------------------------------

def polos_malha_fechada(R, S, T=None):
    """Raízes de A S + B R (em z), planta ARX(1,3,1) com atraso."""
    A = np.array([1.0, -A1])
    Bq = np.r_[0.0, B]
    n = max(len(A) + len(S), len(Bq) + len(R)) - 1
    P = np.pad(np.convolve(A, S), (0, n - len(A) - len(S) + 1)) + np.pad(np.convolve(Bq, R), (0, n - len(Bq) - len(R) + 1))
    return np.roots(P)
