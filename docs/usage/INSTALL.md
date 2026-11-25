# Instalação · ziviSynthi AKS 3.9.6

[English](INSTALL.en.md) · [Avisos e créditos](../../NOTICES.md#português)

## Experimentar sem o SMK

Abra o Web Studio no GitHub Pages ou o arquivo `zivisynthi-aks-editor.html` da pasta `artefatos/web-studio/` no Chrome ou Edge. Clique em **Experimentar / Try** para ativar o áudio e ir ao teclado virtual. Toque com o mouse, por toque ou pelas teclas **A W S E D F T G Y H U J K**. O modo local aceita edição de pinos e timbres, importação e exportação de patches JSON e reprodução de frases no **KS**. Para testar uma frase de fábrica, carregue um preset do banco **Sequências KS** e pressione **Play** no KS. O transporte da grade **SEQ** controla o SMK vinculado.

O HTML reúne o sintetizador, os timbres e a interface. Se o navegador bloquear o áudio ao abrir pelo disco, execute `python3 -m http.server 8000 --bind 127.0.0.1` na pasta do HTML e abra [o editor por localhost](http://127.0.0.1:8000/zivisynthi-aks-editor.html).

## Instalar no SMK‑37 Pro

O arquivo `zivisynthi-felucca-smk37-3.9.6.fwsc` é destinado ao **M‑VAVE SMK‑37 Pro**. O arquivo `zivisynthi-fm1-host-linux-x86_64.tar.gz` é um simulador para computador. A identidade prevista depois de instalar o FWSC é `SMK-37 Pro_99500`.

1. Na página inicial do GitHub Pages, clique em **Instalar firmware** para abrir o Web Studio com o pacote incorporado e os downloads. Para instalar com uma cópia local, baixe o editor e o pacote de firmware em **Releases** ou no artefato **zivisynthi-aks-smk37-pro** do GitHub Actions. Confira `SHA256SUMS.txt` em Linux com `sha256sum -c SHA256SUMS.txt` no diretório extraído.
2. Exporte patches pessoais e sequências que deseja preservar. Confirme a versão de origem e consulte a [compatibilidade e recuperação](SMK37_FELUCCA_BANCADA.md#instalação-e-compilação). Uma instalação existente da linha Open precisa do procedimento específico para mudar de layout de flash.
3. Abra o Web Studio em Chrome ou Edge. Para servir a cópia local do artefato de release, abra a pasta que contém o editor, execute `python3 -m http.server 8000 --bind 127.0.0.1` e navegue para [o editor local](http://127.0.0.1:8000/zivisynthi-aks-editor.html).
4. Ligue o teclado por USB, abra **Firmware**, autorize USB MIDI e identifique o dispositivo. Selecione a 3.9.6 incorporada ou carregue o FWSC baixado. Confirme a operação e espere a identificação após o reinício.

O Studio aceita arquivos identificados por SHA‑256 para reversão. O build padrão do GitHub Actions incorpora a 3.9.6. Para usar a 3.9.5, selecione o FWSC correspondente como arquivo externo ou gere o editor com `FIRMWARE_ROLLBACK`. A [recuperação de fábrica](https://github.com/m-vave/smk37_open#updates-and-recovery) segue o procedimento M‑VAVE para o Pro.

A verificação desta versão cobre o simulador, a imagem e o protocolo do atualizador. Confira o [roteiro físico de validação](../development/HARDWARE_VALIDATION_PRO.md) para gravação e áudio no instrumento.
