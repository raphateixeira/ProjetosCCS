# CLAUDE.md

Contexto para sessões futuras do Claude Code neste repositório.

## O que é este repositório

`ProjetosCCS` é um satélite do hub acadêmico do Prof. Dr. Raphael Teixeira
(`raphateixeira.github.io`, UFPA-CAMTUC-FEE), publicado em
<https://raphateixeira.github.io/ProjetosCCS/>. Guarda os projetos do Code Composer Studio
(TI C2000) com a placa **LAUNCHXL-F28379D** — diversos projetos, não ligados a uma
aplicação específica — e as notas em Quarto que os documentam. Tema visual: `TemaRTx.scss`
(compartilhado, não versionado — não inventar paleta própria).

## Organização

- `Notas/` — notas numeradas (Nota 01, Nota 02, …), todas no menu **Notas** do `_quarto.yml`
  e na seção Notas do `index.qmd` (não há mais seção/menu "Projetos").
  - Nota simples: arquivo `NotaNN<Tema>.qmd` (front matter conforme `_templates/nota-modelo.qmd`).
  - Nota com ensaios: pasta `Notas/<Tema>/` com `index.qmd` = **uma apresentação revealjs**
    (Python/Jupyter), `dados/` (exportações do CCS), `analise/` (scripts Python), `imgs/`.
    Ex.: Nota 02 em `Notas/IdentificacaoSallenKeyPRBS/`.
  - Nota de planejamento (muito texto, equações e tabelas): pasta `Notas/<Tema>/` com
    `index.qmd` em **HTML** (documento, `code-fold`), não revealjs. Ex.: Nota 03 em
    `Notas/ControleSallenKey/` (simulações em `analise/controle_sim.py`).
  - Conteúdo geral da placa (DAC/ADC, amostragem, exportação do CCS) fica na Nota 01; as
    demais notas só remetem a ela.
- `ProjetosSK/` — **somente projetos do CCS** do filtro Sallen-Key, **um por ensaio, simples e
  sem configuração pela WATCH** (o ensaio roda sozinho ao gravar; parâmetros como `#define`):
  `SK01Degrau/` (onda quadrada 0,6↔1,4 V, Ts = 1 ms, 2000 amostras), `SK02PRBS/` (PRBS 1,0±0,4 V,
  Ts = 1 ms, Tb = 5 ms, 2000 amostras) — esses dois já no padrão refatorado: `main.c` com os
  parâmetros em unidades físicas (struct), `placa.c/.h` (`ConfiguraF28379D`, `ADC2y`, `DAC2u`,
  `FimAmostragem`), `ensaio.c/.h`, ISR `LacoAmostragem`, buffers `uV`/`yV`; `SK03ControleP/`, `SK04ControlePI/`, `SK05Senoide/` (uma senoide, 5 Hz,
  1,0±0,3 V, 800 amostras), `SK06VarreduraSenoidal/` (resposta em frequência: seno em degraus
  de 0,5 a 50 Hz com *lock-in* na placa; exporta `freq_hz`, `ganho`, `fase_graus` (15 valores) e
  `y_buf` da varredura inteira (8730 amostras, região RAMGS7..RAMGS11 unida no `.cmd`; u é
  reconstruído por `Notas/IdentificacaoSallenKeyPRBS/analise/varredura.py`).
  `SK07MPCNumerico/`, `SK08MPCAnalitico/`, `SK09MPCRestricoes/` (Nota 06, `Notas/ControlePreditivo/`,
  apresentação revealjs que, por ora, trata **só do SK07**). SK07 = "MPC convencional", versão
  didática: ARX(2,2,1) da Nota 02 nas variações (Δy, Δu), previsão por simulação a partir das
  medidas, J = Σ(r−ŷ)² + λΣΔu², gradiente com `nIter` iterações fixas por amostra (Np = 60,
  Nu = 4, λ = 3, nIter = 50); réplica em `analise/convencional.py`. Na apresentação, evitar
  termos de variantes (GPC, resposta livre, observador, espaço de estados, Q/R, deadbeat, "de
  livro"). SK08 (analítico) e SK09 (Hildreth) ainda usam a formulação em espaço de estados com
  observador (réplica em `analise/mpc.py`) e estão fora da apresentação; `dados/DadosMPCAnalitico01.csv`
  é o ensaio real do SK08. Padrão SK01 mais `mpc.c/.h`; medem ciclos com o CPU Timer 1
  (`Cronometro()` em `placa.h`, `struct desempenho`), gravam `rV`, `uV`, `yV`, `tV` (µs), `nV`,
  800 amostras; matrizes em `mpcdados` (RAMGS2..RAMGS5 unidos no `.cmd`), código em RAMLS0..RAMLS4
  unidos; compilados com `-O2`. Manter as réplicas em Python em sincronia com os `mpc.c`.
  **Cuidado com `-O2`:** o `Device_init` chama `SysCtl_deviceCal()` → `Device_cal` (OTP 0x070282),
  que altera XAR1 sem restaurar; com otimização, argumentos guardados em XAR1 durante essa
  chamada se perdem (o vetor da ISR saía 0x70000 → ITRAP). Por isso `placa.c` dos projetos MPC
  copia `Ts` e o ponteiro da ISR para variáveis `volatile` antes do `Device_init`.
  Buffers (`uV`/`yV` ou `u_buf`/`y_buf` nos projetos ainda não refatorados) em `ramgs0`/`ramgs1`. É a pasta aberta como
  *workspace* no CCS (*File → Open Folder*), com `.theia/` (configurações de depuração).
  Nenhum arquivo de conteúdo aqui: dados exportados vão para `Notas/<Tema>/dados/`.
  Só renomear/mover essa pasta com o CCS fechado; depois, reabrir a pasta e fazer Clean + Rebuild.
- Nunca versionar artefatos de build
  (`Debug/`, `CPU1_RAM/`, `CPU1_FLASH/`, `*.out`, `.metadata/`) — já estão no `.gitignore`.
- Projetos da LaunchPad usam a C2000Ware/driverlib com o símbolo `_LAUNCHXL_F28379D`.
