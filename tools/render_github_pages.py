"""Render the home and the repository's public Markdown documentation."""
from html import escape, unescape
from html.parser import HTMLParser
import os
from pathlib import Path
import re
import shutil
import subprocess
from urllib.parse import quote, unquote, urlsplit, urlunsplit


BILINGUAL_SECTIONS = {
    "ENTRADA_AUDIO_USB.md": ("entrada-de-áudio-usb--usb-audio-input", "english"),
    "KS_SEQUENCER.md": ("motor-ks--ks-engine", "english"),
    "MATRIX_REFERENCE.md": ("matriz-do-zivisynthi-aks--pin-matrix", "english"),
    "PRESETS_HANDBOOK.md": ("presets-do-zivisynthi-aks--handbook-presets", "english"),
    "NOTICES.md": ("português", "english"),
}

def localized_article(original, page):
    return Path("pt") / page if original.name in BILINGUAL_SECTIONS or original.parent == Path("docs/releases") else page


def relative_link(page, target):
    path, separator, fragment = target.partition("#")
    relative = os.path.relpath(path, page.parent)
    if path.endswith("/"):
        relative = "./" if relative == "." else relative + "/"
    return escape(relative + separator + fragment, quote=True)


def layout(page, title, content, source, english=False, home=False, translations=None):
    language = "en" if english else "pt-BR"
    defaults = ("./", "pt/") if home else ("docs/", "pt/docs/")
    en_page, pt_page = translations or defaults
    alternate = "".join(
        f'<a lang="{code}" hreflang="{code}" data-language="{code}" '
        f'href="{relative_link(page, target)}"{current}>{label}</a>'
        for code, target, label in (("en", en_page, "EN"), ("pt-BR", pt_page, "PT"))
        for current in (' aria-current="true"' if code == language else "",)
    )
    docs = "docs/" if english else "pt/docs/"
    gallery = (f'<a href="#firmware-screens">{"Screens" if english else "Telas"}</a>'
               f'<a href="#downloads">Downloads</a>') if home else ""
    restore = f' data-restore-language="{relative_link(page, pt_page)}"' if page == Path("index.html") else ""
    return f"""<!doctype html>
<html lang="{language}"{restore}>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{escape(title)}</title>
  <link rel="alternate" hreflang="en" href="{relative_link(page, en_page)}">
  <link rel="alternate" hreflang="pt-BR" href="{relative_link(page, pt_page)}">
  <script src="{relative_link(page, 'site.js')}"></script>
  <link rel="stylesheet" href="{relative_link(page, 'site.css')}">
</head>
<body>
<a class="skip-link" href="#main-content">{'Skip to content' if english else 'Ir para o conteúdo'}</a>
<header class="site-header">
  <nav aria-label="{'Site navigation' if english else 'Navegação do site'}">
    <a class="brand"{"" if home else ' data-preview-route="home"'} href="{relative_link(page, './' if english else 'pt/')}">ziviSynthi AKS</a>
    <div class="site-links">
    {gallery}
    <a href="{relative_link(page, docs)}"{' data-preview-route="docs"'}>{'Documentation' if english else 'Documentação'}</a>
    <a href="{relative_link(page, 'studio/#try' if english else 'pt/studio/#try')}">Web Studio</a>
    <a href="{escape(source, quote=True)}">GitHub</a>
    </div>
    <div class="preferences">
      <div class="language" aria-label="{'Language' if english else 'Idioma'}">{alternate}</div>
      <div class="theme" data-enhanced role="group" aria-label="{'Color theme' if english else 'Tema de cores'}">
        <button type="button" data-set-theme="system" aria-pressed="false">Auto</button>
        <button type="button" data-set-theme="light" aria-pressed="false">{'Light' if english else 'Claro'}</button>
        <button type="button" data-set-theme="dark" aria-pressed="false">{'Dark' if english else 'Escuro'}</button>
      </div>
    </div>
  </nav>
</header>
{content}
<footer>Victor Valentim · <a href="{relative_link(page, 'docs/NOTICES.html#english' if english else 'pt/docs/NOTICES.html#português')}">{'Notices and credits' if english else 'Avisos e créditos'}</a> · <a href="{relative_link(page, 'LICENSE')}">GPL-3.0-only</a></footer>
</body>
</html>
"""


def home_page(page, source, manifest, version, files, english, illustration):
    if english:
        copy = {
            "edition": "Version", "title": "A pin-matrix synthesizer for the M‑VAVE SMK‑37 Pro",
            "lead": "Patch, sequence and play on the controller or directly in your browser.",
            "try": "Play in Web Studio", "install": "Install firmware", "guide": "User guide",
            "display": "SMK‑37 Pro display", "views": "Firmware screens",
            "origin": "Interface rendered from firmware drawing commands",
            "gallery_title": "The SMK‑37 Pro interface",
            "gallery_intro": "Explore the matrix, module controls, preset bank and sequencer on the controller.",
            "studio_modes": ("Instant Patchers", "Edit KS phrases"),
            "animation": ("Play animation", "Pause animation", "Resume animation"),
            "gallery_nav": ("Previous screen", "Next screen", "Enlarge image", "Close image", "Replay startup"),
            "boot": "Startup animation, version 3.9.5 · 5 seconds",
            "screens": (("Matrix", "Overview with a 16 × 16 pin matrix"),
                        ("Oscillator", "Oscillator 1 controls and waveform"),
                        ("Presets", "Instrument preset bank"),
                        ("Sequencer", "KS sequencer controls and steps"),
                        ("Startup", "Animated startup screen from version 3.9.5")),
            "instrument": "A patch starts with a pin",
            "instrument_body": "The EMS Synthi AKS used a pin matrix to connect its analog modules. ziviSynthi AKS brings that way of patching to a digital instrument. Connect oscillators, noise, the resonant filter, ring modulation, the trapezoid envelope and spring-style reverb through the 16 × 16 matrix.",
            "play": "Play on the Pro or in the browser",
            "play_body": "On the SMK‑37 Pro, use the knobs, pads and wheels to shape sounds and play the KS-inspired sequencer. Explore 128 presets, build a patch in Web Studio, or connect the Pro by USB MIDI for live editing.",
            "learn_history": "History and emulation", "collection": "Visual reference: MESS collection", "illustration": "Illustration of the EMS Synthi AKS with its silver panel and blue KS keyboard",
            "release": "Downloads",
            "release_body": "Get the SMK‑37 Pro firmware, the standalone editor, the preset bank or the corresponding GPL source.",
            "notes": "Release notes", "releases": "Release on GitHub", "compatibility": "Firmware targets",
            "downloads": ("Offline Web Studio", "SMK‑37 Pro firmware", "Preset bank", "Corresponding source"),
        }
    else:
        copy = {
            "edition": "Versão", "title": "Sintetizador de matriz para o M‑VAVE SMK‑37 Pro",
            "lead": "Crie patches, programe sequências e toque no controlador ou diretamente no navegador.",
            "try": "Tocar no Web Studio", "install": "Instalar firmware", "guide": "Guia de uso",
            "display": "Tela do SMK‑37 Pro", "views": "Telas do firmware",
            "origin": "Interface renderizada a partir dos comandos de desenho do firmware",
            "gallery_title": "A interface do SMK‑37 Pro",
            "gallery_intro": "Explore a matriz, os controles dos módulos, o banco de presets e o sequenciador no controlador.",
            "studio_modes": ("Instant Patchers", "Editar frases KS"),
            "animation": ("Reproduzir animação", "Pausar animação", "Retomar animação"),
            "gallery_nav": ("Tela anterior", "Próxima tela", "Ampliar imagem", "Fechar imagem", "Repetir abertura"),
            "boot": "Animação de abertura da versão 3.9.5 · 5 segundos",
            "screens": (("Matriz", "Visão geral com a matriz de 16 × 16 pinos"),
                        ("Oscilador", "Forma de onda e controles do oscilador 1"),
                        ("Presets", "Banco de presets do instrumento"),
                        ("Sequenciador", "Controles e passos do sequenciador KS"),
                        ("Abertura", "Animação de abertura da versão 3.9.5")),
            "instrument": "Um patch começa com um pino",
            "instrument_body": "O EMS Synthi AKS conectava seus módulos analógicos por uma matriz de pinos. O ziviSynthi AKS leva esse modo de criar patches a um instrumento digital. Ligue osciladores, ruído, filtro ressonante, modulação em anel, envoltória trapezoidal e reverberação inspirada em mola pela matriz 16 × 16.",
            "play": "Toque no Pro ou no navegador",
            "play_body": "No SMK‑37 Pro, use os knobs, pads e rodas para modelar sons e tocar o sequenciador inspirado no KS. Explore 128 presets, crie patches no Web Studio ou conecte o Pro por USB MIDI para edição ao vivo.",
            "learn_history": "História e emulação", "collection": "Referência visual: coleção MESS", "illustration": "Ilustração do EMS Synthi AKS com painel prateado e teclado KS azul",
            "release": "Downloads",
            "release_body": "Baixe o firmware do SMK‑37 Pro, o editor independente, o banco de presets e o código-fonte correspondente sob GPL.",
            "notes": "Notas da versão", "releases": "Versão no GitHub", "compatibility": "Alvos de firmware",
            "downloads": ("Web Studio offline", "Firmware do SMK‑37 Pro", "Banco de presets", "Código-fonte correspondente"),
        }

    def link(target):
        return relative_link(page, target)

    studio = "studio/" if english else "pt/studio/"
    docs = "docs/" if english else "pt/docs/"
    previous, following, zoom, close, replay = copy["gallery_nav"]
    play_art, pause_art, resume_art = copy["animation"]
    targets_page = "docs/usage/FIRMWARE_TARGETS.en.html" if english else "docs/usage/FIRMWARE_TARGETS.html"
    history_page = "docs/usage/HISTORY.en.html" if english else "docs/usage/HISTORY.html"
    release_path = f"docs/releases/v{version}.html"
    if not english:
        release_path = f"pt/{release_path}#português"
    screen_paths = (manifest["screenshot"], "docs/img/smk37-pro-oscillator.png",
                    "docs/img/smk37-pro-presets.png", "docs/img/smk37-pro-sequencer.png")
    panels = []
    hero_illustration = illustration.replace('viewBox="0 0 1024 1024"',
                                              'viewBox="135 65 754 878" aria-hidden="true" focusable="false"', 1)
    controls = []
    for index, (label, description) in enumerate(copy["screens"]):
        selected = index == 0
        panel_id = f"screen-{index}"
        tab_id = f"screen-tab-{index}"
        if index < len(screen_paths):
            vector_path = str(Path(screen_paths[index]).with_suffix(".svg"))
            media = f'<img src="{link(vector_path)}" alt="{escape(description)}" width="1024" height="1024" loading="lazy">'
        else:
            media = (f'<video controls muted playsinline preload="none" width="1024" height="1024" '
                     f'aria-label="{escape(description)}" poster="{link("docs/img/smk37-pro-boot-395.png")}">'
                     f'<source src="{link("docs/img/smk37-pro-boot-395.mp4")}" type="video/mp4"></video>')
        hidden = "" if selected else " hidden"
        panels.append(f'<div class="screen-view" id="{panel_id}" role="tabpanel" aria-labelledby="{tab_id}"'
                      f'{hidden}>{media}</div>')
        controls.append(f'<button type="button" id="{tab_id}" role="tab" data-screen="{panel_id}" '
                        f'aria-controls="{panel_id}" aria-selected="{str(selected).lower()}" '
                        f'tabindex="{0 if selected else -1}">{label}</button>')
    downloads = "\n".join(f'<li><a href="{link(path)}" download>{label}<span aria-hidden="true">↓</span></a></li>'
                          for path, label in zip(files, copy["downloads"], strict=True))
    redirect = """<script>
if (location.hash === "#firmware-drawer") location.replace("studio/#firmware-drawer");
</script>
""" if page == Path("index.html") else ""
    main = f"""{redirect}<main id="main-content">
  <section class="hero" aria-labelledby="hero-title">
    <div class="hero-copy">
      <a class="version" href="{link(release_path)}">{copy['edition']} {escape(version)}</a>
      <h1 id="hero-title">ziviSynthi <span>AKS</span></h1>
      <p class="tagline">{copy['title']}</p>
      <p class="lead">{copy['lead']}</p>
      <div class="actions">
        <a class="primary" href="{link(studio + '#try')}">{copy['try']} <span aria-hidden="true">↗</span></a>
        <a href="{link(studio + '#firmware-drawer')}">{copy['install']}</a>
      </div>
    </div>
    <figure class="hero-art" data-artwork>
      <div class="hero-art-plate" role="img" aria-label="{escape(copy['illustration'], quote=True)}">{hero_illustration}</div>
      <figcaption class="hero-art-caption">
        <span>EMS Synthi AKS · <a href="https://mess.foundation/product/synthi-aks/">{copy['collection']}</a></span>
        <button type="button" data-art-control data-enhanced data-play="{play_art}" data-pause="{pause_art}"
          data-resume="{resume_art}">{play_art}</button>
      </figcaption>
    </figure>
  </section>
  <section class="firmware-showcase" id="firmware-screens" aria-labelledby="firmware-title" data-motion>
    <div>
      <h2 id="firmware-title">{copy['gallery_title']}</h2>
      <p>{copy['gallery_intro']}</p>
    </div>
    <figure class="console" data-gallery>
      <div class="console-bar"><span aria-hidden="true" class="console-light"></span>{copy['display']}</div>
      <div class="screen">{''.join(panels)}</div>
      <div class="screen-controls" data-enhanced role="tablist" aria-label="{copy['views']}">{''.join(controls)}</div>
      <figcaption>
        <div class="screen-meta">
          <span>{copy['origin']}</span>
          <span class="screen-nav" data-enhanced>
            <button type="button" data-cycle="-1" aria-label="{previous}">←</button>
            <button type="button" data-zoom>{zoom}</button>
            <button type="button" data-replay hidden>{replay}</button>
            <button type="button" data-cycle="1" aria-label="{following}">→</button>
          </span>
        </div>
        <span data-screen-caption="screen-4" hidden>{copy['boot']}</span>
        <span class="visually-hidden" data-screen-status aria-live="polite"></span>
      </figcaption>
      <dialog class="screen-dialog" aria-label="{zoom}">
        <div><img alt="" width="1024" height="1024"></div>
        <form method="dialog"><button autofocus>{close}</button></form>
      </dialog>
    </figure>
  </section>
  <div class="details" data-motion>
    <section class="instrument-story" aria-labelledby="instrument">
      <div>
        <h2 id="instrument">{copy['instrument']}</h2>
        <p>{copy['instrument_body']}</p>
        <a href="{link(history_page)}">{copy['learn_history']} ↗</a>
      </div>
    </section>
    <section aria-labelledby="play">
      <h2 id="play">{copy['play']}</h2>
      <p>{copy['play_body']}</p>
      <div class="release-links">
        <a href="{link(studio + '#instant-patchers')}">{copy['studio_modes'][0]} ↗</a>
        <a href="{link(studio + '#ks-editor')}">{copy['studio_modes'][1]} ↗</a>
        <a href="{link(docs)}" data-preview-route="docs">{copy['guide']} ↗</a>
      </div>
    </section>
  </div>
  <section class="downloads" aria-labelledby="downloads" data-motion>
    <div>
      <h2 id="downloads">{copy['release']} <span>{escape(version)}</span></h2>
      <p>{copy['release_body']}</p>
      <div class="release-links">
        <a href="{link(release_path)}">{copy['notes']}</a>
        <a href="{link(targets_page)}">{copy['compatibility']}</a>
        <a href="{escape(source, quote=True)}/releases/tag/v{escape(version)}">{copy['releases']} ↗</a>
      </div>
    </div>
    <ul>
      {downloads}
      <li><a href="{link('SHA256SUMS.txt')}">SHA-256<span aria-hidden="true">↗</span></a></li>
    </ul>
  </section>
</main>
"""
    return layout(page, "ziviSynthi AKS · " + copy["title"], main, source, english, home=True)


def render_markdown(root, source, commit, pages, original, page, english, translations):
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
            target = pages[target] if english else localized_article(target, pages[target])
        elif target != Path("LICENSE") and not (target.parts[:2] == ("docs", "img")):
            if target.is_absolute() or ".." in target.parts or not (root / target).is_file():
                raise ValueError(f"{original}: invalid documentation link {value}")
            remote = f"{source}/blob/{commit}/{quote(target.as_posix())}"
            return f'{attribute}="{escape(remote + (f"#{url.fragment}" if url.fragment else ""), quote=True)}"'
        relative_path = Path(os.path.relpath(target, page.parent)).as_posix()
        uri = urlunsplit(("", "", quote(relative_path), url.query, url.fragment))
        return f'{attribute}="{escape(uri, quote=True)}"'

    content = re.sub(r'\b(href|src)="([^"]*)"', rewrite, result.stdout)
    if original == Path("docs/README.md"):
        content = '<span id="english"></span><span id="português"></span>' + content
    content = content.replace("<table>", '<div class="table-scroll"><table>').replace("</table>", "</table></div>")
    return layout(page, title + " · ziviSynthi AKS", f'<main id="main-content" class="docs">{content}</main>',
                  source, english, translations=translations)


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
        for name in ("href", "src", "poster"):
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


def render_site(root, output, source, commit, manifest, version, files, editor_pt):
    for asset in ("site.css", "site.js"):
        shutil.copyfile(root / "platform/pages" / asset, output / asset)
    pages = {path.relative_to(root): path.relative_to(root).with_suffix(".html")
             for path in (root / "docs/usage").glob("*.md")}
    pages.update({Path("docs/README.md"): Path("docs/index.html"),
                  Path("docs/README.pt.md"): Path("pt/docs/index.html"),
                  Path(f"docs/releases/v{version}.md"): Path(f"docs/releases/v{version}.html"),
                  Path("NOTICES.md"): Path("docs/NOTICES.html")})
    guide_pair = ("docs/", "pt/docs/")
    index_pair = {Path("docs/README.md"): (True, guide_pair), Path("docs/README.pt.md"): (False, guide_pair)}
    local_articles = []
    for original, page in pages.items():
        path = output / page
        path.parent.mkdir(parents=True, exist_ok=True)
        english = original.name.endswith(".en.md")
        translations = guide_pair
        if original in index_pair:
            english, translations = index_pair[original]
        elif original.parent == Path("docs/releases") or original.name in BILINGUAL_SECTIONS:
            other = localized_article(original, page)
            pt, en = BILINGUAL_SECTIONS.get(original.name, ("português", ""))
            translations = (f"{page}#{en}" if en else str(page), f"{other}#{pt}")
            english = True
            local_articles.append(other)
            counterpart = output / other
            counterpart.parent.mkdir(parents=True, exist_ok=True)
            counterpart.write_text(render_markdown(root, source, commit, pages, original, other, False,
                                                   translations), encoding="utf-8")
        elif original.parent == Path("docs/usage"):
            base = original.name.replace(".en.md", ".md")
            pt = original.with_name(base)
            en = pt.with_suffix(".en.md")
            if pt in pages and en in pages:
                translations = (str(pages[en]), str(pages[pt]))
        path.write_text(render_markdown(root, source, commit, pages, original, page, english, translations),
                        encoding="utf-8")
    illustration = (root / "docs/img/ems-synthi-aks-illustration.svg").read_text(encoding="utf-8")
    home, portuguese, legacy = Path("index.html"), Path("pt/index.html"), Path("en/index.html")
    for page, english in ((home, True), (portuguese, False), (legacy, True)):
        (output / page.parent).mkdir(parents=True, exist_ok=True)
        downloads = files if english else (editor_pt, *files[1:])
        (output / page).write_text(home_page(page, source, manifest, version, downloads, english, illustration),
                                   encoding="utf-8")
    verify_links(output, [home, portuguese, legacy, Path("studio/index.html"),
                          Path("pt/studio/index.html"), *pages.values(), *local_articles])
