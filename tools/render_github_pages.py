"""Render the home and the repository's public Markdown documentation."""
from html import escape, unescape
from html.parser import HTMLParser
import os
from pathlib import Path
import re
import shutil
import subprocess
from urllib.parse import quote, unquote, urlsplit, urlunsplit


def relative_link(page, target):
    path, separator, fragment = target.partition("#")
    relative = os.path.relpath(path, page.parent)
    if path.endswith("/"):
        relative = "./" if relative == "." else relative + "/"
    return escape(relative + separator + fragment, quote=True)


def layout(page, title, content, source, english=False, home=False):
    language = "en" if english else "pt-BR"
    en_page = "./" if home else "docs/#english"
    pt_page = "pt/" if home else "docs/#português"
    en_current = ' aria-current="page"' if english else ""
    pt_current = ' aria-current="page"' if not english else ""
    alternate = (f'<div class="language" aria-label="{ "Language" if english else "Idioma" }">'
                 f'<a lang="en" hreflang="en" href="{relative_link(page, en_page)}"{en_current}>EN</a>'
                 f'<a lang="pt-BR" hreflang="pt-BR" href="{relative_link(page, pt_page)}"{pt_current}>PT</a></div>')
    docs = "docs/#english" if english else "docs/#português"
    return f"""<!doctype html>
<html lang="{language}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#1e2223">
  <title>{escape(title)}</title>
  <link rel="stylesheet" href="{relative_link(page, 'site.css')}">
</head>
<body>
<header>
  <nav aria-label="{'Site navigation' if english else 'Navegação do site'}">
    <a class="brand" href="{relative_link(page, './' if english else 'pt/')}">ziviSynthi AKS</a>
    <a href="{relative_link(page, docs)}">{'Documentation' if english else 'Documentação'}</a>
    <a href="{relative_link(page, 'studio/#try' if english else 'pt/studio/#try')}">Web Studio</a>
    <a href="{escape(source, quote=True)}">GitHub</a>
    {alternate}
  </nav>
</header>
{content}
<footer>Victor Valentim · <a href="{relative_link(page, 'docs/NOTICES.html' + ('#english' if english else '#português'))}">{'Notices and credits' if english else 'Avisos e créditos'}</a> · <a href="{relative_link(page, 'LICENSE')}">GPL-3.0-only</a></footer>
</body>
</html>
"""


def home_page(page, source, manifest, version, files, english):
    if english:
        lang = {
            "status": "Pre-release", "tagline": "An EMS-inspired matrix synthesizer for the M‑VAVE SMK‑37 Pro.",
            "try": "Play in Web Studio", "install": "Install on SMK‑37 Pro", "docs": "User guide",
            "instrument": "The EMS Synthi AKS", "instrument_body": "The 1972 EMS Synthi AKS put a pin matrix, joystick and analog synthesis modules in a portable case. The lid housed its touch keyboard and KS sequencer. A pin sets the path a signal takes, including paths that turn audio into control.",
            "model": "The digital instrument", "model_body": "ziviSynthi AKS recreates that patching workflow with its own DSP. Three oscillators, a resonant filter, ring modulation, envelope shaping and spring-style reverb share a 16 × 16 matrix. The KS-inspired sequencer records and plays back gestures and notes. The models approximate the original circuits and leave room for new patches.",
            "tools": "Two ways to play", "web": "Web Studio", "web_body": "Play with a computer keyboard or touch, patch the matrix, explore the presets and export patches. Connect a supported Pro by USB MIDI when you want to edit the hardware from the browser.",
            "hardware": "M‑VAVE SMK‑37 Pro", "hardware_body": "On the Pro, the knobs, pads, wheels and LCD become the instrument panel. The firmware also includes 128 instrument presets, four USB effects, and an offline HTML editor is available.",
            "release": "Download the pre-release", "validation": "The image and updater have been verified in host tests. Flashing, analog output and Bluetooth still need testing on a physical SMK‑37 Pro.",
            "notes": "Release notes", "releases": "GitHub Releases", "screen": "Matrix screen on the simulated SMK‑37 Pro",
            "historical": "Original EMS Synthi AKS with its keyboard and sequencer",
            "downloads": ("Offline editor", "SMK‑37 Pro firmware", "Presets", "Corresponding source"),
        }
    else:
        lang = {
            "status": "Pré-release", "tagline": "Sintetizador de matriz inspirado no EMS para o M‑VAVE SMK‑37 Pro.",
            "try": "Tocar no Web Studio", "install": "Instalar no SMK‑37 Pro", "docs": "Guia de uso",
            "instrument": "O EMS Synthi AKS", "instrument_body": "Em 1972, o EMS Synthi AKS reuniu matriz de pinos, joystick e módulos de síntese analógica em uma maleta. A tampa abrigava o teclado sensível ao toque e o sequenciador KS. Cada pino define um caminho para o sinal, inclusive rotas que usam o áudio como controle.",
            "model": "O instrumento digital", "model_body": "O ziviSynthi AKS recria esse modo de fazer patches com DSP próprio. Três osciladores, filtro ressonante, modulação em anel, envoltória e reverberação inspirada em mola compartilham uma matriz 16 × 16. O sequenciador inspirado no KS grava e reproduz gestos e notas. Os modelos aproximam o comportamento dos circuitos originais e permitem criar novos patches.",
            "tools": "Duas formas de tocar", "web": "Web Studio", "web_body": "Toque com o teclado do computador ou por toque, modifique a matriz, explore os timbres e exporte patches. Conecte um Pro compatível por USB MIDI para editar o instrumento pelo navegador.",
            "hardware": "M‑VAVE SMK‑37 Pro", "hardware_body": "No Pro, os knobs, pads, rodas e LCD formam o painel do instrumento. O firmware inclui 128 timbres e quatro efeitos USB. O editor também está disponível como HTML offline.",
            "release": "Baixar a pré-release", "validation": "A imagem e o atualizador passaram nos testes em host. A instalação, a saída analógica e o Bluetooth ainda precisam ser testados em um SMK‑37 Pro físico.",
            "notes": "Notas da versão", "releases": "Releases no GitHub", "screen": "Tela da matriz no simulador do SMK‑37 Pro",
            "historical": "EMS Synthi AKS original com teclado e sequenciador",
            "downloads": ("Editor offline", "Firmware do SMK‑37 Pro", "Presets", "Código-fonte correspondente"),
        }
    studio = "studio/" if english else "pt/studio/"
    docs = "docs/#english" if english else "docs/#português"
    release_path = f"docs/releases/v{version}.html" + ("" if english else "#português")
    def link(target):
        return relative_link(page, target)
    downloads = "\n".join(f'    <li><a href="{link(path)}" download>{label}</a></li>'
                          for path, label in zip(files, lang["downloads"], strict=True))
    redirect = """<script>
if (location.hash === "#firmware-drawer") location.replace("studio/#firmware-drawer");
</script>
""" if page == Path("index.html") else ""
    main = f"""{redirect}<main>
  <section class="hero" aria-labelledby="hero-title">
    <div class="hero-copy">
      <p class="version"><span></span>{lang['status']} {escape(version)} · M‑VAVE SMK‑37 Pro</p>
      <h1 id="hero-title">ziviSynthi <em>AKS</em></h1>
      <p class="lead">{lang['tagline']}</p>
      <div class="actions">
        <a class="primary" href="{link(studio + '#try')}">{lang['try']} <span aria-hidden="true">↗</span></a>
        <a href="{link(studio + '#firmware-drawer')}">{lang['install']}</a>
      </div>
    </div>
    <figure class="hero-instrument">
      <img src="{link('docs/img/ems-synthi-aks.webp')}" alt="{lang['historical']}" width="624" height="930" fetchpriority="high">
      <figcaption>EMS Synthi AKS · Guy Hatton / <a href="https://commons.wikimedia.org/wiki/File:EMS_Synthi_AKS_(opened).jpg">Wikimedia Commons</a> · <a href="https://creativecommons.org/licenses/by-sa/3.0/">CC BY‑SA 3.0</a></figcaption>
    </figure>
  </section>
  <section class="context" aria-label="{lang['instrument']}">
    <div>
      <h2>{lang['instrument']}</h2>
      <p>{lang['instrument_body']}</p>
      <a class="text-link" href="https://www.synthforbreakfast.nl/wp-content/uploads/2022/03/Synthi-AKS-The_Educational_Handbook.pdf">The Synthi Educational Handbook ↗</a>
    </div>
    <div>
      <h2>{lang['model']}</h2>
      <p>{lang['model_body']}</p>
      <a class="text-link" href="{link(docs)}">{lang['docs']} ↗</a>
    </div>
  </section>
  <section class="experience" aria-labelledby="play-title">
    <div>
      <h2 id="play-title">{lang['tools']}</h2>
      <h3>{lang['web']}</h3>
      <p>{lang['web_body']}</p>
      <h3>{lang['hardware']}</h3>
      <p>{lang['hardware_body']}</p>
    </div>
    <div class="screens">
      <figure class="lcd">
        <img src="{link('docs/img/smk37-pro-splash.png')}" alt="ziviSynthi AKS · SMK‑37 Pro / 3.9.6" loading="lazy" width="240" height="240">
        <figcaption>{'Boot screen · host preview' if english else 'Tela de abertura · prévia no simulador'}</figcaption>
      </figure>
      <figure class="lcd">
        <img src="{link(manifest['screenshot'])}" alt="{lang['screen']}" loading="lazy" width="240" height="240">
        <figcaption>{lang['screen']}</figcaption>
      </figure>
    </div>
  </section>
  <section class="downloads" aria-labelledby="downloads">
    <div>
      <h2 id="downloads">{lang['release']}</h2>
      <p class="notice">{lang['validation']}</p>
      <div class="release-links">
        <a href="{link(release_path)}">{lang['notes']}</a>
        <a href="{escape(source, quote=True)}/releases">{lang['releases']} ↗</a>
      </div>
    </div>
    <ul>
{downloads}
      <li><a href="{link('SHA256SUMS.txt')}">SHA-256</a></li>
    </ul>
  </section>
</main>
"""
    return layout(page, "ziviSynthi AKS · Matrix Synthesizer", main, source, english, home=True)


def render_markdown(root, source, commit, pages, original, page):
    text = (root / original).read_text(encoding="utf-8")
    title = text.splitlines()[0].removeprefix("# ")
    result = subprocess.run(["pandoc", "-f", "gfm", "-t", "html5", "--no-highlight", "--wrap=none"],
                            input=text, text=True, capture_output=True, check=True)

    def rewrite(match):
        attribute, encoded = match.groups()
        value = unescape(encoded)
        url = urlsplit(value)
        if url.scheme or url.netloc or not url.path:
            return match[0]
        target = Path(os.path.normpath(original.parent / unquote(url.path)))
        if target in pages:
            target = pages[target]
        elif target != Path("LICENSE") and not (target.parts[:2] == ("docs", "img")):
            if target.is_absolute() or ".." in target.parts or not (root / target).is_file():
                raise ValueError(f"{original}: invalid documentation link {value}")
            remote = f"{source}/blob/{commit}/{quote(target.as_posix())}"
            return f'{attribute}="{escape(remote + (f"#{url.fragment}" if url.fragment else ""), quote=True)}"'
        relative_path = Path(os.path.relpath(target, page.parent)).as_posix()
        uri = urlunsplit(("", "", quote(relative_path), url.query, url.fragment))
        return f'{attribute}="{escape(uri, quote=True)}"'

    content = re.sub(r'\b(href|src)="([^"]*)"', rewrite, result.stdout)
    content = content.replace("<table>", '<div class="table-scroll"><table>').replace("</table>", "</table></div>")
    return layout(page, title + " · ziviSynthi AKS", f'<main class="docs">{content}</main>',
                  source, original.name.endswith(".en.md") or str(original) in {"docs/README.md", "docs/releases/v3.9.6.md", "CHANGELOG.md"})


class PageReferences(HTMLParser):
    def __init__(self, markup):
        super().__init__()
        self.ids = set()
        self.references = []
        self.feed(markup)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            self.ids.add(attrs["id"])
        for name in ("href", "src"):
            if name in attrs:
                self.references.append(attrs[name])


def verify_links(output, pages):
    references = {path: PageReferences((output / path).read_text(encoding="utf-8")) for path in pages}
    for page, data in references.items():
        for link in data.references:
            uri = urlsplit(link)
            if uri.scheme or uri.netloc:
                continue
            target = Path(os.path.normpath(page.parent / unquote(uri.path))) if uri.path else page
            if (output / target).is_dir():
                target /= "index.html"
            if target.is_absolute() or ".." in target.parts or not (output / target).is_file():
                if target == Path("SHA256SUMS.txt"):
                    continue  # The manifest is written after HTML validation.
                raise ValueError(f"{page}: missing {link}")
            if not uri.path and page in {Path("studio/index.html"), Path("pt/studio/index.html")} and uri.fragment == "bank-9":
                continue  # The editor creates the Output Channels rack from its module catalog.
            if target in references and uri.fragment and unquote(uri.fragment) not in references[target].ids:
                raise ValueError(f"{page}: anchor not found in {link}")


def render_site(root, output, source, commit, manifest, version, files):
    shutil.copyfile(root / "platform/pages/site.css", output / "site.css")
    pages = {path.relative_to(root): path.relative_to(root).with_suffix(".html")
             for path in (root / "docs").rglob("*.md")}
    pages.update({Path("docs/README.md"): Path("docs/index.html"),
                  Path("CHANGELOG.md"): Path("docs/CHANGELOG.html"),
                  Path("NOTICES.md"): Path("docs/NOTICES.html")})
    for original, page in pages.items():
        path = output / page
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(render_markdown(root, source, commit, pages, original, page), encoding="utf-8")
    home, portuguese, legacy = Path("index.html"), Path("pt/index.html"), Path("en/index.html")
    for page, english in ((home, True), (portuguese, False), (legacy, True)):
        (output / page.parent).mkdir(parents=True, exist_ok=True)
        (output / page).write_text(home_page(page, source, manifest, version, files, english), encoding="utf-8")
    verify_links(output, [home, portuguese, legacy, Path("studio/index.html"),
                          Path("pt/studio/index.html"), *pages.values()])
