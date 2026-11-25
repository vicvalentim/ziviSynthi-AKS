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
    if home and english:
        alternate = '<a lang="pt-BR" hreflang="pt-BR" href="../">Português</a>'
    elif home:
        alternate = '<a lang="en" hreflang="en" href="en/">English</a>'
    else:
        alternate = ""
    docs = "docs/#english" if english else "docs/"
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
    <a class="brand" href="{relative_link(page, 'en/' if english else './')}">ziviSynthi AKS</a>
    <a href="{relative_link(page, docs)}">{'Documentation' if english else 'Documentação'}</a>
    <a href="{relative_link(page, 'studio/#try')}">Web Studio</a>
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
        status = "pre-release"
        lead = "A pin-matrix synthesizer for the M‑VAVE SMK‑37 Pro, inspired by the EMS Synthi AKS."
        notice = "Play through the browser with your computer keyboard or touch. Flashing requires a SMK‑37 Pro connected over USB."
        try_label, install_label, docs_label = "Try in your browser", "Install firmware", "Read the docs"
        caption = "16 × 16 pin matrix on the simulated SMK‑37 Pro display"
        download_labels = "Offline editor (HTML)", "Firmware (FWSC)", "Presets (JSON)", "Corresponding source"
        release = "Release notes"
    else:
        status = "pré-lançamento"
        lead = "Sintetizador de matriz de pinos para o M‑VAVE SMK‑37 Pro, inspirado no EMS Synthi AKS."
        notice = "Toque no navegador com o teclado do computador ou por toque. Para gravar o firmware, conecte o SMK‑37 Pro por USB."
        try_label, install_label, docs_label = "Experimentar no navegador", "Instalar firmware", "Ler a documentação"
        caption = "Matriz 16 × 16 no LCD simulado do SMK‑37 Pro"
        download_labels = "Editor offline (HTML)", "Firmware (FWSC)", "Presets (JSON)", "Código-fonte correspondente"
        release = "Notas da versão"
    downloads = "\n".join(f'    <li><a href="{relative_link(page, path)}" download>{label}</a></li>'
                          for path, label in zip(files, download_labels, strict=True))
    release_path = f"docs/releases/v{version}.html"
    if english:
        release_path += "#english"
    # Keep the original installer fragment working after moving the Studio off the home page.
    redirect = """<script>
if (location.hash === "#firmware-drawer") location.replace("studio/#firmware-drawer");
</script>
""" if not english else ""
    main = f"""{redirect}<main>
  <section class="hero">
    <div>
      <p class="version">SMK‑37 Pro · {escape(version)} {status}</p>
      <h1>ziviSynthi AKS</h1>
      <p class="lead">{lead}</p>
      <div class="actions">
        <a class="primary" href="{relative_link(page, 'studio/#try')}">{try_label}</a>
        <a href="{relative_link(page, 'studio/#firmware-drawer')}">{install_label}</a>
        <a href="{relative_link(page, 'docs/#english' if english else 'docs/')}">{docs_label}</a>
      </div>
      <p class="notice">{notice}</p>
    </div>
    <figure>
      <img src="{relative_link(page, manifest['screenshot'])}" alt="{caption}">
      <figcaption>{caption}</figcaption>
    </figure>
  </section>
  <section class="downloads" aria-labelledby="downloads">
    <h2 id="downloads">Downloads</h2>
    <ul>
{downloads}
      <li><a href="{relative_link(page, 'SHA256SUMS.txt')}">SHA-256</a></li>
      <li><a href="{relative_link(page, release_path)}">{release}</a></li>
    </ul>
  </section>
</main>
"""
    return layout(page, "ziviSynthi AKS", main, source, english, home=True)


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
                  source, original.name.endswith(".en.md"))


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
            if not uri.path and page == Path("studio/index.html") and uri.fragment == "bank-9":
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
    home = Path("index.html")
    english_home = Path("en/index.html")
    (output / english_home.parent).mkdir(exist_ok=True)
    for page, english in ((home, False), (english_home, True)):
        (output / page).write_text(home_page(page, source, manifest, version, files, english), encoding="utf-8")
    verify_links(output, [home, english_home, Path("studio/index.html"), *pages.values()])
