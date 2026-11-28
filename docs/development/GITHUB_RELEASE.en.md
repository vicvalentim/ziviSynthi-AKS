# GitHub builds and release preparation

[Português](GITHUB_RELEASE.md) · [Installation](../usage/INSTALL.en.md)

## First push

Create an empty repository on GitHub. Extract the package and open a terminal in `repositorio/`. Include the settings under `.github/` and the `.gitignore` file.

```sh
git init -b main
git add .
git status --short
git commit -m "Publish ziviSynthi AKS 3.9.7"
git remote add origin https://github.com/USER/REPOSITORY.git
git push -u origin main
```

If the remote already has commits, clone it, copy in the contents of `repositorio/` while preserving its `.git` directory, and review the changes before pushing. If you already pushed the previous export, also remove the documents listed in the ZIP’s `LEIA-ME.md`. The export’s `artefatos/` directory contains downloads for Releases.

## GitHub Actions

In **Actions → Build firmware and editor → Run workflow**, select a target. The jobs run on Ubuntu x86‑64 and upload the results under **Artifacts**.

| Target | Output | Verification |
|---|---|---|
| `smk37-pro` | FWSC, editor with embedded firmware, JSON presets, manifest, SHA‑256 sums and patched base source | Felucca simulator, SysEx, FWSC inspection and updater tests. |
| `smk37-open-pro`, `smk37-open-elite` | Separate Open development images with matching HTML updaters | Model and package checks for each image. |
| `fm-1` | FM‑1 development firmware | FM‑1-specific package and build checks. |
| `fm-1-host` | Linux x86‑64 simulator in a `.tar.gz` archive | Matrix, KS, modules and presets in the simulator. |
| `web-studio` | Standalone HTML | Protocol, live editing and presets in Node.js. |
| `all` | All six sets | Runs each target independently. |

The **CI** workflow runs `make test` and checks the citation metadata and site generation on pushes and pull requests. Pages uses the published release downloads and original checksums on ordinary deployments. The Felucca Pro uses its own flash map and USB identity. The Open and FM‑1 device builds are development targets. See [firmware targets](../usage/FIRMWARE_TARGETS.en.md) for the matching installation paths.

## Build dependencies

The workflow fetches the pinned base, BSP and SDK and verifies the compiler version and the final Pro FWSC hash.

| Component | Pinned version |
|---|---|
| fm1-base, Pro | `eafac162b4d7bc59176919a36eed179955315f93` |
| felucca-bsp, Pro | `a702b840ae2bb7c4fbbd0a168ed509e129b2e9f7` |
| fm1-base, FM‑1 simulator | `2bb8e74f2962475eeeefd5179420bd8d73a3da6c` |
| felucca-bsp, FM‑1 simulator | `2bd9dbe1fcbb8c67ba6721b2435f3fb0597a202a` |
| JieLi AC79NN SDK 1.2.1 | `AC79NN_SDK_V1.2.1_2023-12-13` / `d179b4484759423312073f5fbb232501aa491047` |
| JieLi Linux PI32v2 | `20260730.1`, archive SHA‑256 `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd` |

If the manufacturer changes the download URL, set `ZIVI_JIELI_ARCHIVE_URL` in repository variables to a URL serving the pinned archive. `ZIVI_AC79_GIT_URL` accepts a mirror with the same SDK tag and commit.

### Local Linux build

Install Git, GNU Make, a C11 compiler, curl and Node.js 24. Then run:

```sh
python3 -m venv .deps/venv
source .deps/venv/bin/activate
python -m pip install Pillow==13.0.0
tools/build_github.sh all
```

Use `FM1_BASE`, `FM1_HOST_BASE`, `AC79_SDK` or `JIELI_TOOLCHAIN` to reuse local dependencies. Results go to `dist/`. To embed the optional 3.9.6 rollback, run `FIRMWARE_ROLLBACK=/path/to/3.9.6.fwsc tools/build_github.sh smk37-pro`.

## GitHub Pages

1. Push the source to GitHub. In **Settings → Pages → Build and deployment → Source**, select **GitHub Actions**.
2. In **Actions → Publish ziviSynthi AKS to GitHub Pages**, click **Run workflow** on the default branch. Once Pages is enabled, each push to that branch triggers deployment. The manual action can also be used to redeploy.
3. Copy the URL displayed in **Settings → Pages** into **About → Website** on the repository's main page.

| Path | Content |
|---|---|
| `/` and `/pt/` | English and Portuguese home pages with links to the editor, installation, guides and downloads |
| `/docs/` | Site documentation generated from the repository Markdown |
| `/studio/#try` | Web Studio with Try available without hardware |
| `/studio/#firmware-drawer` | SMK‑37 Pro firmware installer |
| `/firmware.json` | Firmware catalog with the site's actual URL |

The workflow packages the verified FWSC, offline editor, presets, corresponding sources and SHA‑256 checksums in the same deployment. It uses the URL returned by GitHub Pages, including custom domains. The old `/#firmware-drawer` link continues to open the installer. The source tree’s `firmware.json` can be left without a URL until a site exists.

To stage the same site locally, install Pandoc, build the `smk37-pro` target from a clean commit and run:

```sh
python tools/stage_github_pages.py dist/smk37-pro build/pages \
  --site https://example.github.io/zivisynthi-aks/ \
  --source https://github.com/example/zivisynthi-aks
python -m http.server 8000 --bind 127.0.0.1 --directory build/pages
```

Open [the local site](http://127.0.0.1:8000/). The example URLs are only used for the preview.

[GitHub Pages documentation](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)

## Publishing a pre-release

1. Check the root catalog URLs and screenshot path at the release revision.
2. Run **all** and deploy Pages from the same source revision. Publish the corresponding source alongside the FWSC. The Pages source archive includes the application and the patched base with its licenses.
3. For a planned new pre-release, run the Pages workflow manually with `publish_new_release` enabled on the chosen default-branch revision. After deployment, confirm the pre-release contains the notes, FWSC, both standalone HTMLs, manifest, presets, corresponding source and checksums. The [3.9.7 release notes](../releases/v3.9.7.md) record its physical validation status.
4. Once the release is public, submit the root `firmware.json` to the relevant firmware catalog.

[Scientific release and Zenodo versioning](SCIENTIFIC_RELEASE.en.md)

[GitHub Pages documentation](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages) · [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository) · [Catalog format](https://github.com/charlesvestal/fm1-base/blob/main/docs/FIRMWARE-JSON.md) · [Third-party notices](../../NOTICES.md#english)
