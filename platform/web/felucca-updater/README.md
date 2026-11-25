# Transporte Felucca · Felucca transport

[English](#english)

Os arquivos `fm1pkg.js`, `fm1ports.js` e `fm1ota.js` vêm de `web/` no fm1-base, revisão `eafac162b4d7bc59176919a36eed179955315f93`, sob MPL 2.0. O código-fonte correspondente está nesta pasta e no pacote distribuído do projeto. Os créditos permanecem nos cabeçalhos e a licença está em [MPL-2.0.txt](MPL-2.0.txt).

O Studio confere o checksum e o tamanho da resposta de identidade. Na resposta de conclusão, devolve a flag da requisição e trata falha de envio como desconexão. O adaptador em `firmware.js` seleciona o par USB, restringe origem e carregador ao Pro e valida o SHA‑256 do pacote antes de transferi-lo.

## English

`fm1pkg.js`, `fm1ports.js` and `fm1ota.js` come from `web/` in fm1-base at revision `eafac162b4d7bc59176919a36eed179955315f93` under MPL 2.0. The corresponding source is included here and in the project distribution. Credits remain in the headers. [MPL-2.0 license](MPL-2.0.txt)

The Studio verifies the checksum and length of the identity response. In the completion reply, it echoes the request flag and treats a failed send as a disconnect. The adapter in `../firmware.js` selects USB endpoints, restricts source devices and the bootloader to the Pro, and verifies the package SHA‑256 before transfer.
