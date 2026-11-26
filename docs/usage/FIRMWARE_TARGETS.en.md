# Firmware targets · M‑VAVE

Choose by the model printed on the instrument and by the firmware currently installed. A `.fwsc` file contains a model-specific installation package. Back up presets and sequences, and use the updater for that model.

| Device and base | Build target | Output and installation path |
|---|---|---|
| SMK‑37 Pro · Felucca | `smk37-pro` | Published ziviSynthi AKS **3.9.7**, identity `SMK-37 Pro_99600`. Use the [Web Studio installation guide](INSTALL.en.md). |
| SMK‑37 Pro · Open V01 | `smk37-open-pro` | Development build **0.8.2-alpha3**, identity `SMK-37 Pro_8900823`, SMK37 Open family 8. Use its matching HTML updater and follow the origin check. |
| SMK‑37 Elite · Open V01 | `smk37-open-elite` | Development build **0.8.2-alpha3**, identity `SMK-37 Elite_8900823`, SMK37 Open family 9. Use its matching HTML updater with the official **Elite** Open V01 as the origin. |
| FM‑1 · fm1-base | `fm-1` | FM‑1-specific development firmware **0.1.0-alpha1** in `.fwsc`. Use the [fm1-base installation and recovery instructions](https://charlesvestal.github.io/fm1-base/install/) with this package. Its panel and USB features follow the FM‑1 implementation. |
| Linux x86‑64 | `fm-1-host` | Desktop simulator archive `.tar.gz`. |
| Browser | `web-studio` | Standalone editor and browser synthesizer in `.html`. |

The Pro and Elite Open builds share the instrument application, with separate model identifiers and update families. Their HTML updaters establish the USB identity before installation. Link the Pro Web Studio to the Felucca build for live editing and firmware updates. Use the dedicated updaters for Open and FM‑1. M‑VAVE documents separate Open and factory images for Pro and Elite. [M‑VAVE Open and recovery](https://github.com/m-vave/smk37_open#updates-and-recovery)

## Build and export

Open **Actions → Build firmware and editor → Run workflow**, then select a target. The workflow creates a separate downloadable artifact for each target and includes `SHA256SUMS.txt`. The `all` option creates all six artifacts.

On Linux x86‑64, from the repository root, choose the matching command.

```sh
tools/build_github.sh smk37-pro
tools/build_github.sh smk37-open-pro
tools/build_github.sh smk37-open-elite
tools/build_github.sh fm-1
tools/build_github.sh fm-1-host
tools/build_github.sh web-studio
```

Find the exports under `dist/<target>/`. If you already have the matching SDK, set `SMK37_OPEN_SDK` to its root before running an Open target. The build checks the SDK's manifest and file hashes. The FM‑1 and SMK‑37 Pro firmware bring their corresponding base as part of their source distribution.
