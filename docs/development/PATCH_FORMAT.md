# Formato de patch · Patch format

[English](#english)

O painel exporta JSON na versão 3. Cada pino em `pins` é uma tripla `[linha, coluna, tipo]` com índices de 0 a 15. O tipo 1 representa 100% e o tipo 2 representa 50%. O vetor contém apenas pinos presentes. `controls` inclui `tune`, `shape`, `level`, `mix`, `lfo`, `cutoff` e os parâmetros em [parameters.def](../../app/core/parameters.def). O campo opcional `sequenceTrigger` seleciona o disparo pelo KS e tem valor padrão `false`. `name` é opcional.

| Campo discreto | Valores |
|---|---|
| `envMode` | 0 Gate, 1 Único, 2 Repetir |
| `triggerMode` | 0 Teclado/KS, 1 Limiar em C |

A importação valida versão, faixas, endereços e duplicatas antes de aplicar a alteração ao AudioWorklet. Um arquivo inválido preserva o patch e o transporte em uso. A versão 1 preenche os parâmetros adicionais com valores iniciais e traduz Mix em níveis complementares de seno e rampa. Em patches v1/v2, os pinos que usavam o antigo KS PITCH na linha 16 são migrados para o roteamento interno com ganho e destino preservados.

Na versão 3, `ksRoutes` registra conexões internas do KS como pares `[coluna, tipo]` em que a coluna vai de 0 a 15 e o tipo é 1 (100%) ou 2 (50%). Apenas as conexões ativas são listadas. Sem `ksMatrix` ou com valor `false`, a linha 16 é Stick Y. Com `ksMatrix: true`, a linha 16 recebe a afinação do KS e as conexões de `ksRoutes` ficam suspensas, mas preservadas no arquivo.

## Frase KS opcional

`sequence` aceita `lengthMs` inteiro de 1 a 30.000 e até 256 eventos `[milissegundo, nota MIDI, velocidade]`. Os eventos alternam ataque e soltura em ordem crescente. A soltura tem velocidade zero e repete a nota do ataque. O fim do último evento deve preceder a duração. O KS converte os milissegundos em quadros na taxa local.

O Felucca Pro 3.1 também aceita `[milissegundo, nota MIDI, velocidade, ataque]` em toda a frase, com `ataque` booleano. Esses estados preservam transições legato, inclusive o retorno a uma tecla mantida sem novo ataque. O primeiro evento tem velocidade positiva e ataque verdadeiro. Os eventos têm tempos distintos. Uma gravação interna é arredondada a milissegundos na transferência. Patches anteriores com pares de três campos continuam válidos.

A presença de `sequence` carrega a frase em pausa. A ausência do campo deixa o patch sem frase. O campo opcional `ks` especifica `realtimeSpread` e `sequenceSpread` de 25 a 200, `speed` de 25 a 400, `transpose` de −24 a 24, `limitSeconds` de 1 a 30 e `loop` booleano. O editor exporta os ajustes atuais. Ao importar um patch antigo com frase, o editor aplica loop ligado, velocidade e spread de 100% e transposição zerada.

Gravações com notas isoladas ou legato podem integrar o patch exportado. A coleção **Meus presets** mantém o mesmo formato JSON no navegador. O SMK mantém o patch ativo em RAM.

## English

[Português](#formato-de-patch--patch-format)

The editor exports v3 JSON. `pins` lists only occupied cells as `[row, column, type]`, with zero-based row and column indices 0–15. Types 1 and 2 mean 100% and 50%. `controls` covers `tune`, `shape`, `level`, `mix`, `lfo`, `cutoff` and entries from [parameters.def](../../app/core/parameters.def). The optional `name` identifies a patch. Optional `sequenceTrigger` defaults to `false`.

| Field | Values |
|---|---|
| `envMode` | 0 gate, 1 single, 2 repeat |
| `triggerMode` | 0 keyboard/KS, 1 threshold at C |

Import validates version, ranges, addresses and duplicates before applying to the AudioWorklet. Invalid input leaves the live patch and transport intact. Version 1 defaults newer parameters and expands Mix to complementary sine/ramp levels. For v1/v2, former KS pitch pins from row 16 migrate to equivalent internal routes. In v3, `ksRoutes` lists `[column, type]` using 0–15 and 1/2. When `ksMatrix` is missing or false, row 16 is Stick Y. When true, it carries KS pitch and preserves internal routes in a suspended state.

Optional `sequence` contains integer `lengthMs` from 1 to 30,000 and up to 256 events `[ms, MIDI note, velocity]` in increasing time order. Attack/release alternate. Release velocity is 0 and repeats the attacked note. The last event precedes the length. A four-field form adds a Boolean attack marker for legato. Use one format across the phrase. The first event has positive velocity and attack true in the four-field form. Events have distinct times. Recordings are rounded to milliseconds for transfer and converted to local audio frames.

A present sequence loads paused. Optional `ks` fields are `realtimeSpread` and `sequenceSpread` 25–200, `speed` 25–400, `transpose` −24 to 24, `limitSeconds` 1–30 and Boolean `loop`. Importing an older phrase defaults to loop on, speed/spread 100%, transpose 0. Browser presets retain the JSON format. On-device, the active patch lives in RAM until saved to a USER slot. [Protocol details](WEB_STUDIO_PROTOCOL.en.md)
