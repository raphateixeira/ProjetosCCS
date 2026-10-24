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

- `Notas/` — notas **sem número** (identificadas só pelo título; não usar "Nota 01", "Nota 02"…
  em títulos nem em referências cruzadas: escrever o nome, ex. "[identificação do Sallen-Key](…)").
  O `index.qmd` tem uma seção `##` por grupo: **LaunchPad F28379D** (ferramentas + nota da placa,
  `Nota01LaunchPadF28379D.qmd` — o nome do arquivo ficou), **Filtro Sallen-Key**, **Conversor
  Buck**, **Motor-Gerador** (`ProjetosMG/`, ainda vazio). Cada sistema tem **uma única nota do
  sistema** (documento HTML extenso: o sistema, o modelo, os experimentos), **apresentações
  revealjs** com os ensaios (identificação e controle) e a tabela **Projetos do CCS** com o
  `.zip` de cada projeto. Os mesmos grupos ficam no **menu superior à direita** (`navbar.right`
  do `_quarto.yml`: um menu por sistema, com cabeçalhos "Nota do sistema"/"Apresentações" e
  "Projetos do CCS" → âncora da seção no índice). Atualmente: SK → nota do sistema =
  Planejamento dos Ensaios de Controle (`ControleSallenKey/`); apresentações: Identificação,
  Controle P, Controle PI, Controle Preditivo. Buck → nota do sistema em preparação;
  apresentação: Identificação do Conversor Buck. Inversor → apresentações Inversor Trifásico
  em Malha Aberta (`Notas/InversorMalhaAberta/`, VSI01) e Controle de Corrente
  do Inversor Trifásico (`Notas/ControleCorrenteVSI/`, revealjs com figuras recortadas dos PDFs
  da TI em `imgs/ti_*.png`; os PDFs ficam em `docs/`, no `.gitignore`).
- **Códigos para download**: o repositório é privado. `scripts/empacota_projetos.py` roda como
  `pre-render` e grava `downloads/<Projeto>.zip` de cada `Projetos*/<Projeto>/` com `.project`
  (só fontes e configurações; sem `Debug/`, `CPU1_*`, `device/driverlib/`), publicados pelo
  `resources: downloads/*.zip`. `downloads/` está no `.gitignore`. Projeto novo: acrescentar a
  linha na tabela do sistema no `index.qmd` (só entra no site depois de versionado).
  - Nota simples: arquivo `NotaNN<Tema>.qmd` (front matter conforme `_templates/nota-modelo.qmd`).
  - Nota com ensaios: pasta `Notas/<Tema>/` com `index.qmd` = **uma apresentação revealjs**
    (Python/Jupyter), `dados/` (exportações do CCS), `analise/` (scripts Python), `imgs/`.
    Ex.: identificação do SK em `Notas/IdentificacaoSallenKeyPRBS/` — reescrita curta (16 slides),
    revealjs **sem Jupyter**: `analise/ident_sk.m` (MATLAB) gera `imgs/sk_*.png` e os números;
    dados `dados/DadosPRBSSK01.csv` e `DadosDegrauSK01.csv` (ensaios do Kauã, Ts = 1 ms, decimados
    para 5 ms). Conclusão: com Ts = 1 ms o ARX(2,2,1) erra K e o polo lento (mal condicionado;
    a subida tem um salto de 0,17 V + patamar de ≈ 10 ms, não linear); com 5 ms, ARX = OE
    (K = 1,00, τ = 29 e 7,9 ms). Os .py antigos de `analise/` ficaram, sem uso na nota.
  - Nota de planejamento (muito texto, equações e tabelas): pasta `Notas/<Tema>/` com
    `index.qmd` em **HTML** (documento, `code-fold`), não revealjs. Ex.: planejamento do SK em
    `Notas/ControleSallenKey/` (simulações em `analise/controle_sim.py`).
  - Conteúdo geral da placa (DAC/ADC, amostragem, exportação do CCS) fica na nota da placa; as
    demais notas só remetem a ela.
- `ProjetosSK/` — **somente projetos do CCS** do filtro Sallen-Key, **um por ensaio, simples e
  sem configuração pela WATCH** (o ensaio roda sozinho ao gravar; parâmetros como `#define`):
  `SK01Degrau/` (onda quadrada 0,6↔1,4 V, Ts = 1 ms, 2000 amostras), `SK02PRBS/` (PRBS 1,0±0,4 V,
  **Ts = 5 ms, Tb = 20 ms (Nb = 4)**, 2000 amostras = 10 s; antes 1 ms / 5 ms, que deu ARX ruim) — esses dois já no padrão refatorado: `main.c` com os
  parâmetros em unidades físicas (struct), `placa.c/.h` (`ConfiguraF28379D`, `ADC2y`, `DAC2u`,
  `FimAmostragem`), `ensaio.c/.h`, ISR `LacoAmostragem`, buffers `uV`/`yV`; `SK03ControleP/`, `SK04ControlePI/`, `SK05Senoide/` (uma senoide, 5 Hz,
  1,0±0,3 V, 800 amostras), `SK06VarreduraSenoidal/` (resposta em frequência: seno em degraus
  de 0,5 a 50 Hz com *lock-in* na placa; exporta `freq_hz`, `ganho`, `fase_graus` (15 valores) e
  `y_buf` da varredura inteira (8730 amostras, região RAMGS7..RAMGS11 unida no `.cmd`; u é
  reconstruído por `Notas/IdentificacaoSallenKeyPRBS/analise/varredura.py`).
  `SK07MPCNumerico/`, `SK08MPCAnalitico/`, `SK09MPCRestricoes/` (nota de controle preditivo, `Notas/ControlePreditivo/`,
  apresentação revealjs que, por ora, trata **só do SK07**). SK07 = "MPC convencional", versão
  didática: ARX(2,2,1) da identificação do SK nas variações (Δy, Δu), previsão por simulação a partir das
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
- `ProjetosVSI/` — projetos do CCS do inversor trifásico (LAUNCHXL-F28379D + **BOOSTXL-3PHGANINV**
  em J1–J4: ePWM1/2/3 = braços A/B/C em GPIO0–5, GPIO124 = /PWM enable **ativo em nível
  baixo** (0 liga o buffer; o VSI01 antigo estava invertido), como no
  MotorControl SDK `solutions/boostxl_3phganinv/f2837x`). Sem barramento, o BoosterPack fica
  sem 5 V/3,3 V (LM5017): INA240 e buffer desligados, os comparadores disparam. Outro *workspace* do CCS (pasta própria com `.theia/`).
  `VSI01MalhaAberta/` = **primeiro experimento, para iniciantes** (reescrito em 2026-10-23 a
  partir de uma proposta do professor, revisada): **um `main.c` só**, nomes descritivos em
  português (`TOPO`, `TEMPO_MORTO`, `CONTAGENS_MORTO`, `razaoCiclica[3]`, `AtualizaPWM`,
  `ConfiguraModuloPWM`, `EscreveRazao`, `ConfiguraPinos`), variáveis na janela **Watch**
  (`modo` 0 = 50 % / 1 = 25-50-75 % / 2 = senoidal, `habilita`, `indice`, `frequencia`).
  fPWM = 20 kHz, tempo morto 100 ns, razão limitada a 5–95 %, CBC6 (parada do emulador) força
  A e B em 0, contadores sincronizados só pelo TBCLKSYNC (a cadeia de sync da proposta foi
  retirada: faltava `setCountModeAfterSync`). Mede as tensões das fases do BoosterPack
  (ADCINC3/B3/A3 = vA/vB/vC, ×24,7, filtro de 1 kHz) e o barramento (ADCIN14, ADCD), SOCA do
  EPWM1 no zero, lidos na ISR depois do ADCINT1; `grava` = 1 → 2 (gravando) → 0 enche
  `tensaoA/B/C` (2000 floats, `ramgs0..2`), exportados da Watch para CSV e lidos por
  `Notas/InversorMalhaAberta/analise/plota_tensoes.py`. Com o ADC, o `.text` do `main.obj`
  passou de 2 K palavras: `--gen_func_subsections=on` no `.cproject` (como no VSI02).
  `VSI01MalhaAbertaSemGravacao/` = cópia da versão **testada na placa** em 2026-10-23 (modos
  0–3, sem ADC; código de máquina do `main.obj` idêntico ao build que funcionou). Na versão com
  ADC, as funções do PWM (`ConfiguraPinos`, `ConfiguraModuloPWM`, `EscreveRazao`) têm código de
  máquina idêntico ao testado; só entraram as chamadas a `ConfiguraADC`/`LeTensoes`. `TUTORIAL.md` na
  pasta. Apresentação `Notas/InversorMalhaAberta/` (revealjs, 31 slides, figuras e GIFs gerados
  por `analise/figuras.py`, inclusive a montagem das placas a partir das figuras da TI e a
  simulação da carga RL a 12 V). Modo 3 = razão manual (`razaoManual[3]`). A apresentação
  inclui o mapa dos pinos (`imgs/pinos_launchpad.png`, serigrafia da SPRUI77C fig. 18 girada) e
  um filtro RC de 10 kΩ/100 nF (corte de 159 Hz) para ver a senoide do modo 2, nos pinos J4 ou
  nos bornes VA/VB/VC.
  `VSI02ControleCorrente/`: carga RL (4,7 Ω, 1 mH, estrela); estrutura pedida pelo professor
  (`main.c`, `hal.c/h`, `controle.c/h`, `tarefas.c/h`, não `placa.c`). ISR = ADCA INT1 (SOCA do
  EPWM1 no zero, a cada `nControle` períodos); correntes ADCINC2/B2/A2 (iA/iB/iC), vA/vB/vC em
  C3/B3/A3, Vdc em ADCIN14 (ADCD); i = −7,32 mA·(N − N0) (REF5030 de 3,0 V, INA240 0,1 V/A,
  invertido), V = 24,7·vADC; CMPSS6/3/1 + GPIO24 → TRIP4 → DCAEVT1 → OST; limiar em torno do
  offset medido. Níveis 1 (50 %), 2 (malha aberta), 3 (PI dq + desacoplamento, kp = 2πfcL,
  ki = 2πfcR), 4 (identificação: PRBS 9 bits/degrau/multisseno Schroeder em v_dq). Registro de 4
  canais × 2000 em `ramgs0..3`, multisseno em `ramgs4`. UART SCIA 115200 com comandos de texto
  (S, H, D, L, C, N, F, V, R, P, E, I, M, G, B; ver `tarefas.h`). Precisa de
  `--gen_func_subsections=on` (no `.cproject`); no `.cmd` de FLASH o `.text` não usa a FLASHB.
  Compilado fora do CCS com o cl2000 de `/Applications/Texas/ccs2101` (RAM e FLASH linkam).
  Pendentes: script Python do PC (Etapa 3) e `TUTORIAL.md` (Etapa 4).
- `ProjetosBuck/` — projetos do CCS do conversor buck da IC (`GitRTx/ControleBuck`: Vin = 30 V,
  L = 525 µH, C = 41,7 µF, R = 1 Ω, MOSFET + diodo → uma chave só). Outro *workspace* do CCS
  (pasta própria com `.theia/`). Saída da placa por **PWM** (EPWM1A, GPIO0, J4.40), não pelo DAC;
  y no ADCINA3 (J3.26) via divisor `kSensor`; ADC disparado pelo SOCA do ePWM1 (contador = 0).
  Dois structs no `main.c`: `ParametrosConversor` (fPWM, Ts, Vin, uMax, kSensor) e
  `ParametrosDegrau`. `BK01Degrau/`: malha aberta, u = d·Vin; rampa 0→10 V (0,5 s),
  acomodação (0,5 s), onda quadrada 10↔12 V (Tq = 20 ms), fPWM = 20 kHz, Ts = 50 µs,
  grava `uV` (tensão comandada) e `yV` (2000 amostras, `ramgs0`/`ramgs1`).
  `PWM_prbs/` (projeto do aluno, nome interno `Gpio_PWM`, **não renomear arquivos**): registradores
  bitfield, laço por *polling* do ADCINT1, PRBS de 9 bits [9 5 0] com `PRBS_NB` = 4 (Tb = 200 µs),
  Ts = 50 µs, `TAM_EXP` = 2044 (um período), `DadosBuck[3][TAM_EXP]` (t, d, y). Arquivos em
  ISO-8859-1 com CRLF (preservar); caminhos da C2000Ware apontam para `D:\TexasInstruments` (Windows).
  Nota de identificação do buck (`Notas/IdentificacaoBuckPRBS/`, revealjs **sem Jupyter**: código em MATLAB,
  `analise/ident_buck.m` gera as figuras em `imgs/`; números dos slides copiados da saída do
  script): modelo médio G(s) = 1,532e9/(s²+2400s+5,106e7) (entrada d), Ts = 50 µs, PRBS de
  9 bits com Nb = 4, ARX(2,2,2) (atraso extra do CMPA com sombra), ruído de medida σ = 20 mV,
  validação em outro trecho da PRBS e na onda quadrada do BK01. Sem slides de roteiro nem
  divisores de seção. Notação: D = D0 + d, Vo = Vo0 + y (d e y sem til no ARX). Cabeçalhos de tabela do tema ficam em maiúsculas: unidades/math no cabeçalho vão em
  `[...]{style="text-transform:none"}`.
- Nunca versionar artefatos de build
  (`Debug/`, `CPU1_RAM/`, `CPU1_FLASH/`, `*.out`, `.metadata/`) — já estão no `.gitignore`.
- Projetos da LaunchPad usam a C2000Ware/driverlib com o símbolo `_LAUNCHXL_F28379D`.
