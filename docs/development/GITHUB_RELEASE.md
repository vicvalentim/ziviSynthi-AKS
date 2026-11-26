# Compilação no GitHub e preparação de release

[English](GITHUB_RELEASE.en.md) · [Instalação](../usage/INSTALL.md)

## Primeiro envio

Crie um repositório vazio no GitHub. Extraia o pacote e abra o terminal em `repositorio/`, incluindo os arquivos de configuração de `.github/` e `.gitignore`. Dentro dessa pasta, execute:

```sh
git init -b main
git add .
git status --short
git commit -m "Publish ziviSynthi AKS 3.9.7"
git remote add origin https://github.com/USUARIO/REPOSITORIO.git
git push -u origin main
```

Se o repositório remoto já tiver commits, clone-o, copie para ele o conteúdo de `repositorio/` preservando o diretório `.git` e revise a alteração antes de enviar. Se já tiver enviado o export anterior, remova também os documentos da lista em `LEIA-ME.md` do ZIP. A pasta `artefatos/` do pacote contém os downloads para Releases.

## GitHub Actions

Abra **Actions → Build firmware and editor → Run workflow** e selecione um alvo. O job usa Ubuntu x86‑64 e entrega os arquivos em **Artifacts** depois da compilação e dos testes.

| Alvo | Saída | Verificação |
|---|---|---|
| `smk37-pro` | FWSC, editor com o firmware incorporado, presets JSON, metadados, SHA‑256 e fontes modificadas da base | Simulador Felucca, SysEx, inspeção do FWSC e testes do atualizador. |
| `smk37-open-pro`, `smk37-open-elite` | Imagens Open de desenvolvimento com atualizadores HTML próprios | Conferência do modelo e do pacote de cada imagem. |
| `fm-1` | Firmware de desenvolvimento para FM‑1 | Conferência do build e do pacote específico do FM‑1. |
| `fm-1-host` | Executável Linux x86‑64 em `.tar.gz` | Matriz, KS, módulos e presets no simulador. |
| `web-studio` | HTML autônomo | Protocolo, edição ao vivo e presets no Node.js. |
| `all` | Os seis conjuntos | Executa cada alvo independentemente. |

O workflow **CI** executa `make test` em pushes e pull requests. O pipeline de build produz artefatos para revisão. A pré-release v3.9.7 é publicada pelo workflow Pages após a implantação, com os artefatos do mesmo commit.

O alvo **Felucca Pro** tem mapa de flash e identidade USB próprios. Os builds Open e FM‑1 de dispositivo são alvos de desenvolvimento. Consulte os [alvos de firmware](../usage/FIRMWARE_TARGETS.md) para identificar o instalador correspondente.

## Dependências

O job busca as bases e o SDK em upstream e confere a versão do compilador. Os diretórios locais configurados por variáveis são úteis para repetir o mesmo build sem descarregar as dependências.

| Componente | Versão usada |
|---|---|
| fm1-base, alvo Pro | `eafac162b4d7bc59176919a36eed179955315f93` |
| felucca-bsp, alvo Pro | `a702b840ae2bb7c4fbbd0a168ed509e129b2e9f7` |
| fm1-base, simulador FM‑1 | `2bb8e74f2962475eeeefd5179420bd8d73a3da6c` |
| felucca-bsp, simulador FM‑1 | `2bd9dbe1fcbb8c67ba6721b2435f3fb0597a202a` |
| SDK JieLi AC79NN 1.2.1 | `AC79NN_SDK_V1.2.1_2023-12-13` / `d179b4484759423312073f5fbb232501aa491047` |
| JieLi Linux PI32v2 | `20260730.1`, SHA‑256 do arquivo `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd` |

Se a URL do fabricante mudar para outra versão, configure `ZIVI_JIELI_ARCHIVE_URL` nas variáveis do repositório com um endereço que entregue o arquivo fixado. `ZIVI_AC79_GIT_URL` permite usar uma origem alternativa com a mesma tag e o mesmo commit do SDK. O build do Pro também confere o hash final do FWSC com a tabela do atualizador.

### Mesmo build no Linux local

```sh
python3 -m venv .deps/venv
source .deps/venv/bin/activate
python -m pip install Pillow==13.0.0
tools/build_github.sh all
```

Requisitos locais adicionais são Git, GNU Make, um compilador C11, curl e Node.js 24. Para reutilizar dependências, forneça `FM1_BASE`, `FM1_HOST_BASE`, `AC79_SDK` ou `JIELI_TOOLCHAIN`. Os artefatos saem em `dist/`. Para incorporar também a reversão 3.9.6 no HTML, execute `FIRMWARE_ROLLBACK=/caminho/para/3.9.6.fwsc tools/build_github.sh smk37-pro`.

## GitHub Pages

1. Envie o código ao GitHub. Em **Settings → Pages → Build and deployment → Source**, selecione **GitHub Actions**.
2. Em **Actions → Publish ziviSynthi AKS to GitHub Pages**, clique em **Run workflow** na branch padrão. Depois de ativado o Pages, cada push nessa branch aciona a publicação. A execução manual também pode ser usada para republicar.
3. Copie a URL exibida em **Settings → Pages** para **About → Website** na página do repositório.

| Caminho | Conteúdo |
|---|---|
| `/` e `/pt/` | Páginas iniciais em inglês e português com acesso ao editor, à instalação, aos guias e aos downloads |
| `/docs/` | Documentação do site gerada a partir dos arquivos Markdown do repositório |
| `/studio/#try` | Web Studio com modo Try disponível sem teclado |
| `/studio/#firmware-drawer` | Instalador do firmware para o SMK‑37 Pro |
| `/firmware.json` | Catálogo de firmware com a URL do site publicado |

O workflow reúne o FWSC verificado, o editor offline, os presets, o código-fonte correspondente e os hashes SHA‑256 na mesma implantação. A URL vem do GitHub Pages e também funciona com domínio próprio. O endereço antigo `/#firmware-drawer` continua abrindo o instalador. O `firmware.json` do repositório pode ficar sem URL enquanto o site não existir.

Para montar o mesmo site localmente, instale o Pandoc, compile o alvo `smk37-pro` a partir de um commit limpo e execute:

```sh
python tools/stage_github_pages.py dist/smk37-pro build/pages \
  --site https://example.github.io/zivisynthi-aks/ \
  --source https://github.com/example/zivisynthi-aks
python -m http.server 8000 --bind 127.0.0.1 --directory build/pages
```

Abra [o site local](http://127.0.0.1:8000/). As URLs de exemplo servem à prévia local.

[Guia oficial do GitHub Pages](https://docs.github.com/pt/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)

## Publicação de uma pré-release

1. Confira as URLs no catálogo da raiz e o endereço da captura no commit da release. O arquivo `LICENSE` estabelece GPL-3.0-only para a aplicação e os materiais autorais.
2. Rode o alvo **all** no commit escolhido e publique o site pela branch padrão com esse mesmo conteúdo. Preserve o código-fonte correspondente ao distribuir o FWSC. O site oferece `zivisynthi-aks-source.zip` com a versão do aplicativo e a base modificada com suas licenças.
3. Após a implantação do Pages, confira se a pré-release **v3.9.7** inclui [as notas da versão](../releases/v3.9.7.md#português), o FWSC, os dois HTMLs autônomos, o manifesto, os presets, o código-fonte correspondente e os hashes. As notas informam o estado da validação física.
4. Após publicar, encaminhe o `firmware.json` da raiz aos catálogos de firmware.

[Formato de firmware.json](https://github.com/charlesvestal/fm1-base/blob/main/docs/FIRMWARE-JSON.md) · [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository) · [Avisos de terceiros](../../NOTICES.md#português)
