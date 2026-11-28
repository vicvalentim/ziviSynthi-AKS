# Scientific release and preservation

[Português](SCIENTIFIC_RELEASE.md) · [Scientific publication and citation](../usage/SCIENTIFIC_PUBLICATION.en.md)

## Reference release and evidence

The 3.9.7 scientific deposit was issued on **26 November 2026**. The separately maintained GitHub distribution is a **pre-release**. The tag `v3.9.7` points to `008029810adfb4bbac43c7ad284d080ceec165ea`, also the commit identified in the source ZIP comment. The repository had added DOI links in both READMEs before this consolidation. The site is generated from Python and Markdown via Pandoc, and deployed by GitHub Actions. [Release](https://github.com/vicvalentim/ziviSynthi-AKS/releases/tag/v3.9.7) · [Tag source](https://github.com/vicvalentim/ziviSynthi-AKS/tree/v3.9.7)

| Check | Observation |
|---|---|
| Publisher, authorship and DOI | Zenodo and DataCite agree on version 3.9.7, Victor Hugo Soares Valentim, UFRB, ORCID 0000-0002-0282-7947 and version DOI `10.5281/zenodo.24617384`. DataCite links it by `IsVersionOf` to Concept DOI `10.5281/zenodo.24617383`. |
| Zenodo files | One file, `zivisynthi-aks-source.zip` (3,024,816 bytes). Contains the application, build recipe and `fm1-base-source.tar.gz` with the modified framework and BSP, including the upstream notices. |
| Source ZIP integrity | MD5 `1eaddf47b18595f0d4725f74f0ca45df` matches Zenodo. SHA‑256 `982f10beaf7329a2d165f24ce10d8cacb6c83afa97fb547a6e299e5d61408543` matches the downloaded GitHub release file. All 193 application files match the Git objects at the tag. |
| GitHub distribution | All six entries in the original `SHA256SUMS.txt` were checked against the downloaded assets. FWSC SHA‑256 is `896e8839b2991a1171d7d0f071461bbef55cff10a9259c8a46fba07581d22082`. |
| Scope of the deposit | The FWSC, standalone HTMLs, separate preset ZIP, GitHub checksum manifest, SDK and compiler are outside the Zenodo deposit. SDK and compiler acquisition depend on upstream availability. |
| Firmware and validation | `firmware.json` identifies the SMK‑37 Pro and GPL‑3.0‑only. The updater recognizes `SMK-37 Pro_99600`, and the release notes report automated package, flash-layout and updater tests. Flashing this exact build on a physical unit is pending. |
| External indexing | altstrument lists 3.9.7 as a pre-release for the Pro. Its device category groups Pro and Elite. A Software Heritage record for the tag could not be confirmed (the public API request returned HTTP 403). |

[Zenodo record/API](https://zenodo.org/api/records/24617384) · [DataCite metadata](https://api.datacite.org/dois/10.5281/zenodo.24617384) · [DataCite series](https://api.datacite.org/dois/10.5281/zenodo.24617383) · [Original SHA‑256 list](https://github.com/vicvalentim/ziviSynthi-AKS/releases/download/v3.9.7/SHA256SUMS.txt) · [altstrument](https://altstrument.org/firmware/m-vave-smk-37/zivisynthi-aks)

## Reconstruct 3.9.7

On Linux x86‑64, check out the tag and follow the [build guide at v3.9.7](https://github.com/vicvalentim/ziviSynthi-AKS/blob/v3.9.7/docs/development/GITHUB_RELEASE.en.md). It provides the fm1-base and felucca-bsp commits, JieLi AC79NN SDK tag and commit, PI32v2 toolchain version and archive hash, Python/Pillow requirements and configuration overrides. The source ZIP also preserves the exact patched base used in the release. Preserve the original `SHA256SUMS.txt` and use a separate output directory for rebuilt files.

```sh
git clone --branch v3.9.7 https://github.com/vicvalentim/ziviSynthi-AKS.git
cd ziviSynthi-AKS
git rev-parse HEAD
make -j2 test
tools/build_github.sh smk37-pro
```

The `smk37-pro` target builds the `SMK-37 Pro` Felucca configuration, runs host and updater tests, checks the recognized FWSC SHA‑256 and stages outputs under `dist/smk37-pro/`. The published assets and deposit were byte-compared as described above. The audit did not establish bit-for-bit reproducibility for every distributed artifact, especially generated archives. [Installation and rollback](../usage/INSTALL.en.md) · [3.9.7 validation notes](../releases/v3.9.7.md)

For documentation changes, Pages takes the original release downloads and `SHA256SUMS.txt` as inputs. It writes a separate `PAGES-SHA256SUMS.txt` for the full generated site. The `--published` flag in `tools/stage_github_pages.py` stages that mode locally.

Citation metadata passed CFF 1.2.0 validation, the Zenodo GitHub importer's legacy deserializer and JSON-LD expansion with the official CodeMeta 3.0 context. CI checks the agreement between the metadata and the bilingual references without consulting Zenodo or DataCite. The site generates a project sitemap. Crawler directives for the GitHub Pages hostname belong at the origin root, above this repository's site path.

## License and attribution audit

The application GPL‑3.0‑only declaration, the notices and the public manifest agree. The archived base carries MPL 2.0 texts for fm1-base and felucca-bsp, BSP notices and separate GPL, Apache and BSD terms for components. The transport extracted into Web Studio retains its original headers and MPL text. The Web Studio fonts carry SIL OFL 1.1, and the patched base archive includes the Terminus font notice. The JieLi SDK and compiler are obtained separately. Zenodo's legacy API emits the license identifier `gpl-3.0`, with the display label “GNU General Public License v3.0 only” in DataCite. The repository uses the exact SPDX identifier `GPL-3.0-only`, and the Zenodo submission metadata uses the current vocabulary ID `gpl-3.0-only`. [Notices](../../NOTICES.md#english) · [Zenodo license vocabulary](https://zenodo.org/api/vocabularies/licenses/gpl-3.0-only)

The audit checked the notices and source archive. A full legal determination of compatibility for every linked component, including the vendor libraries, is outside those checks. The Synthi artwork names the MESS collection as its visual reference. Keep that attribution and verify rights separately before redistributing any source photographs. [MESS collection](https://mess.foundation/product/synthi-aks/)

## Maintain the scientific record

1. Set the next software version after reviewing the DSP, hardware and browser changes. Run the existing CI suite and record the exact device, build, firmware identity and physical validation results.
2. Compile the release artifacts, check the manifests, generate SHA‑256 sums and publish the tested commit and matching source on GitHub. The Pages workflow's manual `publish_new_release` input is for a new planned pre-release. Ordinary pushes reuse existing downloads.
3. In the published Zenodo record, select **New version**. Attach the new source archive, firmware package, standalone editors, presets, manifest, checksums and a description of the external dependencies, to the extent authorized for distribution. Replace inherited files in the new draft with the files for the new version.
4. Verify authorship, ORCID, affiliation, license, date and release URL. Publish the version in the existing record series. Check the new version DOI and confirm in DataCite that its `IsVersionOf` relation uses the existing Concept DOI.
5. After verification, update `CITATION.cff`, `.zenodo.json`, `codemeta.json`, the APA/ABNT/BibTeX entries in both READMEs and the scientific guide. The website uses CodeMeta for its citation components and JSON-LD. Update catalog entries only when the release warrants it.
6. Verify the links and bilingual pages. After the PR is integrated, confirm that GitHub displays **Cite this repository** on the default branch.

Keep the currently deposited version in the citation metadata during development of the next version. The existing Zenodo record was deposited manually. Use the new-version workflow to retain the same Concept DOI. Enabling automated GitHub archiving would require confirmation that it can continue that existing series. For 4.0.0, complete physical validation of the intended hardware and include the distributable artifacts and original checksums in the new deposit. [Zenodo version management](https://help.zenodo.org/docs/deposit/manage-versions/) · [GitHub citation files](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-citation-files)
