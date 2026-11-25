# Compilação local · Local builds

[Português](#português) · [English](#english)

## Português

Instale Git, Make, um compilador C11, Python 3 e Node.js. O adaptador FM‑1 usa Pillow para rasterizar as fontes. Para as revisões fixadas das dependências e o build completo do SMK‑37 Pro, use o guia de compilação e publicação em `docs/development/GITHUB_RELEASE.md` no repositório.

- `make all` compila o renderizador em C. O exemplo abaixo exporta um WAV a 48 kHz.
- `make test` executa as regressões do núcleo C e do editor JavaScript.
- `make serve` monta o HTML autônomo em `build/web/index.html`. Abra `http://127.0.0.1:8000/`.
- `tools/build_github.sh fm-1-host` gera o simulador Linux x86‑64. A saída de testes nativos fica em `build/fm1-host/`.

## English

Install Git, Make, a C11 compiler, Python 3 and Node.js. The FM‑1 adapter uses Pillow to rasterize fonts. See `docs/development/GITHUB_RELEASE.en.md` in the source repository for dependency revisions and the full SMK‑37 Pro build.

- `make all` builds the C renderer. The example below exports a 48 kHz WAV.
- `make test` runs the C and JavaScript editor regression tests.
- `make serve` builds the standalone HTML in `build/web/index.html`. Open `http://127.0.0.1:8000/`.
- `tools/build_github.sh fm-1-host` builds the Linux x86‑64 simulator. Native test output goes to `build/fm1-host/`.

```sh
make all
./build/zivisynthi-host sine.wav 69 sine
make test
make serve
```
