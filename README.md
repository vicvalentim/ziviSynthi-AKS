# ziviSynthi AKS

[Português](README.pt.md)

[![Project DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.24617383.svg)](https://doi.org/10.5281/zenodo.24617383)
[![License GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue)](LICENSE)
[![CI status](https://github.com/vicvalentim/ziviSynthi-AKS/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/vicvalentim/ziviSynthi-AKS/actions/workflows/ci.yml)
[![Pre-release v3.9.7](https://img.shields.io/badge/release-v3.9.7-blue)](https://github.com/vicvalentim/ziviSynthi-AKS/releases/tag/v3.9.7)
[Web Studio](https://vicvalentim.github.io/ziviSynthi-AKS/studio/#try)

A 16 × 16 pin-matrix synthesizer for the **M‑VAVE SMK‑37 Pro**, created by **Victor Valentim** and inspired by the EMS Synthi AKS. Three oscillators, a resonant filter, ring modulation, a trapezoid envelope and reverb. Includes a KS sequencer, a 64-step SEQ, ARP, 128 instrument presets and four USB effects.

**Version 3.9.7** for the M‑VAVE SMK‑37 Pro. [Release notes](docs/releases/v3.9.7.md) · [Notices and credits](NOTICES.md#english)

## Start here

| Task | Where to go |
|---|---|
| Try or install | Open the [project site](https://vicvalentim.github.io/ziviSynthi-AKS/) for Web Studio, documentation and downloads. Click **Try Web Studio** to play in the browser without hardware. To flash the Pro, connect it over USB and choose **Install firmware**. [Installation](docs/usage/INSTALL.en.md) |
| Play and edit | **K4** in Overview selects presets. **GLOBE** opens the modules. **PITCH** and **MOD** operate Stick X/Y. [Manual](docs/usage/SMK37_FELUCCA_BANCADA.en.md) |
| Use the browser studio | In **Patch Matrix → SMK preset**, choose any of the 132 factory patches. In Try, save or load a SEQ to send it to KS for playback. USB FX process a local audio file. [KS guide](docs/usage/KS_SEQUENCER.md#english) |
| Build and contribute | `make test` checks the C core and editor. `make serve` opens the Studio on localhost. [Build and GitHub Actions](docs/development/GITHUB_RELEASE.en.md) |

![16 × 16 matrix on the simulated SMK‑37 Pro display](docs/img/smk37-pro-matrix.png)

[Documentation](docs/README.md) · [Releases](https://github.com/vicvalentim/ziviSynthi-AKS/releases) · [Changelog](CHANGELOG.md) · [GPL‑3.0‑only](LICENSE)

## How to cite

Cite the archived 3.9.7 release when referring to that version in research or artistic work. The badge identifies the series of versions.

**APA**

Soares Valentim, V. H. (2026). ziviSynthi AKS: An Open-Source Pin-Matrix Synthesizer Firmware Inspired by the EMS Synthi AKS (Version 3.9.7) [Computer software]. Zenodo. https://doi.org/10.5281/zenodo.24617384

**ABNT**

SOARES VALENTIM, Victor Hugo. ziviSynthi AKS: An Open-Source Pin-Matrix Synthesizer Firmware Inspired by the EMS Synthi AKS. Versão 3.9.7. Zenodo, 2026. Software. DOI: 10.5281/zenodo.24617384.

**BibTeX**

```bibtex
@software{soaresvalentim_zivisynthi_aks_3_9_7,
  author = {Soares Valentim, Victor Hugo},
  title = {{ziviSynthi AKS: An Open-Source Pin-Matrix Synthesizer Firmware Inspired by the EMS Synthi AKS}},
  version = {3.9.7},
  year = {2026},
  publisher = {Zenodo},
  doi = {10.5281/zenodo.24617384},
  url = {https://doi.org/10.5281/zenodo.24617384}
}
```

## Preservation and indexing

The source archive is preserved at Zenodo. The firmware is listed in the altstrument catalog.

[Zenodo](https://zenodo.org/records/24617384) · [altstrument](https://altstrument.org/firmware/m-vave-smk-37/zivisynthi-aks) · [Research and preservation](docs/usage/SCIENTIFIC_PUBLICATION.en.md)
