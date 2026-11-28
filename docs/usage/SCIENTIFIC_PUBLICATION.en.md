# Scientific publication and citation

[Português](SCIENTIFIC_PUBLICATION.md) · [How to cite](../../README.md#how-to-cite)

**ziviSynthi AKS: An Open-Source Pin-Matrix Synthesizer Firmware Inspired by the EMS Synthi AKS** is a research instrument by Victor Hugo Soares Valentim (Victor Valentim), Universidade Federal do Recôncavo da Bahia (UFRB), Brazil. The project studies digital lutherie, musical computing, embedded audio and computational reinterpretations of historical electronic instruments. [ORCID](https://orcid.org/0000-0002-0282-7947) · [Contact](mailto:zivito@ufrb.edu.br)

| Identifies | Reference |
|---|---|
| Archived software version 3.9.7, issued 26 November 2026 | [10.5281/zenodo.24617384](https://doi.org/10.5281/zenodo.24617384) |
| Series of archived versions | [10.5281/zenodo.24617383](https://doi.org/10.5281/zenodo.24617383) |
| Firmware distribution | [GitHub v3.9.7](https://github.com/vicvalentim/ziviSynthi-AKS/releases/tag/v3.9.7) |
| Project and catalog | [Website and Web Studio](https://vicvalentim.github.io/ziviSynthi-AKS/) · [Source](https://github.com/vicvalentim/ziviSynthi-AKS) · [altstrument](https://altstrument.org/firmware/m-vave-smk-37/zivisynthi-aks) |

Use the version DOI when citing the software used. The README gives APA, ABNT and BibTeX. [CITATION.cff](../../CITATION.cff) · [Zenodo metadata](https://zenodo.org/api/records/24617384) · [DataCite metadata](https://api.datacite.org/dois/10.5281/zenodo.24617384)

## Software architecture and contribution

The 16 × 16 signal and modulation matrix, three oscillators, noise source, resonant filter, ring modulation, trapezoid envelope and spring-style reverb take inspiration from the EMS Synthi AKS. The C DSP core implements a digital synthesis and routing model. The KS-style keyboard sequencer records performance events. SEQ provides up to 64 steps, alongside ARP and SCALE. [Matrix reference](MATRIX_REFERENCE.md#english) · [KS engine](KS_SEQUENCER.md#english)

The instrument includes 128 patches and four USB audio effects. The Felucca build uses fm1-base and felucca-bsp for hardware support on the M‑VAVE SMK‑37 Pro. The bilingual Web Studio can edit patches, presets and sequences over bidirectional USB MIDI/SysEx. Try uses browser synthesis without hardware and can process a local audio file through the USB effect patches. [USB audio](ENTRADA_AUDIO_USB.md#english) · [USB Studio protocol](../development/WEB_STUDIO_PROTOCOL.en.md)

The contribution is the development of a playable instrument that makes patch routing, synthesis, sequencing and digital control available for examination and reuse. The modeled circuits and interaction are a computational interpretation. Electrical equivalence with the original EMS instrument has not been demonstrated. The project is independent of EMS and M‑VAVE. The release has automated checks of the package, flash layout and updater. A hardware flash test of this exact 3.9.7 build remains pending. [Release notes](../releases/v3.9.7.md)

## Preservation and license

The [Zenodo deposit](https://zenodo.org/records/24617384) preserves the corresponding source archive. GitHub Releases supplies the compiled firmware, both standalone editors, preset archive and checksums. altstrument is an external firmware catalog. The original project is GPL‑3.0‑only. Third-party material keeps the terms in its notices. [Preservation report](../development/SCIENTIFIC_RELEASE.en.md) · [Notices and credits](../../NOTICES.md#english)
