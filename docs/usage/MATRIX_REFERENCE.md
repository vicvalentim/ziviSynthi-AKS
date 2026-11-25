# Matriz do ziviSynthi AKS · Pin matrix

[English](#english)

Cada ponto combina uma letra de coluna e um número de linha. O mapa foi conferido com a folha de patch e com os exemplos de roteamento do AKS. [Synthi Educational Handbook](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf) · [AKS Patch Sheet](https://medias.audiofanzine.com/files/ems-synthi-aks-service-manual-481665.pdf)

| Linha | Fonte | Coluna | Destino |
|---|---|---|---|
| 1 | Retorno da saída 1 | A | Entrada da saída 1 |
| 2 | Retorno da saída 2 | B | Monitor |
| 3 | Oscilador 1, seno e rampa somados | C | Entrada da saída 2 e sinal para o disparo por limiar |
| 4 | Oscilador 2, retangular e triangular somadas | D | Entrada de sinal da envoltória |
| 5 | Oscilador 3 retangular | E | Modulador em anel A |
| 6 | Oscilador 3 triangular | F | Modulador em anel B |
| 7 | Ruído | G | Entrada do reverb |
| 8 | Entrada 1, afinação do teclado por padrão | H | Entrada de sinal do filtro |
| 9 | Entrada 2, velocidade MIDI por padrão | I | Frequência do Oscilador 1 |
| 10 | Saída do filtro | J | Frequência do Oscilador 2 |
| 11 | Controle de envoltória trapezoidal | K | Frequência do Oscilador 3 |
| 12 | Sinal processado pela envoltória | L | Decaimento da envoltória |
| 13 | Saída do modulador em anel | M | Mistura do reverb |
| 14 | Saída do reverb | N | Frequência de corte do filtro |
| 15 | Joystick X digital | O | Nível da saída 1 |
| 16 | Stick Y digital ou afinação do KS no modo EMS | P | Nível da saída 2 |

Os 256 cruzamentos das fontes e destinos ativos aceitam pinos de 100% e 50%. O monitor B mede o ponto selecionado. No modo de disparo por limiar, a envoltória usa a magnitude do sinal em C, antes do ganho da saída 2.

## Roteamento e realimentação

O motor avalia a matriz por amostra nesta ordem:

1. Entradas, Stick X/Y, afinação do KS e ruído.
2. Cálculo da envoltória, incluindo a coluna L e a linha 11.
3. Osciladores.
4. Filtro.
5. Sinal processado pela envoltória, modulação em anel e reverberação.
6. Saídas.
7. Monitor B.

A matriz lê fontes de um estágio anterior na amostra atual. Para fontes do mesmo estágio ou de um estágio posterior, usa a amostra anterior e multiplica o ganho do pino por 0,72. G12 e G14, por exemplo, têm um quadro de atraso. O disparo por limiar usa C na amostra anterior.

A soma dos pinos é limitada a ±1 nos barramentos de áudio e a ±1,5 nos barramentos de frequência. Os ganhos dos pinos e dos retornos são escolhas digitais. A calibração com a carga resistiva do AKS requer uma unidade física de revisão identificada.

| Patch | Pinos centrais |
|---|---|
| Inicial | I8, D3, A12, C12 |
| Rampa filtrada | I8, H3, D10, A12, C12, N6 a 50%, B12 |
| Modulação em anel | I8, E3, F6, D13, A12, C12, B12 |
| KS · Sequência e teclado | D3, A12, J8, C4, P9 e KS interno para I. No modo EMS, use I16. |
| Envelope e molas | I8, D3, A12 a 50%, G12, C14 |
| Filtro ressonante | H7 a 50%, N6 a 50%, D10, A12, C12 |
| Trapezoide cíclico | H3, D10, A12, N11 a 50%, G12, C14 a 50%, I8 |

| Controle | Fonte na matriz | Faixa |
|---|---|---|
| PITCH | Stick X, linha 15 | −1 a +1, centro na posição de repouso |
| MOD | Stick Y, linha 16 | −1 a +1, centro no MIDI 64 |
| KS interno | Conexões dedicadas para as colunas A–P | Desligado, 50% ou 100% por destino |
| KS no modo EMS | Afinação do KS na linha 16 | Spread e transposição do sequenciador |

Os CC16 e CC17 externos também controlam X e Y, respectivamente, com centro em 64. No modo controlador físico, PITCH e MOD transmitem Pitch Bend de 14 bits e CC1. No modo EMS, a linha 16 comuta para o KS, e as conexões internas ficam suspensas. Retirar todos os pinos mantém os ajustes do KS interno.

O sinal em A vai para Out 1 com panorama de fábrica 100% L. O sinal em C vai para Out 2 com panorama de fábrica 100% R. Nos timbres secos, o mesmo sinal pode alimentar A e C. Nos timbres com molas, A costuma receber o sinal seco em A12, enquanto G12 envia ENV SIGNAL ao reverb e C14 leva o retorno para C. Alguns patches somam C12 a 50% para sustentar a presença no canal direito.

## English

[Português](#matriz-do-zivisynthi-aks--pin-matrix)

Each address combines a column letter with a row number. The map follows the EMS patch sheet and routing examples. [Service manual and patch sheet](https://medias.audiofanzine.com/files/ems-synthi-aks-service-manual-481665.pdf)

| Row | Source | Column | Destination |
|---|---|---|---|
| 1 | Output 1 return | A | Output 1 input |
| 2 | Output 2 return | B | Meter |
| 3 | Oscillator 1 sine and ramp | C | Output 2 input and threshold trigger |
| 4 | Oscillator 2 pulse and triangle | D | Envelope signal input |
| 5 | Oscillator 3 pulse | E | Ring modulator A |
| 6 | Oscillator 3 triangle | F | Ring modulator B |
| 7 | Noise | G | Reverb input |
| 8 | Input 1, keyboard pitch by default | H | Filter input |
| 9 | Input 2, MIDI velocity by default | I | Oscillator 1 frequency |
| 10 | Filter output | J | Oscillator 2 frequency |
| 11 | Trapezoid control | K | Oscillator 3 frequency |
| 12 | Envelope-processed signal | L | Envelope decay |
| 13 | Ring modulator output | M | Reverb mix |
| 14 | Reverb output | N | Filter cutoff |
| 15 | Stick X | O | Output 1 level |
| 16 | Stick Y or KS pitch in EMS mode | P | Output 2 level |

White and red indicate 100% digital gain, green 50%. The Studio preserves red as visual metadata. Column B is the meter bus. A threshold trigger reads the magnitude at C before Output 2 gain.

Each sample evaluates inputs and noise, then envelope, oscillators, filter, envelope signal/ring/reverb, outputs and B. Same-stage and later-stage sources use the prior sample with gain multiplied by 0.72, including G12 and G14. The C threshold reads the previous sample. Audio buses clamp at ±1 and frequency buses at ±1.5. These are digital routing values. The analog load of the pins depends on the original circuit.

Start with I8, D3, A12 and C12 for a played patch. For filtered ramp, use H3 and D10 instead of D3, with N6 at 50% and B12 for metering. Ring modulation can use E3, F6 and D13. For a spring return on the right, use A12 dry, G12 to feed the reverb and C14 for the return. OUT 1 and OUT 2 default to hard left and hard right.

PITCH and MOD map to the bipolar Stick X/Y. MIDI CC16/17 also address X/Y and center at 64. In controller mode, the levers emit 14-bit Pitch Bend and CC1. Internal KS routing adds independent off/50%/100% paths to A–P. EMS mode assigns row 16 to KS pitch and suspends internal routes. Clearing pins preserves internal KS settings.
