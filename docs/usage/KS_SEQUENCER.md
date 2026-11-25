# Motor KS · KS engine

[English](#english)

O capítulo IV do *Synthi Educational Handbook* descreve o registro de notas no tempo, memória para 256 eventos, velocidade e altura independentes, os controles de spread em tempo real e de sequência, a seleção de trigger, Random e a saída de afinação do sequenciador na linha 16. O relógio do AKS histórico regula a duração máxima e a reprodução. O ziviSynthiA usa o relógio de áudio do hospedeiro para controlar posições e preservar pausas de execução. [Peter Grogono, The Synthi Educational Handbook, cap. IV](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf)

## Operação

| Controle | Comportamento |
|---|---|
| Record | Inicia uma gravação a partir da posição zero. A nota já mantida pode entrar no quadro inicial. Até a primeira nota, o tempo decorrido é silêncio. Escolher Record novamente pelo painel web inicia nova gravação. |
| Play | Finaliza uma gravação em curso e começa na posição zero. Ao reproduzir, volta ao início quando pressionado novamente no painel web. |
| Stop | Para o transporte, libera a saída de disparo do KS e guarda a frase em RAM. |
| Loop | Repete a frase com suas pausas e reinicializa o gate no ponto de retorno. |
| Sequence Length | Limite de 1 a 30 segundos para a próxima gravação. O fim efetivo é determinado pelo instante de Stop, Play ou pelo limite. |
| Velocidade | Avanço de 25% a 400% do tempo registrado, independente da afinação. |
| Realtime Pitch Spread | Escala os intervalos ao vivo em relação a A4, de 25% a 200%. |
| Sequence Pitch Spread | Escala os intervalos e a transposição do KS, de 25% a 200%. |
| Transpose | Desloca a sequência em ±24 semitons antes do spread. |
| Random | Seleciona uma das 30 notas virtuais a cada pressão e sustenta a nota até a liberação. |
| Trigger | Seleciona teclado ou sequência para a envoltória. Durante Record, a via do KS deixa passar o teclado para monitoração. |

Cada evento armazena quadro de áudio, nota, velocidade e indicador de ataque. A contagem de tempo começa na primeira nota. A gravação registra a voz monofônica efetiva com prioridade da última nota e retorno das notas mantidas. Note Off ocupa uma posição quando muda o estado. Eventos recebidos no mesmo quadro são consolidados pelo estado final. Os limites de tempo e memória encerram o registro sem exceder o buffer. O motor preserva a temporização no relógio do áudio, com resolução de entrada dependente de quando o hospedeiro entrega os eventos. O player lê na fronteira exata de cada quadro e usa centésimos de quadro para o avanço da velocidade.

A velocidade MIDI é uma aproximação digital da dinâmica histórica. A seleção de trigger controla a envoltória abreviada do motor, com um reataque suave de 2 ms quando o nível já está audível. A calibração atual da envoltória está descrita na [auditoria do circuito](../development/AUDITORIA_CIRCUITO_AKS.md#calibração-395).

## Matriz e execução independente

| Conexões no patch KS | Resultado |
|---|---|
| KS interno para I, D3 e A12 | O sequenciador afina o Oscilador 1. A envoltória selecionada por **Trigger = Sequência** controla o sinal que chega à saída esquerda. |
| J8, C4 e P9 | O teclado afina o Oscilador 2. A velocidade do teclado abre o nível da saída direita. |

No modo padrão, KS interno → I equivale à conexão de afinação usada em I16 no exemplo original. Os eixos PITCH e MOD continuam disponíveis nas linhas 15 e 16. A linha 8 leva afinação ao vivo e a 9, dinâmica ao vivo. O Oscilador 2 fornece uma voz independente.

No SMK, abra **SEQ → REPEAT**. Use **>** para alternar o modo EMS, **K5** para escolher a coluna do KS interno e **K7** para definir sua intensidade. No editor, use **KS → Roteamento de afinação KS**. No modo EMS, **I16** reproduz a conexão do Experimento 12, parte b. Use **D3** e **A12** para o sinal de Oscilador 1 pela envoltória, e use **C12** para ouvir o mesmo sinal no outro lado. Selecione o trigger **KS** para ouvir a frase. Para a parte a, a afinação em tempo real usa **I8** e trigger **KEYS**.

No simulador nativo, SEQ alterna entre a matriz e o KS. REC e PLAY alternam operação e parada, HOME para, SAVE carrega o patch KS, EDIT seleciona o trigger, LFO altera o loop e ARP emite Random enquanto mantido. K1/K2 comandam os spreads, K3 o limite, K4 a velocidade e SELECT a transposição. A gravação permanece em RAM e pode ser refeita com REC.

No Felucca Pro 3.2, SEQ abre STEP, e REPEAT dentro de STEP abre o KS. ARP tem seu próprio botão. Mantenha FX no KS para Random. A visualização mostra notas e cursor. A gravação conserva transições legato em presets JSON quando necessário.

## English

[Português](#motor-ks--ks-engine)

The model follows chapter IV of the *Synthi Educational Handbook*. It records note state against the audio clock, preserving timing and rests with up to 256 events. Realtime and recorded pitch spread are independent. [Peter Grogono, The Synthi Educational Handbook](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf)

| Control | Behavior |
|---|---|
| Record | Arm the recorder. A held note may start in the first frame. Recording positions are measured from the first note. |
| Play / Stop | Play finishes a take and starts at its beginning. Stop releases the KS gate and retains the phrase in RAM. |
| Loop | Repeat the phrase and clear the gate at the wrap point. |
| Sequence Length | Set the maximum for the next recording, 1–30 seconds. Stop, Play or the limit determines actual duration. |
| Speed | 25–400% of recorded time, independent of pitch. |
| Realtime / Sequence Pitch Spread | 25–200% around A4, separately for live and recorded pitch. |
| Transpose | ±24 semitones before applying sequence spread. |
| Random | Pick one of 30 virtual notes and hold until release. |
| Trigger | Select keyboard or KS for the envelope. Record passes live notes through for monitoring. |

Events store audio frame, note, velocity and attack. Last-note priority and held-note fallback capture monophonic legato. Same-frame input collapses to the final state. The player advances with fractional frame precision. MIDI velocity approximates original dynamics. [Current envelope calibration](../development/AUDITORIA_CIRCUITO_AKS.md#english)

On the Pro, use **SEQ → REPEAT** to open KS and hold **FX** for Random. Use **>** for EMS mode, **K5** for the internal route destination and **K7** for its strength. The Studio exposes these under **KS → Roteamento de afinação KS**. Internal KS to I tunes Oscillator 1 while keeping both joystick axes. In EMS mode, use I16 for that path instead, as in experiment 12b. D3 feeds the envelope, A12 reaches the left output and C12 duplicates it on the right. To play a separate live voice, use J8, C4 and P9. For experiment 12a, use I8 and keyboard trigger. [Bench controls](SMK37_FELUCCA_BANCADA.en.md#seq-ks-and-recording)
