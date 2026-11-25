"""Translate the Studio's labels at build time, leaving patch data intact."""
import re


def read_strings(path):
    return dict(line.split("\t", 1) for line in path.read_text(encoding="utf-8").splitlines() if line)


def localize_html(html, strings):
    def label(match):
        source = match[1]
        return source.replace(source.strip(), strings.get(source.strip(), source.strip()), 1)

    def attribute(match):
        return f'{match[1]}="{strings.get(match[2], match[2])}"'

    html = html.replace('lang="pt-BR"', 'lang="en"', 1)
    html = html.replace('Experimentar / <span lang="en">Try</span>', 'Try Web Studio')
    html = re.sub(r'^\s*<p>Pré-lançamento independente[^\n]*\n', '\n', html, flags=re.M)
    html = re.sub(r'(?<=>)([^<>]+)(?=<)', label, html)
    return re.sub(r'\b(aria-label|title|placeholder|label)="([^"]*)"', attribute, html)


def localize_js(script, strings):
    # Match complete source string literals, so IDs, presets and message keys stay intact.
    def replacement(match):
        source = match[1]
        name = source.strip()
        return '"' + source.replace(name, strings.get(name, name), 1) + '"'

    return re.sub(r'"((?:\\.|[^"\\\n])*)"', replacement, script)
