"""Controle preditivo (MPC) do filtro Sallen-Key — réplica em Python do firmware.

Projetos do CCS (ProjetosSK/), todos com o mesmo modelo, observador e custo:
    SK07MPCNumerico    — MPC "de livro": custo minimizado pelo método do gradiente a cada Ts
    SK08MPCAnalitico   — solução analítica antes do laço: Δu(k) = Ky r̃(k) − Kmpc x̂(k)
    SK09MPCRestricoes  — QP com restrições em u, Δu e y, resolvido por Hildreth a cada Ts

Formulação em espaço de estados (Wang, 2009):
    modelo (ARX da Nota 02 em forma canônica observável, variáveis de desvio)
        xm(k+1) = Am xm(k) + Bm ũ(k),  ỹ(k) = Cm xm(k)
    modelo aumentado, com integrador:  x(k) = [Δxm(k); ỹ(k)]
        x(k+1) = A x(k) + B Δu(k),  ỹ(k) = C x(k)
    predição:  Ŷ = F x̂(k) + Φ ΔU
    custo:     J = Σ_{j=1..Np} [r̃(k) − ŷ(k+j)]² + λ Σ_{j=0..Nu−1} Δu(k+j)²
    observador corrente com os três polos em alfa_obs.
Os algoritmos seguem passo a passo o mpc.c de cada projeto, para que a simulação preveja o
que a placa vai fazer.

Planta: ARX(2,2,1) identificado na Nota 02 com Ts = 1 ms (DadosPRBS1ms.csv).
"""

import time
from pathlib import Path

import numpy as np

AQUI = Path(__file__).resolve().parent

# ---------------------------------------------------------------------------
# Modelo (Nota 02) e condições do ensaio — os mesmos números do main.c
# ---------------------------------------------------------------------------

TS = 1e-3                                    # s
A = np.array([1.60918827, -0.62250084])      # ỹ(k) = a1 ỹ(k−1) + a2 ỹ(k−2) + b1 ũ(k−1) + b2 ũ(k−2)
B = np.array([0.01096776, 0.00237673])
U_OP, Y_OP = 1.0, 0.99671789                 # ponto de operação da identificação
K_EST = B.sum() / (1 - A.sum())              # ganho estático ≈ 1,002
C0 = Y_OP * (1 - A.sum()) - B.sum() * U_OP   # termo constante em variáveis absolutas
U_MIN, U_MAX = 0.0, 1.7                      # saturação do atuador (LM358 em 3,3 V)
SIGMA = 0.0019                               # ruído do ADC medido (V)
LSB = 3.0 / 4095                             # quantização do ADC (V)
F_CPU = 200e6                                # SYSCLK (Hz): 200 000 ciclos por Ts


def u_equilibrio(y):
    """Entrada constante que mantém a saída da planta em y."""
    return (y * (1 - A.sum()) - C0) / B.sum()


class Planta:
    """y(k) = a1 y(k−1) + a2 y(k−2) + b1 u(k−1) + b2 u(k−2) + C0 (atraso de uma amostra)."""

    def __init__(self, y0):
        self.y = [y0, y0]
        self.u = [u_equilibrio(y0)] * 2

    def saida(self):
        return self.y[0]

    def aplica(self, u):
        self.u = [u, self.u[0]]
        yn = A @ np.array(self.y) + B @ np.array(self.u) + C0
        self.y = [yn, self.y[0]]


# ---------------------------------------------------------------------------
# Referência do ensaio (ensaio.c): onda quadrada
# ---------------------------------------------------------------------------

def referencia(N=800, k0=100, meio=200, baixo=0.8, alto=1.2):
    """r = baixo até k0; depois alterna alto/baixo a cada `meio` amostras."""
    r = np.full(N, baixo)
    for k in range(k0, N):
        r[k] = alto if ((k - k0) // meio) % 2 == 0 else baixo
    return r


# ---------------------------------------------------------------------------
# Modelo em espaço de estados e matrizes do MPC (ConfiguraMPC, antes do laço)
# ---------------------------------------------------------------------------

def espaco_estados():
    """Am, Bm, Cm (forma canônica observável do ARX) e o modelo aumentado A, B, C."""
    a1, a2 = A; b1, b2 = B
    Am = np.array([[a1, 1.0], [a2, 0.0]])
    Bm = np.array([b1, b2])
    Cm = np.array([1.0, 0.0])
    Aa = np.zeros((3, 3))
    Aa[:2, :2] = Am
    Aa[2, :2] = Cm @ Am
    Aa[2, 2] = 1.0
    Ba = np.r_[Bm, Cm @ Bm]
    Ca = np.array([0.0, 0.0, 1.0])
    return Am, Bm, Cm, Aa, Ba, Ca


def matrizes(Np, Nu):
    """F (Np×3): linhas C A^j, j = 1..Np;  Φ (Np×Nu): Φ[j, i] = C A^(j−i) B, j ≥ i."""
    *_, Aa, Ba, Ca = espaco_estados()
    F = np.zeros((Np, 3))
    g = np.zeros(Np)                         # g_j = C A^(j−1) B: resposta ao degrau do modelo
    v = Ca.copy()
    for j in range(Np):
        g[j] = v @ Ba
        v = v @ Aa
        F[j] = v
    Phi = np.zeros((Np, Nu))
    for i in range(Nu):
        Phi[i:, i] = g[:Np - i]
    return F, Phi


def ganho_observador(alfa):
    """Observador corrente x̂(k|k) = x̂(k|k−1) + L [ỹ(k) − C x̂(k|k−1)], polos de (I − LC)A em alfa
    (os três): Ackermann, L = φ(A) O⁻¹ e3, O = [CA; CA²; CA³], φ(A) = (A − αI)³."""
    *_, Aa, Ba, Ca = espaco_estados()
    O = np.array([Ca @ Aa, Ca @ Aa @ Aa, Ca @ Aa @ Aa @ Aa])
    phi = np.linalg.matrix_power(Aa - alfa * np.eye(3), 3)
    return phi @ np.linalg.solve(O, np.array([0.0, 0.0, 1.0]))


def inverte(Hm):
    """Gauss-Jordan sem pivotamento (H é simétrica definida positiva), como no firmware."""
    n = len(Hm)
    M = np.hstack([Hm.astype(float), np.eye(n)])
    for c in range(n):
        M[c] /= M[c, c]
        for l in range(n):
            if l != c:
                M[l] -= M[l, c] * M[c]
    return M[:, n:]


def restricoes(Phi, Nu, usa_u, usa_du, passo_y):
    """Linhas de M (MΔU ≤ γ) na ordem do firmware e o "tipo" de cada linha, para montar γ.

    tipo: ('u+', j)  u(k+j) ≤ u_max      ('u−', j)  −u(k+j) ≤ −u_min
          ('du+', j) Δu(k+j) ≤ Δu_max    ('du−', j) −Δu(k+j) ≤ Δu_max
          ('y+', j)  ŷ(k+j+1) ≤ y_max, em ŷ(k+passo_y), ŷ(k+2 passo_y), … até ŷ(k+Np)
                     (passo_y = 0: sem restrição de saída)
    Com u e Δu juntos, as linhas de u(k) e Δu(k) seriam paralelas (dual degenerado): a de
    u(k) sai, e o limite de Δu(k) passa a ser o mais apertado dos dois.
    """
    Np = len(Phi)
    L = np.tril(np.ones((Nu, Nu)))
    linhas, tipos = [], []
    j0 = 1 if usa_du else 0
    if usa_u:
        for j in range(j0, Nu):
            linhas.append(L[j]); tipos.append(("u+", j))
            linhas.append(-L[j]); tipos.append(("u-", j))
    if usa_du:
        for j in range(Nu):
            e = np.zeros(Nu); e[j] = 1
            linhas.append(e); tipos.append(("du+", j))
            linhas.append(-e); tipos.append(("du-", j))
    if passo_y > 0:
        for j in range(passo_y - 1, Np, passo_y):
            linhas.append(Phi[j]); tipos.append(("y+", j))
    M = np.array(linhas) if linhas else np.zeros((0, Nu))
    return M, tipos


# ---------------------------------------------------------------------------
# Controlador
# ---------------------------------------------------------------------------

class MPC:
    """MPC em espaço de estados (modelo aumentado + observador), custo J = |R̃ − Ŷ|² + λ|ΔU|².

    metodo: 'gradiente' — descida do gradiente, passo 1/L, partida a quente (SK07)
            'analitico' — Δu(k) = Ky r̃ − Kmpc x̂, ganhos calculados antes do laço (SK08)
            'hildreth'  — QP com restrições, subida coordenada no dual (SK09)
    alfa_obs: polos do observador (os três); 0 = deadbeat (equivale ao GPC com y medido).
    """

    def __init__(self, Np=60, Nu=4, lam=3.0, alfa_obs=0.9, metodo="gradiente",
                 it_max=500, tol=1e-8, usa_u=False, usa_du=False, du_max=0.01, passo_y=0,
                 y_max=1.2, u_min=U_MIN, u_max=U_MAX):
        self.Np, self.Nu, self.lam, self.metodo = Np, Nu, lam, metodo
        self.it_max, self.tol = it_max, tol
        self.u_min, self.u_max, self.du_max, self.y_max = u_min, u_max, du_max, y_max
        self.usa_u, self.usa_du = usa_u, usa_du
        *_, self.Aa, self.Ba, self.Ca = espaco_estados()
        self.F, self.Phi = matrizes(Np, Nu)
        self.Lobs = ganho_observador(alfa_obs)
        self.H = self.Phi.T @ self.Phi + lam * np.eye(Nu)
        self.Hinv = inverte(self.H)
        k1 = (self.Hinv @ self.Phi.T)[0]             # 1ª linha de (ΦᵀΦ + λI)⁻¹Φᵀ
        self.k1 = k1
        self.Ky = k1.sum()                           # ganho da referência
        self.Kmpc = k1 @ self.F                      # realimentação de estados
        # passo do gradiente: 1/L, L = maior soma de linha de |H| (Gershgorin) ≥ λmax(H)
        self.passo = 1.0 / np.abs(self.H).sum(axis=1).max()
        self.M, self.tipos = restricoes(self.Phi, Nu, usa_u, usa_du, passo_y if metodo == "hildreth" else 0)
        if metodo == "hildreth" and len(self.M):
            self.MHinv = self.M @ self.Hinv
            self.P = self.MHinv @ self.M.T
        self.iters = []

    # --- estados -----------------------------------------------------------
    def reinicia(self, y0, u0):
        self.xp = np.array([0.0, 0.0, y0 - Y_OP])    # x̂(k|k−1): planta parada em y0
        self.u_ant = u0
        self.dU = np.zeros(self.Nu)                  # partida a quente (gradiente)
        self.lam_dual = np.zeros(len(self.M))        # partida a quente (Hildreth)
        self.iters = []

    def _observador(self, y):
        """x̂(k|k) a partir de x̂(k|k−1) e da medida ỹ(k)."""
        self.x = self.xp + self.Lobs * (y - Y_OP - self.Ca @ self.xp)
        return self.x

    def passo_ctrl(self, r, y):
        x = self._observador(y)
        rt = r - Y_OP
        it = 0
        if self.metodo == "analitico":
            du = self.Ky * rt - self.Kmpc @ x
            f = None
        else:
            f = self.F @ x                           # resposta livre (desvio)
            g = -self.Phi.T @ (rt - f)               # J = ½ΔUᵀHΔU + gᵀΔU
            if self.metodo == "gradiente":
                xs = np.r_[self.dU[1:], 0.0]         # partida a quente: solução anterior deslocada
                for it in range(1, self.it_max + 1):
                    grad = self.H @ xs + g
                    xs = xs - self.passo * grad
                    if grad @ grad < self.tol:
                        break
                self.dU = xs
            else:
                xs, it = self._hildreth(g, f)
            du = xs[0]
            if self.metodo == "hildreth" and self.usa_du:   # garantia: |Δu(k)| ≤ Δu_max
                du = min(max(du, -self.du_max), self.du_max)
        self.iters.append(it)
        self.v = self.u_ant + du
        return self.v

    def _gama(self, f):
        ua = self.u_ant
        gam = []
        for tipo, j in self.tipos:
            if tipo == "u+":
                gam.append(self.u_max - ua)
            elif tipo == "u-":
                gam.append(ua - self.u_min)
            elif tipo == "du+":
                gam.append(min(self.du_max, self.u_max - ua) if (j == 0 and self.usa_u) else self.du_max)
            elif tipo == "du-":
                gam.append(min(self.du_max, ua - self.u_min) if (j == 0 and self.usa_u) else self.du_max)
            else:
                gam.append(self.y_max - Y_OP - f[j])
        return np.array(gam)

    def _hildreth(self, g, f):
        x0 = -self.Hinv @ g
        if len(self.M) == 0:
            return x0, 0
        gam = self._gama(f)
        if np.all(self.M @ x0 <= gam):
            self.lam_dual[:] = 0.0
            return x0, 0
        dd = gam - self.M @ x0                       # d = γ + M H⁻¹ g
        lam = self.lam_dual.copy()
        P = self.P
        it = 0
        for it in range(1, self.it_max + 1):
            var = 0.0
            for i in range(len(lam)):
                wi = -(dd[i] + P[i] @ lam - P[i, i] * lam[i]) / P[i, i]
                wi = max(0.0, wi)
                var += (wi - lam[i]) ** 2
                lam[i] = wi
            if var < self.tol:
                break
        self.lam_dual = lam
        return x0 - self.MHinv.T @ lam, it

    def aplicado(self, u):
        """u(k) aplicado (saturado): x̂(k+1|k) = A x̂(k|k) + B Δu(k)."""
        self.xp = self.Aa @ self.x + self.Ba * (u - self.u_ant)
        self.u_ant = u

    # --- custo computacional ---------------------------------------------------
    def operacoes(self, it):
        """Multiplicações-acumulações (MAC) por amostra, contadas como no mpc.c: (predição, otimização).
        Predição = observador (+ resposta livre F x̂, exceto no analítico)."""
        Np, Nu, m = self.Np, self.Nu, len(self.M)
        obs = 3 + 9 + 3
        if self.metodo == "analitico":
            return obs, 4
        pred = obs + 3 * Np
        if self.metodo == "gradiente":
            otim = Nu * Np + it * (Nu * Nu + 2 * Nu)
        else:
            otim = Nu * Np + Nu * Nu + m * Nu + it * m * m + (m * Nu if it else 0)
        return pred, otim


# ---------------------------------------------------------------------------
# Simulação em malha fechada
# ---------------------------------------------------------------------------

def simula(ctrl, r, ruido=True, semente=1):
    """A cada k: lê y(k) (com ruído e quantização), calcula u(k), satura e aplica."""
    rng = np.random.default_rng(semente)
    N = len(r)
    planta = Planta(r[0])
    ctrl.reinicia(r[0], u_equilibrio(r[0]))
    y = np.zeros(N); ym = np.zeros(N); u = np.zeros(N); v = np.zeros(N); tc = np.zeros(N)
    for k in range(N):
        y[k] = planta.saida()
        ym[k] = y[k]
        if ruido:
            ym[k] = np.round((y[k] + rng.normal(0, SIGMA)) / LSB) * LSB
        t0 = time.perf_counter()
        v[k] = ctrl.passo_ctrl(r[k], ym[k])
        tc[k] = time.perf_counter() - t0
        u[k] = min(max(v[k], ctrl.u_min), ctrl.u_max)     # saturação do atuador (Satura no mpc.c)
        ctrl.aplicado(u[k])
        planta.aplica(u[k])
    return dict(t=np.arange(N) * TS, r=r, y=y, ym=ym, u=u, v=v, tc=tc, iters=np.array(ctrl.iters))


def metricas(s, k0=100, k1=300, faixa=0.02):
    """Degrau de subida em k0: acomodação (2 %), sobressinal, erro em regime; esforço de u."""
    y, r, u = s["y"], s["r"], s["u"]
    yf = y[k1 - 20:k1].mean()
    amp = yf - y[k0 - 1]
    fora = np.where(np.abs(y[k0:k1] - yf) > faixa * abs(r[k0] - r[k0 - 1]))[0]
    ts = (fora[-1] + 1) * TS if len(fora) else 0.0
    os_ = 100 * max(0.0, (y[k0:k1].max() - yf) / amp)
    return dict(ts=ts, os=os_, ess=r[k1 - 1] - yf, IAE=np.sum(np.abs(r - y)) * TS,
                std_u=u[k1 - 50:k1].std(), TV=np.sum(np.abs(np.diff(u))), u_max=u.max())


def memoria(ctrl):
    """Floats que o MPC usa de fato (Np, Nu e m deste projeto); o firmware reserva
    os tamanhos máximos (NP_MAX, NU_MAX, M_MAX) — ver o .map."""
    Np, Nu, m = ctrl.Np, ctrl.Nu, len(ctrl.M)
    d = {"A, B, L, x̂": 9 + 3 + 3 + 6, "F": 3 * Np, "Φ": Np * Nu, "H": Nu * Nu}
    if ctrl.metodo == "analitico":
        return {"A, B, L, x̂": 21, "Ky, Kmpc": 4}       # F, Φ, H e H⁻¹ só na inicialização
    if ctrl.metodo == "gradiente":
        d.update({"f, g, ΔU, ∇": Np + 3 * Nu})
    else:
        d.update({"f, g, ΔU": Np + 2 * Nu, "H⁻¹": Nu * Nu, "M": m * Nu, "MH⁻¹": m * Nu, "P": m * m,
                  "γ, d, λ, 1/Pᵢᵢ": 4 * m})
    return d


# ---------------------------------------------------------------------------
# Dados exportados do CCS (para comparar medido × previsto, depois dos ensaios)
# ---------------------------------------------------------------------------

def le_ensaio(caminho):
    """CSV do CCS com rV, uV, yV, tV e nV."""
    import sys
    sys.path.insert(0, str(AQUI.parents[1] / "IdentificacaoSallenKeyPRBS" / "analise"))
    from plota_degrau import le_csv_ccs
    d = le_csv_ccs(caminho)
    d["t"] = np.arange(len(d["yV"])) * TS
    return d
