"""Empacota cada projeto do CCS num .zip para download no site.

Roda antes de cada renderização (pre-render no _quarto.yml), localmente e no GitHub.
Para cada pasta Projetos*/<Projeto>/ que tem um .project (projeto do CCS), grava
downloads/<Projeto>.zip só com os fontes e as configurações do projeto: ficam de fora
os artefatos de build, a cópia da driverlib e os arquivos do sistema.

O .zip abre direto no CCS: descompactar e abrir a pasta (File > Open Folder) ou
importar o projeto (File > Import Projects).
"""

from pathlib import Path
import zipfile

RAIZ = Path(__file__).resolve().parent.parent
SAIDA = RAIZ / "downloads"

PASTAS_FORA = {"Debug", "Release", "CPU1_RAM", "CPU1_FLASH", "CPU2_RAM", "CPU2_FLASH",
               ".metadata", ".launches", ".clangd", "__pycache__"}
EXTENSOES_FORA = {".obj", ".out", ".map", ".pp", ".d", ".zip"}
ARQUIVOS_FORA = {".DS_Store", "org.eclipse.cdt.codan.core.prefs", "readme.txt", ".clangd"}


def incluir(arquivo: Path, projeto: Path) -> bool:
    rel = arquivo.relative_to(projeto)
    if any(p in PASTAS_FORA for p in rel.parts[:-1]):
        return False
    if rel.parts[:2] == ("device", "driverlib"):     # a build usa a driverlib da C2000Ware
        return False
    return arquivo.suffix not in EXTENSOES_FORA and arquivo.name not in ARQUIVOS_FORA


def empacota(projeto: Path) -> Path:
    destino = SAIDA / f"{projeto.name}.zip"
    with zipfile.ZipFile(destino, "w", zipfile.ZIP_DEFLATED) as z:
        for arquivo in sorted(projeto.rglob("*")):
            if arquivo.is_file() and incluir(arquivo, projeto):
                z.write(arquivo, Path(projeto.name) / arquivo.relative_to(projeto))
    return destino


def main() -> None:
    SAIDA.mkdir(exist_ok=True)
    for projeto in sorted(RAIZ.glob("Projetos*/*/.project")):
        empacota(projeto.parent)
        print(f"downloads/{projeto.parent.name}.zip")


if __name__ == "__main__":
    main()
