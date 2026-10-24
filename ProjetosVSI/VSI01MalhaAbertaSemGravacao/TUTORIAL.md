# VSI01MalhaAbertaSemGravacao: primeiro teste do inversor, sem gravação (LINCE/UFPA)

LAUNCHXL-F28379D + BOOSTXL-3PHGANINV (nos conectores J1–J4). O programa gera o PWM das três
fases, sem medir nada e sem controle. É a versão testada na placa em 23/10/2026; a versão
com gravação das tensões é o projeto `VSI01MalhaAberta`. A apresentação "Inversor Trifásico em Malha Aberta", no
site dos projetos, tem as figuras da montagem e do que se espera ver em cada passo.

## 1. Dados da placa inversora (guia SLUUBP1A e TIDA-00909)

- Barramento de 12 a 60 V (nominal 48 V), até 7 A RMS por fase.
- Corrente: shunt de 5 mΩ + INA240 (20 V/V), 0,1 V/A em torno de 1,65 V, com sinal invertido.
- Proteção de sobrecorrente própria, em 12 A.
- Habilitação do PWM (/PWM enable) **ativa em nível baixo**, no GPIO124 (pino J2-13).
- O inversor só funciona com o barramento ligado: os 5 V e 3,3 V dele vêm da própria fonte.
- Jumper **J6**: leva 3,3 V do inversor para a LaunchPad. **Retire o J6**, porque a LaunchPad
  é alimentada pela USB.

## 2. Montagem

1. Tudo desligado (sem USB, sem fonte).
2. Retire o J6 do inversor.
3. Fita Kapton entre as placas: o inversor passa por cima de J5–J8 da LaunchPad.
4. Confira a revisão da LaunchPad: na **1.1**, o pino J3-29 (corrente da fase C) está em curto
   com a referência do ADC. Nesse caso, fale com o professor antes de encaixar.
5. Encaixe nos conectores **J1–J4** (perto da USB), com **3V3 sobre 3V3** (pino 1 de J1).
6. Carga RL em estrela (4,7 Ω e 1 mH por fase) nos bornes VA, VB, VC; fonte nos bornes +48V e
   GND (usaremos 12 V).

## 3. No CCS

1. *File → Open Folder*: a pasta `ProjetosVSI` (ou a pasta deste `.zip`).
2. Compile o `VSI01MalhaAberta` (*Build*) e inicie a depuração (*Debug*).
3. Na janela **Watch**: `modo`, `habilita`, `indice`, `frequencia`, `razaoManual`,
   `razaoCiclica`. Modos: 0 = 50 %, 1 = 25/50/75 %, 2 = senoidal, 3 = manual (a razão de cada
   fase em `razaoManual[0..2]`, de 0,05 a 0,95).
4. Para mudar um valor: *Suspend*, digite o valor, *Resume*. Com o programa pausado, as
   saídas de PWM vão para 0 (proteção).

## 4. Pinos de PWM (J4)

| Pino | GPIO | Sinal | Fase |
|---|---|---|---|
| 40 | GPIO0 | EPWM1A | A, chave de cima |
| 39 | GPIO1 | EPWM1B | A, chave de baixo |
| 38 | GPIO2 | EPWM2A | B, chave de cima |
| 37 | GPIO3 | EPWM2B | B, chave de baixo |
| 36 | GPIO4 | EPWM3A | C, chave de cima |
| 35 | GPIO5 | EPWM3B | C, chave de baixo |

Terra das pontas: J2-20 ou J3-22 (GND).

## 5. Etapa A: só a LaunchPad

1. `modo = 0`, J4-40 e J4-39: 20 kHz, 50 %, um o inverso do outro.
2. Zoom numa borda (50 ns/div): os dois em 0 por ≈ 100 ns (tempo morto); nunca os dois em 1.
3. `modo = 1`: J4-40, J4-38 e J4-36 com 25 %, 50 % e 75 %.
4. `modo = 2`, 5 ms/div: largura variando como senoide (60 Hz, 120° entre as fases).

## 6. Etapa B: com o inversor

1. Sem carga, programa rodando com `habilita = 0` e `modo = 0`.
2. Fonte em 12 V com limite de 0,5 A; ligar; LEDs 5V e 3V3 do inversor acendem.
3. `habilita = 1`. Ponta em VA, garra de terra **só no GND** (nunca em VA, VB ou VC).
4. Esperado: pulsos de 0 a 12 V com a razão cíclica do modo.
5. Desligue a fonte, ligue a carga, religue. Modo 0: corrente nula. Modo 1: −0,64 A, 0 e
   +0,64 A contínuos (fonte ≈ 0,32 A). Modo 2, índice 0,5: 0,64 A de pico a 60 Hz.
6. Corrente da fase A em J3-27: 1,65 − 0,1·iA [V]. Tensão de VA filtrada em J3-24: VA/24,7.
7. Para desligar: `habilita = 0`, fonte, USB.

