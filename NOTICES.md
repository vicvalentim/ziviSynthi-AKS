# Avisos e créditos · Notices and credits

[Português](#português) · [English](#english)

## Português

**ziviSynthi AKS** é um projeto independente de Victor Valentim, inspirado no EMS Synthi AKS. Os nomes EMS, M‑VAVE e Felucca identificam o instrumento de referência, o controlador e a base técnica. O projeto é desenvolvido sem afiliação ou endosso dessas marcas ou dos respectivos mantenedores.

O pacote de firmware 3.9.7 destina-se ao **M‑VAVE SMK‑37 Pro**. Antes de gravar, confirme o modelo e o firmware de origem, faça cópias de patches e sequências e consulte o procedimento de recuperação do Pro. Uma gravação pode apagar dados, e uma interrupção pode impedir a inicialização.

O código e os materiais autorais são distribuídos sob [GNU GPL versão 3 somente](LICENSE) (`GPL-3.0-only`). Aplicam-se os termos de garantia e responsabilidade das seções 15 e 16. Os componentes de terceiros conservam seus avisos e suas licenças.

Ao distribuir o FWSC, disponibilize também o código-fonte correspondente. O arquivo `zivisynthi-aks-source.zip` reúne o código da aplicação no commit da compilação e `fm1-base-source.tar.gz`, que contém a base e o BSP modificados com seus avisos e licenças.

## English

**ziviSynthi AKS** is an independent project by Victor Valentim, inspired by the EMS Synthi AKS. The names EMS, M‑VAVE and Felucca identify the reference instrument, the controller and the technical base. The project has no affiliation with or endorsement from these brands or their maintainers.

The firmware package for version 3.9.7 targets the **M‑VAVE SMK‑37 Pro**. Before flashing, verify your model and current firmware, back up patches and sequences, and review the Pro recovery procedure. Flashing may erase data, and an interrupted write may prevent startup.

The original code and assets are distributed under the [GNU GPL version 3 only](LICENSE) (`GPL-3.0-only`). The warranty and liability terms in sections 15 and 16 apply. Third-party components retain their own notices and licenses.

When distributing the FWSC, also make the corresponding source available. The `zivisynthi-aks-source.zip` archive includes the application at the build commit and `fm1-base-source.tar.gz`, which contains the patched base and BSP with their notices and licenses.

## Componentes · Components

A ilustração autoral do Synthi na página do projeto identifica o exemplar da coleção MESS como referência visual. / The project's original Synthi illustration credits the MESS collection instrument as its visual reference. [MESS collection](https://mess.foundation/product/synthi-aks/)

| Componente / Component | Origem e termos / Source and terms |
|---|---|
| Framework, simulador e transporte web / Framework, simulator and web transport | [fm1-base](https://github.com/charlesvestal/fm1-base), Charles Vestal et al. MPL-2.0 nos arquivos da base / MPL-2.0 for base files. A cópia do transporte e sua licença ficam em `platform/web/felucca-updater/` / The transport copy and its license are in `platform/web/felucca-updater/`. |
| BSP | [felucca-bsp](https://github.com/hugelton/felucca-bsp), Leo Kuroshita / Hügelton Instruments et al. MPL-2.0. Ver / See `bsp/NOTICE` and `LICENSES/` for other terms. |
| Bluetooth | Contribuições de / Contributions by Optimist (Henri David). SBC e tabelas de rádio / SBC and radio tables include Apache-2.0 code. micro-ecc, Kenneth MacKay, BSD-2-Clause. |
| Tipografia / Typography | Barlow Semi Condensed, SIL OFL 1.1. |
| SDK e compilador / SDK and compiler | JieLi. Obtidos durante o build / Fetched at build time. |
