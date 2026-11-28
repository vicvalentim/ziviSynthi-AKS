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
EDITOR_PT = "zivisynthi-aks-editor-pt.html"
SOURCE = "zivisynthi-aks-source.zip"
BASE = "fm1-base-source.tar.gz"
PRESETS = "zivisynthi-aks-presets.zip"


def checksum(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy_release(artifacts, output, names):
    sums = (artifacts / "SHA256SUMS.txt").read_text(encoding="utf-8")
    entries = [line.split(maxsplit=1) for line in sums.splitlines() if line]
    if len(entries) != len(names) or any(len(entry) != 2 for entry in entries):
        raise ValueError("Expected exactly one SHA-256 entry for each release asset")
    published = {name: sha for sha, name in entries}
    if set(published) != set(names):
        raise ValueError("Unexpected file names in the published release checksums")
    for name, expected in published.items():
        if checksum(artifacts / name) != expected:
            raise ValueError(f"Published checksum mismatch: {name}")
    for name in (*names, "SHA256SUMS.txt"):
        shutil.copyfile(artifacts / name, output / name)


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
    parser.add_argument("--published", action="store_true",
                        help="reuse the original downloads and checksum manifest from a GitHub release")
    args = parser.parse_args()

    manifest_name = "firmware.json" if args.published else "firmware-smk37.json"
    manifest = json.loads((args.artifacts / manifest_name).read_text(encoding="utf-8"))
    if manifest.get("license") != "GPL-3.0-only":
        parser.error("O catálogo deve indicar GPL-3.0-only.")
    package_pattern = re.compile(r"zivisynthi-felucca-smk37-(\d+\.\d+\.\d+)\.fwsc")
    packages = [p for p in args.artifacts.glob("*.fwsc") if package_pattern.fullmatch(p.name)]
    if len(packages) != 1:
        parser.error("Esperado um pacote de firmware verificado para SMK-37 Pro.")
    package = packages[0].name
    version = package_pattern.fullmatch(package)[1]
    release_files = (package, EDITOR, EDITOR_PT, "firmware.json", PRESETS, SOURCE)
    trusted_packages = json.loads((ROOT / "platform/web/firmware-packages.json").read_text(encoding="utf-8"))
    if checksum(packages[0]) != trusted_packages.get(version, {}).get("sha256"):
        parser.error("Firmware SHA-256 does not match the recognized package")
    downloads_marker = "<!-- PAGES_DISTRIBUTION -->"
    links_marker = "<!-- PAGES_PROJECT_LINKS -->"

    # Publish the exact application snapshot and the patched base used by the build.
    if not args.published:
        subprocess.run(["git", "diff", "--exit-code", "HEAD", "--"], cwd=ROOT, check=True)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    args.output.mkdir(parents=True, exist_ok=True)
    if args.published:
        copy_release(args.artifacts, args.output, release_files)
    else:
        source = args.output / SOURCE
        subprocess.run(["git", "archive", "--format=zip", "--prefix=ziviSynthi-aks/",
                        "--output", str(source.resolve()), "HEAD"], cwd=ROOT, check=True)
        with ZipFile(source, "a") as archive:
            archive.write(args.artifacts / BASE, BASE)

    project_url = escape(args.source, quote=True)
    builder = [sys.executable, ROOT / "tools/build_web.py"]
    english_studio = args.output / "studio/index.html"
    english_studio.parent.mkdir(exist_ok=True)
    portuguese_studio = args.output / "pt/studio/index.html"
    portuguese_studio.parent.mkdir(parents=True, exist_ok=True)
    editors = [
        (english_studio, "en", "../pt/studio/"),
        (portuguese_studio, "pt", "../../studio/"),
    ]
    if not args.published:
        editors.extend([(args.output / EDITOR, "en", EDITOR_PT),
                        (args.output / EDITOR_PT, "pt", EDITOR)])
    for path, locale, alternate in editors:
        subprocess.run([*builder, path, "--language", locale, "--alternate", alternate,
                        "--embed-firmware", version, packages[0]], check=True)
    for path, back, home, english in (
        (english_studio, "../", "../", True),
        (portuguese_studio, "../../", "../", False),
    ):
        source_html = path.read_text(encoding="utf-8")
        if source_html.count(downloads_marker) != 1 or source_html.count(links_marker) != 1:
            parser.error("O editor gerado deve conter os marcadores de distribuição e navegação.")
        labels = ("Firmware (.fwsc)", "Presets", "Corresponding source", "Notices") if english else (
            "Firmware (.fwsc)", "Presets", "Código-fonte correspondente", "Avisos")
        downloads = '<div class="firmware-links" aria-label="Downloads">' + "\n".join(
            f'<a href="{back}{name}" download>{label}</a>'
            for name, label in zip((package, PRESETS, SOURCE), labels[:3], strict=True)) + (
            f'<a href="{back}SHA256SUMS.txt">SHA-256</a>'
            f'<a href="{back}{"docs/" if english else "pt/docs/"}NOTICES.html">{labels[3]}</a></div>')
        links = (f'<a href="{home}">{"Home" if english else "Início"}</a>'
                 f'<a href="{back}{"docs/" if english else "pt/docs/"}">{"Docs" if english else "Documentação"}</a>'
                 f'<a href="{project_url}">GitHub</a>')
        path.write_text(source_html.replace(downloads_marker, downloads).replace(links_marker, links), encoding="utf-8")

    if not args.published:
        for name in (package, PRESETS):
            shutil.copyfile(args.artifacts / name, args.output / name)
    shutil.copyfile(ROOT / "LICENSE", args.output / "LICENSE")
    shutil.copyfile(ROOT / "NOTICES.md", args.output / "NOTICES.md")
    screenshot = args.output / manifest["screenshot"]
    screenshot.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / manifest["screenshot"], screenshot)
    shutil.copytree(ROOT / "docs/img", args.output / "docs/img", dirs_exist_ok=True)
    manifest.update(site=args.site + "/", install=args.site + "/studio/#firmware-drawer", source=args.source)
    if not args.published:
        (args.output / "firmware.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    render_site(ROOT, args.output, args.source, commit, manifest, version, (EDITOR, package, PRESETS, SOURCE), EDITOR_PT)
    if not args.published:
        (args.output / "SHA256SUMS.txt").write_text(
            "".join(f"{checksum(args.output / name)}  {name}\n" for name in release_files), encoding="utf-8")
    with (args.output / "PAGES-SHA256SUMS.txt").open("w", encoding="utf-8") as checksums:
        for path in sorted(args.output.rglob("*")):
            if path.is_file() and path.name != "PAGES-SHA256SUMS.txt":
                checksums.write(f"{checksum(path)}  {path.relative_to(args.output).as_posix()}\n")


if __name__ == "__main__":
    main()
