# Alvos de firmware · M‑VAVE

Escolha pelo modelo inscrito no instrumento e pelo firmware instalado. O arquivo `.fwsc` contém um pacote de instalação específico para o modelo. Faça uma cópia dos presets e das sequências e use o atualizador correspondente.

| Modelo e base | Alvo de compilação | Saída e caminho de instalação |
|---|---|---|
| SMK‑37 Pro · Felucca | `smk37-pro` | ziviSynthi AKS **3.9.7** publicado, identidade `SMK-37 Pro_99600`. Use o [guia de instalação do Web Studio](INSTALL.md). |
| SMK‑37 Pro · Open V01 | `smk37-open-pro` | Compilação de desenvolvimento **0.8.2-alpha3**, identidade `SMK-37 Pro_8900823`, família 8 do SDK Open. Use o atualizador HTML correspondente e siga a conferência da origem. |
| SMK‑37 Elite · Open V01 | `smk37-open-elite` | Compilação de desenvolvimento **0.8.2-alpha3**, identidade `SMK-37 Elite_8900823`, família 9. Use o atualizador HTML correspondente a partir do Open V01 oficial do **Elite**. |
| FM‑1 · fm1-base | `fm-1` | Firmware de desenvolvimento **0.1.0-alpha1** em `.fwsc` próprio do FM‑1. Use as [instruções de instalação e recuperação da fm1-base](https://charlesvestal.github.io/fm1-base/install/) com esse pacote. O painel e a operação por USB seguem a implementação do FM‑1. |
| Linux x86‑64 | `fm-1-host` | Simulador para computador, distribuído em `.tar.gz`. |
| Navegador | `web-studio` | Editor e sintetizador independentes em `.html`. |

As compilações Open para Pro e Elite compartilham o instrumento, com identificadores e famílias de atualização distintos. Os atualizadores HTML identificam o modelo por USB antes de habilitar a instalação. O Web Studio do Pro vincula-se ao firmware Felucca para edição ao vivo e atualização. Use os atualizadores próprios para Open e FM‑1. A M‑VAVE documenta imagens Open e de fábrica específicas para Pro e Elite. [Open e recuperação da M‑VAVE](https://github.com/m-vave/smk37_open#updates-and-recovery)

## Compilar e exportar

Abra **Actions → Build firmware and editor → Run workflow** e selecione o alvo. O workflow cria um artefato separado para cada saída e inclui `SHA256SUMS.txt`. A opção `all` cria os seis artefatos.

Em Linux x86‑64, na raiz do repositório, escolha o comando correspondente.

```sh
tools/build_github.sh smk37-pro
tools/build_github.sh smk37-open-pro
tools/build_github.sh smk37-open-elite
tools/build_github.sh fm-1
tools/build_github.sh fm-1-host
tools/build_github.sh web-studio
```

Os arquivos ficam em `dist/<alvo>/`. Se já tiver o kit correspondente, aponte `SMK37_OPEN_SDK` para sua raiz antes de compilar um alvo Open. A compilação confere o manifesto do SDK e os hashes dos arquivos. Os firmwares do FM‑1 e do SMK‑37 Pro incluem a base correspondente em sua distribuição de código-fonte.
