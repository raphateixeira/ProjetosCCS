"""Identificação ARX do filtro Sallen-Key a partir do ensaio PRBS (SK02PRBS).

Modelo (variáveis de desvio em torno do ponto de operação):

    y(k) = a1 y(k-1) + ... + a_na y(k-na)
         + b1 u(k-nk) + ... + b_nb u(k-nk-nb+1) + e(k)

Estimação por mínimos quadrados nos dados de identificação (primeiros 70 %)
e avaliação nos dados de validação (30 % finais) por predição um passo à
frente e por simulação livre. Para comparação, também o modelo de erro de
saída (OE), que minimiza o erro de simulação.

Uso:
  python identifica_arx.py ../dados/DadosPRBS1ms.csv [--salvar-tabela ../dados/ModelosARX.csv]
  python identifica_arx.py ../dados/DadosPRBS01.csv --ts 0.005 --n-pre 20     (ensaio antigo)
"""

import argparse
import itertools
from dataclasses import dataclass, field

import numpy as np
from scipy import signal
from scipy.linalg import logm
from scipy.optimize import least_squares

import plota_prbs as pp

FRAC_IDENT = 0.7
GRADE_NA = (1, 2)       # 1 polo (só o dominante) ou 2 polos (o filtro)
GRADE_NB = (1, 2, 3)
GRADE_NK = (0, 1, 2)    # o firmware impõe 1 amostra (DAC escrito depois da leitura do ADC); 0 e 2 testam a hipótese
MAX_LAG = 25          # atrasos avaliados na análise de resíduos
MAX_FORA = 3          # pontos fora da banda de 95 % para considerar o resíduo branco (de 25 + 26; ~2,5 ao acaso)


# ---------------------------------------------------------------------------
# Dados
# ---------------------------------------------------------------------------

@dataclass
class Dados:
    t: np.ndarray
    u: np.ndarray        # desvio: u - U_OP
    y: np.ndarray        # desvio: y - y_op
    u_op: float
    y_op: float
    n_ident: int         # amostras [0, n_ident) = identificação; [n_ident, N) = validação
    Ts: float = pp.TS


def prepara(caminho, ts=pp.TS, n_pre=pp.N_PRE, frac_ident=FRAC_IDENT):
    t, u, y = pp.le_prbs(caminho, ts)
    y_op = y[:n_pre].mean()              # ponto de operação medido antes da PRBS
    return Dados(t, u - pp.U_OP, y - y_op, pp.U_OP, y_op, int(frac_ident * len(t)), ts)


# ---------------------------------------------------------------------------
# Modelo ARX
# ---------------------------------------------------------------------------

@dataclass
class ARX:
    na: int
    nb: int
    nk: int
    a: np.ndarray = field(default=None)   # [a1 .. a_na]
    b: np.ndarray = field(default=None)   # [b1 .. b_nb]
    sigma2: float = np.nan                # variância do resíduo (identificação)
    n_eq: int = 0                         # equações usadas na estimação
    tipo: str = "ARX"                     # "ARX" (mínimos quadrados) ou "OE" (erro de saída)

    @property
    def atraso_max(self):
        return max(self.na, self.nk + self.nb - 1)

    @property
    def n_par(self):
        return self.na + self.nb

    @property
    def nome(self):
        return f"{self.tipo}({self.na},{self.nb},{self.nk})"

    def regressores(self, y, u, k):
        """Vetor phi(k) usando y e u medidos."""
        return np.r_[y[k - self.na:k][::-1] if self.na else [],
                     u[k - self.nk - self.nb + 1:k - self.nk + 1][::-1]]

    # ---- estimação --------------------------------------------------------
    def estima(self, y, u):
        m = self.atraso_max
        ks = range(m, len(y))
        Phi = np.array([self.regressores(y, u, k) for k in ks])
        Y = y[m:]
        theta, *_ = np.linalg.lstsq(Phi, Y, rcond=None)
        self.a, self.b = theta[: self.na], theta[self.na:]
        res = Y - Phi @ theta
        self.n_eq = len(Y)
        self.sigma2 = res @ res / len(Y)
        return self

    # ---- uso do modelo ----------------------------------------------------
    def prediz(self, y, u):
        """Predição um passo à frente (usa y medido no passado)."""
        m = self.atraso_max
        yp = np.full(len(y), np.nan)
        theta = np.r_[self.a, self.b]
        for k in range(m, len(y)):
            yp[k] = self.regressores(y, u, k) @ theta
        return yp

    def simula(self, u, y_ini):
        """Simulação livre: usa as próprias saídas simuladas; y_ini = condições iniciais."""
        m = self.atraso_max
        ys = np.zeros(len(u))
        ys[:m] = y_ini[:m]
        theta = np.r_[self.a, self.b]
        for k in range(m, len(u)):
            ys[k] = self.regressores(ys, u, k) @ theta
        return ys

    # ---- propriedades -----------------------------------------------------
    def tf(self, Ts):
        """Função de transferência discreta B(z)/A(z), com o atraso nk."""
        den = np.r_[1.0, -self.a]
        num = np.r_[np.zeros(self.nk), self.b]
        n = max(len(num), len(den))
        return signal.TransferFunction(np.pad(num, (0, n - len(num))),
                                       np.pad(den, (0, n - len(den))), dt=Ts)

    @property
    def polos(self):
        return np.roots(np.r_[1.0, -self.a]) if self.na else np.array([])

    @property
    def estavel(self):
        return bool(np.all(np.abs(self.polos) < 1))

    @property
    def ganho_estatico(self):
        return self.b.sum() / (1 - self.a.sum())

    def constantes_de_tempo(self, Ts):
        """tau = -Ts/ln(z) para polos reais positivos (NaN para os demais)."""
        taus = []
        for z in self.polos:
            taus.append(-Ts / np.log(z.real) if abs(z.imag) < 1e-9 and 0 < z.real < 1 else np.nan)
        return np.sort(np.array(taus))[::-1]


# ---------------------------------------------------------------------------
# Métricas
# ---------------------------------------------------------------------------

def r2(y, yh):
    ok = ~np.isnan(yh)
    y, yh = y[ok], yh[ok]
    return 1 - np.sum((y - yh) ** 2) / np.sum((y - y.mean()) ** 2)


def fit(y, yh):
    """Índice de ajuste (NRMSE, %), como no System Identification Toolbox."""
    ok = ~np.isnan(yh)
    y, yh = y[ok], yh[ok]
    return 100 * (1 - np.linalg.norm(y - yh) / np.linalg.norm(y - y.mean()))


def aic(modelo):
    return modelo.n_eq * np.log(modelo.sigma2) + 2 * modelo.n_par


def avalia(modelo, d):
    """Estima nos dados de identificação e avalia na validação."""
    yi, ui = d.y[: d.n_ident], d.u[: d.n_ident]
    modelo.estima(yi, ui)
    m = modelo.atraso_max
    # validação: começa m amostras antes para ter as condições iniciais medidas
    yv, uv = d.y[d.n_ident - m:], d.u[d.n_ident - m:]
    yp_v = modelo.prediz(yv, uv)[m:]
    ys_v = modelo.simula(uv, yv)[m:]
    e_v = yv[m:] - yp_v
    ree, rue = correlacao(e_v, e_v, MAX_LAG), correlacao(e_v, uv[m:], MAX_LAG)
    banda = 1.96 / np.sqrt(len(e_v))
    yv = yv[m:]
    return dict(
        modelo=modelo, nome=modelo.nome, na=modelo.na, nb=modelo.nb, nk=modelo.nk,
        n_par=modelo.n_par,
        R2_pred_ident=r2(yi, modelo.prediz(yi, ui)),
        R2_pred_val=r2(yv, yp_v), R2_sim_val=r2(yv, ys_v), FIT_sim_val=fit(yv, ys_v),
        AIC=aic(modelo), estavel=modelo.estavel, K=modelo.ganho_estatico,
        fora_ree=int(np.sum(np.abs(ree[1:]) > banda)), fora_rue=int(np.sum(np.abs(rue) > banda)),
    )


def varre(d, grade_na=GRADE_NA, grade_nb=GRADE_NB, grade_nk=GRADE_NK):
    return [avalia(ARX(na, nb, nk), d) for na, nb, nk in itertools.product(grade_na, grade_nb, grade_nk)]


def escolhe(resultados, tolerancia=1.0, nk=1):
    """Critério de escolha:
    1) só modelos estáveis;
    2) FIT de simulação livre na validação a no máximo `tolerancia` pontos
       percentuais do melhor;
    3) atraso `nk` do firmware (u(k) vai ao DAC depois da leitura de y(k));
       nk = 0 e 2 entram na grade só para testar essa hipótese;
    4) entre esses, o de menor AIC (compromisso entre ajuste e número de
       parâmetros).
    Os resíduos são analisados à parte: com Ts = 1 ms nenhum ARX tem resíduo
    branco (o ruído de medição entra filtrado por A(q); ver estima_oe)."""
    estaveis = [r for r in resultados if r["estavel"]]
    melhor = max(r["FIT_sim_val"] for r in estaveis)
    candidatos = [r for r in estaveis if r["FIT_sim_val"] >= melhor - tolerancia and r["nk"] == nk]
    return min(candidatos, key=lambda r: r["AIC"])


# ---------------------------------------------------------------------------
# Modelo de erro de saída (OE)
# ---------------------------------------------------------------------------

def estima_oe(na, nb, nk, d):
    """OE: y(k) = B(q)/A(q) u(k) + v(k), com v branco somado à saída.

    Minimiza o erro de SIMULAÇÃO nos dados de identificação (mínimos quadrados
    não lineares), partindo do ARX de mesma estrutura. Com ruído de medição
    branco, o ARX vê o erro de equação A(q) v(k), colorido, e fica polarizado;
    o OE não."""
    yi, ui = d.y[: d.n_ident], d.u[: d.n_ident]
    arx = ARX(na, nb, nk).estima(yi, ui)
    m = ARX(na, nb, nk, tipo="OE")

    def erro(theta):
        m.a, m.b = theta[:na], theta[na:]
        return m.simula(ui, np.zeros(m.atraso_max)) - yi

    sol = least_squares(erro, np.r_[arx.a, arx.b])
    m.a, m.b = sol.x[:na], sol.x[na:]
    m.n_eq = len(yi)
    m.sigma2 = np.mean(sol.fun ** 2)
    return m


# ---------------------------------------------------------------------------
# Mudança do período de amostragem
# ---------------------------------------------------------------------------

def continuo(modelo, Ts):
    """Modelo contínuo G(s) equivalente por ZOH: num, den em s.

    Vale para nk = 1 e nb <= na (estrutura de um sistema contínuo sem atraso
    discretizado com segurador de ordem zero — a do firmware: u(k) entra no DAC
    logo depois da leitura de y(k)). Usa exp([[Ac, Bc], [0, 0]] Ts) = [[Ad, Bd], [0, 1]]."""
    assert modelo.nk == 1 and modelo.nb <= modelo.na
    num = np.r_[modelo.b, np.zeros(modelo.na - modelo.nb)]      # b1 z^(na-1) + ... (grau na-1)
    den = np.r_[1.0, -modelo.a]
    Ad, Bd, Cd, Dd = signal.tf2ss(num, den)
    n = len(Ad)
    M = np.zeros((n + 1, n + 1))
    M[:n, :n], M[:n, n:] = Ad, Bd
    M[n, n] = 1.0
    L = np.real(logm(M)) / Ts
    numc, denc = signal.ss2tf(L[:n, :n], L[:n, n:], Cd, Dd)
    return np.ravel(numc), denc


def reamostra(modelo, Ts, Ts_novo):
    """Mesmo sistema, outro período de amostragem (ZOH): TransferFunction em Ts_novo."""
    numc, denc = continuo(modelo, Ts)
    return discretiza(numc, denc, Ts_novo)


# ---------------------------------------------------------------------------
# Análise de resíduos
# ---------------------------------------------------------------------------

def correlacao(x, z, max_lag=25):
    """Correlação cruzada normalizada r_xz(tau), tau = 0..max_lag (z atrasado)."""
    x = x - x.mean(); z = z - z.mean()
    n = len(x)
    den = np.sqrt(np.sum(x**2) * np.sum(z**2))
    return np.array([np.sum(x[tau:] * z[: n - tau]) / den for tau in range(max_lag + 1)])


# ---------------------------------------------------------------------------
# Modelos de referência (contínuos, discretizados por ZOH)
# ---------------------------------------------------------------------------

def discretiza(num, den, Ts):
    nd, dd, _ = signal.cont2discrete((num, den), Ts, method="zoh")
    return signal.TransferFunction(np.ravel(nd), dd, dt=Ts)


def simula_tf(tf, u):
    _, y = signal.dlsim(tf, u)
    return np.ravel(y)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", help="CSV do CCS do ensaio PRBS")
    ap.add_argument("--salvar-tabela", help="salva a tabela de modelos em CSV")
    ap.add_argument("--ts", type=float, default=pp.TS, help="período de amostragem [s]")
    ap.add_argument("--n-pre", type=int, default=pp.N_PRE, help="amostras paradas antes da PRBS")
    args = ap.parse_args()

    d = prepara(args.csv, args.ts, args.n_pre)
    res = varre(d)
    cols = ["nome", "n_par", "R2_pred_ident", "R2_pred_val", "R2_sim_val", "FIT_sim_val", "AIC", "K",
            "fora_ree", "fora_rue", "estavel"]
    print(f"{len(d.t)} amostras: identificação {d.n_ident}, validação {len(d.t) - d.n_ident}")
    print(" ".join(f"{c:>13}" for c in cols))
    for r in sorted(res, key=lambda r: -r["FIT_sim_val"] if r["estavel"] else np.inf):
        print(" ".join(f"{r[c]:>13.4f}" if isinstance(r[c], float) else f"{str(r[c]):>13}" for c in cols))
    esc = escolhe(res)
    m = esc["modelo"]
    print(f"\nescolhido: {m.nome}  a = {m.a}  b = {m.b}  polos = {m.polos}  "
          f"tau = {m.constantes_de_tempo(d.Ts)}  K = {m.ganho_estatico:.4f}")

    if args.salvar_tabela:
        import csv
        with open(args.salvar_tabela, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(cols)
            for r in res:
                w.writerow([r[c] for c in cols])
        print(f"tabela salva em {args.salvar_tabela}")


if __name__ == "__main__":
    main()
