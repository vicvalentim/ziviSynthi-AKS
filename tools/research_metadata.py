"""Format citations for the archived release described in CodeMeta."""
import json


def read_research_metadata(root):
    return json.loads((root / "codemeta.json").read_text(encoding="utf-8"))


def format_citations(metadata):
    author, = metadata["author"]
    family, given = author["familyName"], author["givenName"]
    initials = " ".join(f"{part[0]}." for part in given.split())
    title = metadata["name"]
    version = metadata["version"]
    year = metadata["datePublished"][:4]
    doi_url = metadata["identifier"]
    doi = doi_url.removeprefix("https://doi.org/")
    publisher = metadata["publisher"]["name"]
    apa = (f"{family}, {initials} ({year}). {title} "
           f"(Version {version}) [Computer software]. {publisher}. {doi_url}")
    abnt = (f"{family.upper()}, {given}. {title}. Versão {version}. "
            f"{publisher}, {year}. Software. DOI: {doi}.")
    bibtex = (
        f"@software{{soaresvalentim_zivisynthi_aks_{version.replace('.', '_')},\n"
        f"  author = {{{family}, {given}}},\n"
        f"  title = {{{{{title}}}}},\n"
        f"  version = {{{version}}},\n"
        f"  year = {{{year}}},\n"
        f"  publisher = {{{publisher}}},\n"
        f"  doi = {{{doi}}},\n"
        f"  url = {{{doi_url}}}\n"
        "}\n"
    )
    return {"apa": apa, "abnt": abnt, "bibtex": bibtex}
