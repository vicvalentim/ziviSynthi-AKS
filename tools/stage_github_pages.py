"""Stage the ziviSynthi AKS website and verified SMK-37 Pro downloads."""
import argparse
import hashlib
from html import escape
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from urllib.parse import urlsplit
from zipfile import ZipFile

from render_github_pages import render_site

ROOT = Path(__file__).resolve().parents[1]
EDITOR = "zivisynthi-aks-editor.html"
SOURCE = "zivisynthi-aks-source.zip"
BASE = "fm1-base-source.tar.gz"
PRESETS = "zivisynthi-aks-presets.zip"


def https_url(value):
    url = urlsplit(value.strip())
    if url.scheme != "https" or not url.hostname or url.username or url.password or url.query or url.fragment:
        raise argparse.ArgumentTypeError("Informe uma URL HTTPS sem credenciais, consulta ou fragmento.")
    return value.strip().rstrip("/")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifacts", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--site", type=https_url, required=True)
    parser.add_argument("--source", type=https_url, required=True)
    args = parser.parse_args()

    manifest = json.loads((args.artifacts / "firmware-smk37.json").read_text(encoding="utf-8"))
    if manifest.get("license") != "GPL-3.0-only":
        parser.error("O catálogo deve indicar GPL-3.0-only.")
    package_pattern = re.compile(r"zivisynthi-felucca-smk37-(\d+\.\d+\.\d+)\.fwsc")
    packages = [p for p in args.artifacts.glob("*.fwsc") if package_pattern.fullmatch(p.name)]
    if len(packages) != 1:
        parser.error("Esperado um pacote de firmware verificado para SMK-37 Pro.")
    package = packages[0].name
    version = package_pattern.fullmatch(package)[1]
    html = (args.artifacts / EDITOR).read_text(encoding="utf-8")
    downloads_marker = "<!-- PAGES_DISTRIBUTION -->"
    links_marker = "<!-- PAGES_PROJECT_LINKS -->"
    if html.count(downloads_marker) != 1 or html.count(links_marker) != 1:
        parser.error("Gere o editor a partir do código atual antes de montar o site.")

    # Publish the exact application snapshot and the patched base used by the build.
    subprocess.run(["git", "diff", "--exit-code", "HEAD", "--"], cwd=ROOT, check=True)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    args.output.mkdir(parents=True, exist_ok=True)
    studio = args.output / "studio"
    studio.mkdir(exist_ok=True)
    source = args.output / SOURCE
    subprocess.run(["git", "archive", "--format=zip", "--prefix=ziviSynthi-aks/",
                    "--output", str(source.resolve()), "HEAD"], cwd=ROOT, check=True)
    with ZipFile(source, "a") as archive:
        archive.write(args.artifacts / BASE, BASE)

    downloads = ('<div class="firmware-links" aria-label="Downloads da versão">\n'
                 f'              <a href="../{package}" download>Baixar firmware (.fwsc)</a>\n'
                 f'              <a href="../{PRESETS}" download>Baixar presets</a>\n'
                 f'              <a href="../{SOURCE}" download>Código-fonte correspondente (GPL-3.0-only)</a>\n'
                 '              <a href="../SHA256SUMS.txt">SHA-256</a>\n'
                 '              <a href="../LICENSE">Licença / License</a>\n'
                 '              <a href="../docs/NOTICES.html">Avisos / Notices</a>\n'
                 '            </div>')
    project_url = escape(args.source, quote=True)
    links = ('<a href="../">Início</a>\n'
             '      <a href="../docs/">Documentação</a>\n'
             f'      <a href="{project_url}">GitHub</a>')
    page = html.replace(downloads_marker, downloads).replace(links_marker, links)
    (studio / "index.html").write_text(page, encoding="utf-8")
    subprocess.run([sys.executable, ROOT / "tools/build_web.py", args.output / EDITOR], check=True)

    for name in (package, PRESETS, "LICENSE", "NOTICES.md"):
        shutil.copyfile(args.artifacts / name, args.output / name)
    screenshot = args.output / manifest["screenshot"]
    screenshot.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / manifest["screenshot"], screenshot)
    manifest.update(site=args.site + "/", install=args.site + "/studio/#firmware-drawer", source=args.source)
    (args.output / "firmware.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    render_site(ROOT, args.output, args.source, commit, manifest, version, (EDITOR, package, PRESETS, SOURCE))
    with (args.output / "SHA256SUMS.txt").open("w", encoding="utf-8") as checksums:
        for path in sorted(args.output.rglob("*")):
            if path.is_file() and path.name != "SHA256SUMS.txt":
                checksums.write(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(args.output).as_posix()}\n")


if __name__ == "__main__":
    main()
