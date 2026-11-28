"""Check that the citation files describe the same archived software release."""
import json
import re
from pathlib import Path

from ruamel.yaml import YAML

from research_metadata import format_citations, read_research_metadata


ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    cff = YAML(typ="safe").load((ROOT / "CITATION.cff").read_text(encoding="utf-8"))
    zenodo = json.loads((ROOT / ".zenodo.json").read_text(encoding="utf-8"))
    meta = read_research_metadata(ROOT)
    manifest = json.loads((ROOT / "firmware.json").read_text(encoding="utf-8"))
    author, = meta["author"]
    cff_person, = cff["authors"]
    creator, = zenodo["creators"]
    for key, values in {
        "title": [meta["name"], cff["title"], zenodo["title"]],
        "version": [meta["version"], cff["version"], zenodo["version"]],
        "surname": [author["familyName"], cff_person["family-names"], creator["name"].split(", ")[0]],
        "given name": [author["givenName"], cff_person["given-names"], creator["name"].split(", ")[1]],
        "ORCID": [author["@id"], cff_person["orcid"], "https://orcid.org/" + creator["orcid"]],
        "affiliation": [author["affiliation"]["name"], cff_person["affiliation"], creator["affiliation"]],
        "license": [manifest["license"], cff["license"], meta["license"].split("/")[-1].removesuffix(".html")],
        "date": [meta["datePublished"], str(cff["date-released"])],
        "website": [meta["url"], cff["url"]],
        "source": [meta["codeRepository"], cff["repository-code"]],
    }.items():
        require(len(set(values)) == 1, f"Inconsistent scientific metadata: {key}")

    doi = meta["identifier"]
    concept = meta["isPartOf"]["@id"]
    require(doi == meta["@id"] == "https://doi.org/" + cff["doi"], "Version DOI mismatch")
    require(any(i["type"] == "doi" and "https://doi.org/" + i["value"] == concept for i in cff["identifiers"]),
            "Concept DOI missing from CITATION.cff")
    require(doi != concept and all(re.fullmatch(r"https://doi\.org/10\.5281/zenodo\.\d+", i) for i in (doi, concept)),
            "Expected separate Zenodo version and concept DOIs")
    require(meta["sameAs"].rsplit("/", 1)[1] == doi.rsplit(".", 1)[1], "Zenodo record link mismatch")
    require(zenodo["upload_type"] == "software" and zenodo["license"] == cff["license"].lower() and
            zenodo["language"] == "eng" and zenodo["access_right"] == "open", "Unexpected Zenodo deposit fields")
    require({meta["url"], meta["codeRepository"]}.issubset({i["identifier"] for i in zenodo["related_identifiers"]}),
            "Zenodo metadata missing source or website")
    require(meta["@context"] == "https://w3id.org/codemeta/3.0" and meta["@type"] == "SoftwareSourceCode",
            "Unexpected CodeMeta type or context")
    require(set(cff["keywords"]) == set(zenodo["keywords"]) == set(meta["keywords"]), "Keywords differ")
    for name in ("README.md", "README.pt.md"):
        text = (ROOT / name).read_text(encoding="utf-8")
        for format_name, citation in format_citations(meta).items():
            require(citation in text, f"{name}: archived {format_name} citation differs from metadata")
        require(concept in text, f"{name}: project DOI missing")
    print(f"Research metadata and bilingual citations agree for {meta['version']} ({doi}).")


if __name__ == "__main__":
    main()
