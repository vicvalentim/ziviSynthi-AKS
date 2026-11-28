# Publicação científica e preservação

[English](SCIENTIFIC_RELEASE.en.md) · [Publicação científica e citação](../usage/SCIENTIFIC_PUBLICATION.md)

## Versão de referência e evidências

O depósito científico da 3.9.7 tem data de **26 de novembro de 2026**. A distribuição no GitHub é uma **pré-release**. A tag `v3.9.7` aponta para `008029810adfb4bbac43c7ad284d080ceec165ea`, commit que também consta no comentário do ZIP de fontes. Antes desta revisão, os dois READMEs já traziam links DOI. O site é gerado em Python a partir de Markdown via Pandoc e publicado pelo GitHub Actions. [Release](https://github.com/vicvalentim/ziviSynthi-AKS/releases/tag/v3.9.7) · [Código da tag](https://github.com/vicvalentim/ziviSynthi-AKS/tree/v3.9.7)

| Verificação | Resultado |
|---|---|
| Editora, autoria e DOI | Zenodo e DataCite coincidem na versão 3.9.7, no nome Victor Hugo Soares Valentim, na afiliação UFRB, no ORCID 0000-0002-0282-7947 e no DOI da versão `10.5281/zenodo.24617384`. O DataCite registra a relação `IsVersionOf` com o Concept DOI `10.5281/zenodo.24617383`. |
| Arquivos no Zenodo | Um arquivo, `zivisynthi-aks-source.zip` (3.024.816 bytes). Contém a aplicação, a receita de build e `fm1-base-source.tar.gz` com o framework e o BSP modificados, incluindo seus avisos. |
| Integridade do ZIP de fontes | O MD5 `1eaddf47b18595f0d4725f74f0ca45df` coincide com o Zenodo. O SHA‑256 `982f10beaf7329a2d165f24ce10d8cacb6c83afa97fb547a6e299e5d61408543` coincide com o arquivo da release do GitHub. Os 193 arquivos da aplicação correspondem aos objetos Git da tag. |
| Distribuição no GitHub | As seis entradas do `SHA256SUMS.txt` original foram conferidas contra os arquivos baixados. O FWSC tem SHA‑256 `896e8839b2991a1171d7d0f071461bbef55cff10a9259c8a46fba07581d22082`. |
| Cobertura do depósito | O FWSC, os HTMLs autônomos, o ZIP separado de presets, a lista de checksums do GitHub, o SDK e o compilador estão fora do depósito Zenodo. O acesso ao SDK e ao compilador depende dos fornecedores. |
| Firmware e validação | O `firmware.json` identifica o SMK‑37 Pro e GPL‑3.0‑only. O atualizador reconhece `SMK-37 Pro_99600`. As notas registram testes automatizados de pacote, mapa de flash e atualizador. A gravação desta compilação em uma unidade física segue pendente. |
| Indexação externa | O altstrument lista a 3.9.7 como pré-release para o Pro. A categoria de dispositivos reúne Pro e Elite. Não foi possível confirmar um registro da tag no Software Heritage, cuja API pública retornou HTTP 403. |

[API do Zenodo](https://zenodo.org/api/records/24617384) · [Metadados DataCite](https://api.datacite.org/dois/10.5281/zenodo.24617384) · [Série no DataCite](https://api.datacite.org/dois/10.5281/zenodo.24617383) · [Lista SHA‑256 original](https://github.com/vicvalentim/ziviSynthi-AKS/releases/download/v3.9.7/SHA256SUMS.txt) · [altstrument](https://altstrument.org/firmware/m-vave-smk-37/zivisynthi-aks)

## Reconstruir a 3.9.7

Em Linux x86‑64, use a tag e siga o [guia de compilação da v3.9.7](https://github.com/vicvalentim/ziviSynthi-AKS/blob/v3.9.7/docs/development/GITHUB_RELEASE.md). O guia contém os commits da fm1-base e do felucca-bsp, a tag e o commit do SDK JieLi AC79NN, a versão e o hash do pacote do compilador PI32v2, os requisitos de Python/Pillow e as variáveis de configuração. O ZIP preserva também a base modificada da release. Mantenha o `SHA256SUMS.txt` original e grave as recompilações em outro diretório.

```sh
git clone --branch v3.9.7 https://github.com/vicvalentim/ziviSynthi-AKS.git
cd ziviSynthi-AKS
git rev-parse HEAD
make -j2 test
tools/build_github.sh smk37-pro
```

O alvo `smk37-pro` compila a configuração Felucca para o Pro, executa testes de host e do atualizador, confere o SHA‑256 reconhecido do FWSC e prepara os arquivos em `dist/smk37-pro/`. A auditoria comparou os arquivos publicados e depositados conforme a tabela. A reprodutibilidade bit a bit de todos os artefatos, em particular dos arquivos compactados gerados, segue sem verificação. [Instalação e reversão](../usage/INSTALL.md) · [Notas de validação 3.9.7](../releases/v3.9.7.md#português)

Nas alterações documentais, o Pages recebe os downloads já publicados e o `SHA256SUMS.txt` original. O `PAGES-SHA256SUMS.txt` é gerado separadamente para o site. O parâmetro `--published` de `tools/stage_github_pages.py` prepara esse modo localmente.

Os metadados passaram pela validação CFF 1.2.0, pelo desserializador legado que o importador do Zenodo usa para GitHub e pela expansão JSON-LD com o contexto oficial CodeMeta 3.0. O CI confere a coerência dos metadados e das citações bilíngues sem depender de consultas ao Zenodo ou ao DataCite. O site gera um sitemap do projeto. Diretivas para rastreadores no GitHub Pages pertencem à raiz do domínio, acima do caminho deste repositório.

## Licenças e atribuição

A licença GPL‑3.0‑only da aplicação coincide com os avisos e o manifesto público. A base arquivada contém os textos MPL 2.0 de fm1-base e felucca-bsp, avisos do BSP e termos GPL, Apache e BSD de componentes específicos. O transporte no Web Studio conserva os cabeçalhos originais e o texto MPL. A tipografia do Web Studio tem licença SIL OFL 1.1, e a base modificada preserva o aviso da fonte Terminus. O SDK e o compilador JieLi são obtidos separadamente. A API legada do Zenodo retorna o identificador `gpl-3.0`, com o rótulo “GNU General Public License v3.0 only” no DataCite. O repositório usa o identificador SPDX `GPL-3.0-only`. O arquivo de submissão usa o identificador `gpl-3.0-only` do vocabulário atual do Zenodo. [Avisos](../../NOTICES.md#português) · [Vocabulário de licenças](https://zenodo.org/api/vocabularies/licenses/gpl-3.0-only)

A auditoria conferiu os avisos e o ZIP de fontes. Uma determinação jurídica de compatibilidade para cada componente ligado, inclusive bibliotecas do fornecedor, está fora do alcance dessa verificação. A ilustração do Synthi identifica a coleção MESS como referência visual. Preserve o crédito e verifique os direitos antes de redistribuir as fotografias de referência. [Coleção MESS](https://mess.foundation/product/synthi-aks/)

## Manter o registro científico

1. Defina a próxima versão do software após revisar as mudanças no DSP, no hardware e no navegador. Execute o CI e documente equipamento, compilação, identidade USB e resultados dos testes físicos.
2. Compile os artefatos, verifique os manifestos, gere os checksums e publique no GitHub a release e o código-fonte correspondentes ao commit testado. O parâmetro manual `publish_new_release` do Pages é reservado à publicação planejada de uma nova pré-release. Pushes normais reutilizam os downloads existentes.
3. No registro publicado no Zenodo, selecione **New version**. Inclua as fontes da nova versão, o pacote de firmware, editores autônomos, presets, manifesto, hashes e a indicação das dependências externas, conforme os direitos de distribuição. Substitua os arquivos herdados no novo rascunho pelos da versão correspondente.
4. Confira autoria, ORCID, afiliação, licença, data e URL da release. Publique na mesma série. Verifique o DOI da versão e confirme no DataCite que a relação `IsVersionOf` aponta para o Concept DOI existente.
5. Após a conferência, atualize `CITATION.cff`, `.zenodo.json`, `codemeta.json`, as referências APA/ABNT/BibTeX dos READMEs e o guia científico. O site usa CodeMeta nos componentes de citação e no JSON-LD. Atualize os catálogos quando for pertinente à release.
6. Confira os links e as páginas nos dois idiomas. Após a integração do PR, confirme a presença de **Cite this repository** na branch padrão do GitHub.

Mantenha a versão já depositada no metadado de citação enquanto a seguinte estiver em desenvolvimento. O registro atual foi depositado manualmente. Use o fluxo de nova versão para conservar seu Concept DOI. Uma integração automática com o GitHub só deve ser ativada após confirmar que ela consegue continuar essa série. Para a 4.0.0, conclua a validação física dos equipamentos de destino e inclua no novo depósito os artefatos distribuíveis e seus checksums. [Gestão de versões no Zenodo](https://help.zenodo.org/docs/deposit/manage-versions/) · [Arquivos de citação no GitHub](https://docs.github.com/pt/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-citation-files)
