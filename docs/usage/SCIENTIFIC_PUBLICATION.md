# Publicação científica e citação

[English](SCIENTIFIC_PUBLICATION.en.md) · [Como citar](../../README.pt.md#como-citar)

**ziviSynthi AKS: An Open-Source Pin-Matrix Synthesizer Firmware Inspired by the EMS Synthi AKS** é um instrumento de pesquisa de Victor Hugo Soares Valentim (Victor Valentim), Universidade Federal do Recôncavo da Bahia (UFRB), Brasil. O projeto investiga luteria digital, computação musical, áudio embarcado e reinterpretações computacionais de instrumentos eletrônicos históricos. [ORCID](https://orcid.org/0000-0002-0282-7947) · [Contato](mailto:zivito@ufrb.edu.br)

| Identifica | Referência |
|---|---|
| Software depositado na versão 3.9.7, de 26 de novembro de 2026 | [10.5281/zenodo.24617384](https://doi.org/10.5281/zenodo.24617384) |
| Série de versões depositadas | [10.5281/zenodo.24617383](https://doi.org/10.5281/zenodo.24617383) |
| Distribuição do firmware | [GitHub v3.9.7](https://github.com/vicvalentim/ziviSynthi-AKS/releases/tag/v3.9.7) |
| Projeto e catálogo | [Site e Web Studio](https://vicvalentim.github.io/ziviSynthi-AKS/pt/) · [Código-fonte](https://github.com/vicvalentim/ziviSynthi-AKS) · [altstrument](https://altstrument.org/firmware/m-vave-smk-37/zivisynthi-aks) |

Use o DOI da versão quando citar o software utilizado. O README apresenta os formatos APA, ABNT e BibTeX. [CITATION.cff](../../CITATION.cff) · [Metadados do Zenodo](https://zenodo.org/api/records/24617384) · [Metadados do DataCite](https://api.datacite.org/dois/10.5281/zenodo.24617384)

## Arquitetura do software e contribuição

A matriz 16 × 16 de sinal e modulação, os três osciladores, a fonte de ruído, o filtro ressonante, a modulação em anel, a envoltória trapezoidal e a reverberação de mola são inspirados no EMS Synthi AKS. O núcleo DSP em C implementa um modelo digital de síntese e roteamento. O sequenciador de teclado inspirado no KS registra eventos de execução. O SEQ oferece até 64 passos, ao lado do ARP e do SCALE. [Referência da matriz](MATRIX_REFERENCE.md) · [Motor KS](KS_SEQUENCER.md)

O instrumento inclui 128 timbres e quatro efeitos de áudio USB. O firmware Felucca usa fm1-base e felucca-bsp como suporte ao M‑VAVE SMK‑37 Pro. O Web Studio bilíngue edita patches, presets e sequências por USB MIDI/SysEx em comunicação bidirecional. O modo Try usa síntese no navegador sem o controlador e permite processar um arquivo local pelos patches de efeitos USB. [Áudio USB](ENTRADA_AUDIO_USB.md) · [Protocolo USB Studio](../development/WEB_STUDIO_PROTOCOL.md)

A contribuição está no desenvolvimento de um instrumento tocável que permite examinar e reutilizar procedimentos de síntese, roteamento, sequenciamento e controle digital. A modelagem dos circuitos e dos modos de interação é uma interpretação computacional. A equivalência eletrônica com o instrumento da EMS ainda não foi demonstrada. O projeto é independente da EMS e da M‑VAVE. A release tem verificações automatizadas de pacote, mapa de flash e atualizador. A gravação desta compilação 3.9.7 no teclado físico segue pendente. [Notas da versão](../releases/v3.9.7.md#português)

## Preservação e licença

O [depósito no Zenodo](https://zenodo.org/records/24617384) contém o código-fonte correspondente. O GitHub Releases distribui o firmware compilado, os dois editores autônomos, o banco de presets e os checksums. O altstrument é um catálogo externo de firmware. O projeto autoral usa GPL‑3.0‑only. Os materiais de terceiros conservam os termos dos respectivos avisos. [Relatório de preservação](../development/SCIENTIFIC_RELEASE.md) · [Avisos e créditos](../../NOTICES.md#português)
