# ziviSynthi AKS

[Português](README.pt.md)

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.24617384.svg)](https://doi.org/10.5281/zenodo.24617384)

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
