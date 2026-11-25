# Protocolo do Web Studio · Felucca Pro 3.9.5

[English](WEB_STUDIO_PROTOCOL.en.md)

O editor usa Web MIDI SysEx por USB em um contexto seguro, com autorização explícita. O modo virtual usa o AudioWorklet e os catálogos compartilhados com o núcleo C. A coleção de presets pessoais usa o armazenamento local do navegador e exporta o [formato JSON de patch](PATCH_FORMAT.md).

## Transporte

O cabeçalho da requisição é `F0 7D 5A 53 01 comando sérieLo sérieHi`, seguido dos dados em 7 bits e de `F7`. A resposta usa `comando | 0x40`, repete a série e inclui um byte de estado antes dos dados (`0` OK, `1` inválido, `2` incompleto, `3` CRC divergente). O editor espera uma resposta por comando e consolida alterações rápidas. HELLO identifica o modelo e o limite de 96 bytes por parte.

| Comando | Função |
| ---: | --- |
| 1 | HELLO. Retorna modelo, versão e tamanho máximo da parte. |
| 2–4 | Iniciar com tamanho e CRC-16/CCITT-FALSE, enviar partes com deslocamento, confirmar. |
| 5–7 | Alterar parâmetros, pinos e controles do KS. |
| 8 | Parar, reproduzir ou gravar no KS ativo. |
| 9–10 | Ler uma imagem do patch ativo em partes. A leitura durante gravação informa que a frase ainda está em andamento. |
| 11 | Attack manual com prazo de 750 ms, Joystick X, configuração de ARP/SCALE e leitura de estado de execução. |

A imagem contém seis controles rápidos, os parâmetros em ordem de `parameters.def` (IEEE 754 em cinco septetos), 256 posições da matriz em ordem de linha, dez bytes de ajustes do KS, duração em três septetos, um byte de formato e até 256 eventos. Cada evento tem o tempo em milissegundos, a nota e a velocidade. O formato zero usa os pares anteriores. O formato um codifica estados legato e usa o bit 20 do tempo para indicar ataque. O firmware valida limites, CRC e o conteúdo da imagem antes de aplicar a escrita. A transação expira após dois segundos sem parte e o contexto é descartado na mudança de sessão USB.

A imagem é aplicada ao sintetizador no laço de controle. O consumidor de áudio recebe blocos PCM da fila. A coleção pessoal persiste no navegador, e o SMK oferece três slots USER com gravação explicitamente solicitada. O módulo **Firmware** usa o protocolo de atualização Felucca por USB MIDI e suspende o vínculo Studio enquanto grava. A descrição da instalação está no [manual de bancada](../usage/SMK37_FELUCCA_BANCADA.md#instalação-e-compilação).

O firmware 3.2 responde ao HELLO com os bytes de versão `98 4` em decimal. A configuração ARP no comando 11 aceita o payload anterior `[2, ligado, bpmLo, bpmHi, gate, ordem]`. O formato novo acrescenta o índice de divisão `0..7`, na ordem 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32 e 1/32T. A consulta de desempenho mantém os 18 bytes anteriores. No byte de índice 10, os bits 2–4 informam a divisão e os bits 0–1 mantêm os sinalizadores de ARP habilitado e presença da nota. A configuração SCALE mantém o mesmo payload e os seis primeiros índices. O editor limita escalas e divisão aos recursos do firmware conectado.


O firmware 3.3 anuncia a revisão `98 5`. A ordem `3` seleciona notas aleatórias. O payload `[5, flags, chance]` configura HOLD no bit 0, FREE no bit 1 e chance de 0 a 100. A consulta `[6]` retorna os 18 bytes anteriores e mais dois. O byte 18 usa os bits 0 a 3 para HOLD, FREE, gravação SEQ e reprodução SEQ. O byte 19 informa chance. A consulta `[4]` continua com 18 bytes. Ao carregar uma imagem de patch com frase, o firmware importa seus eventos para o SEQ quando couberem. O painel publica o padrão em KS com KBANK para leitura pelo editor.


O Felucca Pro 3.5 anuncia revisão `98 6`. O comando READ recebe opcionalmente `[1]` para devolver o formato com dois eixos e KS interno. No byte de formato, o bit 0 retém a codificação de ataque, o bit 1 indica o novo formato e o bit 2 indica modo EMS. Com o bit 1 ativo, cada byte da linha 16 codifica `tipoDoPino + 3 × tipoDaRotaKS`, com tipos de 0 a 2. O número de bytes permanece igual. Uma imagem antiga com os bits 1 e 2 zerados migra a linha 16 para as rotas internas. Uma leitura antiga devolve na última linha as conexões de afinação efetivas.

No comando 11, `[8, valorLo, valorHi]` controla Stick Y, `[9, coluna, tipo]` altera uma rota interna e `[10, modo]` seleciona modo interno (0) ou EMS (1). O payload de Stick X, identificado por 1, permanece igual.


## Extensão da 3.8

HELLO devolve revisão `98 7`. READ com `[2]` inclui o bit 3 do formato e anexa uma extensão após os eventos KS. A leitura sem argumento ou com `[1]` continua disponível.

| Ordem | Campo |
|---|---|
| 1 | 32 pinos USB, esquerda A–P e direita A–P, valores 0/1/2. |
| 2 | 9 bytes de ARP/SCALE. Flags enabled/HOLD/FREE, BPM de 14 bits, gate, ordem, divisão, chance, tônica e escala. |
| 3 | 6 bytes de cabeçalho SEQ. Comprimento, BPM de 14 bits, BPM de referência de 14 bits e divisão. |
| 4 | 64 passos de 12 bytes. Nota, velocidade, legato, extensão, atraso e duração. Os três tempos usam 21 bits em quadros do DSP. |
| 5 | 24 CC, K1–K8 do banco A, K1–K8 do B, F1–F4 do A e F1–F4 do B. |

Os comandos 12–15 controlam, respectivamente, SEQ, slots USER, rotas USB e CC. Para SEQ, envie `[0, comprimento, bpmLo, bpmHi, divisão]`, `[1, índice, nota, velocidade]`, `[2, executar]` ou `[3]` para iniciar gravação. Para slots USER, `[0]` enumera, `[1, slot]` carrega e `[2, slot]` salva o patch ativo. Os slots são indexados de 0 a 2. O comando 14 recebe `[canal, coluna, quantidade]`. O comando 15 recebe `[índice, cc]`. Respostas 4 e 5 indicam erro de armazenamento e recurso temporariamente ocupado.


## Telemetria da 3.9

HELLO devolve revisão `98 8`. O comando 11 com payload `[7]` responde com os mesmos 20 bytes de `[6]` e acrescenta os campos abaixo.

| Índice | Campo |
|---:|---|
| 20 | Índice do passo em execução, de 0 a 63. Use o bit PLAY do byte 18 para exibir o cursor. |
| 21 | Fonte de clock no bit 0 (1 para MIDI externo) e sincronismo no bit 1. |
| 22–23 | BPM em dois septetos. Com clock externo sem sincronismo, o valor corresponde ao andamento interno. |

As consultas `[4]` e `[6]` e os formatos de patch e de flash mantêm a compatibilidade. O editor solicita a consulta `[7]` ao vincular um firmware com revisão 8 ou superior.


## Transferência independente do SEQ na 3.9.1

HELLO anuncia revisão `98 9`. O bloco do SEQ tem 774 bytes, iguais ao cabeçalho de 6 bytes e aos 64 passos de 12 bytes da extensão 3.8.

| Operação | Mensagens SysEx |
|---|---|
| Ler SEQ ativo | Comando 12 com payload `[4]`. Responde com tamanho em 2 septetos e CRC em 3. Ler as partes com o comando 10. |
| Aplicar SEQ | Comando BEGIN com tamanho e CRC, seguidos do tipo `1`. Enviar PART e COMMIT pelos comandos 3 e 4. Só aplica os passos após validar bloco e CRC. |
| Persistir SEQ | Comando 13 com `[3, slot]`. Substitui a grade em um slot USER ocupado. O slot preserva os parâmetros, as rotas, os CC e a frase KS armazenados anteriormente. |

A forma de BEGIN com cinco bytes continua destinada ao patch completo. O firmware retorna BUSY ao tentar substituir o SEQ com SEQ ou KS em execução. A leitura da grade é bloqueada enquanto estiver gravando. A forma em flash e os endereços dos presets anteriores permanecem compatíveis.

## Edição bidirecional na 3.9.3

HELLO anuncia revisão `98 10`. Durante a edição ao vivo, o editor compara a imagem que tem com a do SMK. Usa o comando 9 com `[3, tamanhoLo, tamanhoHi, crcLo, crcMid, crcHi]`. O retorno usa o cabeçalho de leitura existente. Tamanho zero indica imagem igual ou gravação em andamento. Com tamanho positivo, leia as partes pelo comando 10 e valide o CRC. O editor só incorpora a imagem quando a fila local de escrita está vazia. A comparação cobre controles, toda a matriz, rotas, ajustes e eventos do KS, ARP, SCALE, SEQ e endereços MIDI CC.

O comando 16 lê os controles gerais com payload vazio. O retorno tem 16 valores de dois septetos na ordem abaixo.

| Índices | Campos |
|---|---|
| 0–2 | Master, sintetizador e retorno USB, de 0 a 4096. |
| 3–6 | USB em monitor/matriz, canal de entrada (0 para todos), canal de saída (1–16) e destino (todos, TRS, USB, BLE). |
| 7–10 | Clock externo, envio de clock, BPM de referência do KS (30–300) e envio de notas do KS. |
| 11–13 | USB Thru, CC relativo dos knobs e indicação dos pads (fixa, animada ou apagada). |
| 14–15 | Joystick X e Y, bipolares de 14 bits com centro em 8192. |

Para editar os índices 0–13, envie `[índice, valorLo, valorHi]` no comando 16. Para escrever os eixos, use as operações 1 e 8 do comando 11. Pinos vermelhos e brancos compartilham o código de ganho integral no SMK. A cor vermelha é metadado do editor e é preservada quando a rota correspondente continua integral.

## Revisões incrementais e diagnóstico na 3.9.4

HELLO anuncia revisão `98 11`. O comando 17 recebe a revisão de 21 bits já aplicada pelo editor. Envie zero para iniciar ou refazer a leitura. O retorno começa com um tipo e três septetos de revisão.

| Tipo | Dados após a revisão |
|---:|---|
| 0 | Nenhum. O estado observado está igual, ou o firmware adiou a leitura durante gravação/transferência. |
| 1 | Alterações. Cada controle usa índice `0..40` e cinco septetos IEEE 754. Cada pino usa `[64, linha, coluna, tipo]`, com tipo 0 para remoção. |
| 2 | Cabeçalho de seis bytes do comando 9. Leia as partes com o comando 10 e confira o CRC. O firmware usa a imagem completa para conexão, alterações estruturais, revisões perdidas ou quando a lista ultrapassa 92 bytes. |

A revisão só é aplicada depois de incorporar a resposta e descartar o conflito com edições locais pendentes. A versão 3.9.3 pode continuar usando o comando 9 em modo de comparação. O dispositivo compara campos estáveis dos dados nativos. Polls de revisão igual dispensam empacotamento do KS/SEQ e CRC. Os formatos do patch e do armazenamento USER permanecem compatíveis.

O comando 16 com payload `[1]` acrescenta Range X/Y em Q12 (0–4096) e posições brutas X/Y com centro em 8192. Os índices 14–15 continuam informando o valor efetivo após o range. Os índices 16–17 são graváveis na sessão. O comando 11 com `[11, eixo, posiçãoLo, posiçãoHi]` ajusta a posição bruta. Eixos 0 e 1 correspondem a X e Y.

O comando 18 lê o perfil de desempenho. Use payload `[1]` para reiniciar a janela de medição antes da leitura. Retorna oito valores de cinco septetos, nesta ordem.

1. Blocos preenchidos.
2. Maior tempo de preparação de um bloco em µs.
3. Blocos que alcançaram o prazo nominal de 2,901 ms.
4. Menor reserva de PCM observada enquanto o sintetizador tocava, ou `0xffffffff` enquanto não houver amostra.
5. Novos MISS.
6. Novos DMA.
7. Novos USB DROP.
8. Maior tempo de processamento de um comando Studio em µs, sem contar a própria leitura do perfil.

## Revisão esparsa da 3.9.5

HELLO anuncia `98 12`. O comando 19 recebe a última revisão aceita em três septetos. Zero pede uma referência completa. A resposta começa com um byte de tipo e três de revisão. O tipo 0 termina aí. Os tipos 1 e 2 acrescentam comprimento (2), CRC-16 (3) e o payload se ele couber em até 87 bytes. Para payloads maiores, consulte o mesmo comando 19 com offset em dois septetos e leia em partes de até 96 bytes.

- Tipo 2 contém painel de 42 bytes, base do patch de 475 bytes, extensão de 839 bytes e os eventos KS ao final.
- Tipo 1 começa pelo comprimento da nova frase KS em dois septetos. `0x3fff` indica que a frase anterior continua válida. Seguem intervalos com offset de dois bytes, contagem de um byte e apenas os bytes alterados. A nova frase ocupa o final da mensagem.
- O campo 20 do painel indica o perfil DSP. Use comando 16 com `[20, perfil, 0]`. `0` indica Legado e `1`, AKS.
- O comando 20 recebe um grupo seguido de triplas `[índice, valorLo, valorHi]`. Os grupos 0, 1 e 2 correspondem aos controles KS, ARP/SCALE e cabeçalho SEQ. O firmware valida o grupo antes de aplicar as alterações.
- O comando 11, subcomando 7, aceita três septetos opcionais com o CRC da última telemetria. A resposta é vazia enquanto os dados não mudarem.

As gravações KS e SEQ são encaminhadas quando terminam. Os offsets dos campos fixos independem da quantidade de eventos KS. Os formatos de patch, grade, slot USER e os comandos anteriores continuam aceitos.
