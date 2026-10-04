# ProjetosCCS

Projetos do **Code Composer Studio (CCS)** para a família TI C2000, desenvolvidos pelo
Prof. Dr. Raphael Barros Teixeira (UFPA-CAMTUC-FEE) com a placa **LAUNCHXL-F28379D**
(LaunchPad C2000 Delfino).

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `Notas/` | Notas em Quarto (`.qmd`) sobre a placa, periféricos e projetos — uma por tema, numeradas `NotaNN<Tema>.qmd`. |
| `_templates/` | Modelo de nota (`nota-modelo.qmd`). |
| `imgs/` | Figuras compartilhadas (logo UFPA). |
| `ProjetosSK/` | **Só projetos do CCS** do filtro Sallen-Key, um por ensaio (`SK01Degrau/`, `SK02PRBS/`, `SK03ControleP/`, `SK04ControlePI/`, `SK05Senoide/`, `SK06VarreduraSenoidal/`). Abrir a pasta no CCS como workspace (*File → Open Folder*). Depende da C2000Ware em `~/ti/`. |
| `Notas/<Tema>/` | Notas com ensaios: `index.qmd` (apresentação), `dados/`, `analise/`, `imgs/`. |

Artefatos de build do CCS (`Debug/`, `CPU1_RAM/`, `CPU1_FLASH/`, `*.out`, `.metadata/`)
não são versionados (ver `.gitignore`).

## Renderizar localmente

```sh
quarto preview      # ou: quarto render
```

O tema `TemaRTx.scss` não é versionado: é copiado/baixado de `raphateixeira/.github`
(ver `GitRTx/.github/scripts/atualizar-tema.sh`).

🔗 Site: https://raphateixeira.github.io/ProjetosCCS/
