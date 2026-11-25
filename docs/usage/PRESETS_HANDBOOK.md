# Presets do ziviSynthi AKS · Handbook presets

[English](#english)

O catálogo tem **128 presets**. A primeira coluna abaixo segue os bancos físicos do SMK. A última usa o número MIDI de **Program Change** em Bank Select 0, começando pela posição 0.

| Banco | Grupo | Presets | Program Change |
|---:|---|---:|---:|
| 1 | Estúdio e laboratório | 14 | 0–13 |
| 2 | Performance | 16 | 14–29 |
| 3 | Texturas | 16 | 30–45 |
| 4 | Sequências KS | 16 | 46–61 |
| 5 | Instrumento | 11 | 62–72 |
| 6 | Handbook I | 16 | 73–88 |
| 7 | Handbook II | 16 | 89–104 |
| 8 | Teclas e baixos | 16 | 105–120 |
| 9 | Exploração | 7 | 121–127 |

No **Overview**, gire **K4** para carregar um preset. O rodapé exibe o nome e a posição de 1 a 128. A rotação é circular. Em **Patch**, use **<** e **>** para mudar de banco e os pads para carregar. **K5** move o cursor e **PARA** carrega a seleção. A navegação dos novos bancos vale também para MIDI Bank Select CC0 de 6 a 9 seguido do Program Change local, começando em 0.

O banco adicional **USB FX** tem quatro patches de processamento. Use **PATCH** no teclado ou MIDI Bank Select CC0 = 10 com Program Change 0–3. O K4 e os endereços 0–127 continuam dedicados aos timbres.

No editor web, selecione o grupo na lista. Cada preset contém uma instrução de execução no painel. Ajuste **F1** para ganho do sintetizador, **F3** para corte e **F4** para mistura da mola no teclado físico. Os estudos de ruído, panning e envoltória cíclica podem soar imediatamente ao serem carregados. As frases KS começam em pausa. Para ouvi-las, abra **SEQ → REPEAT → PLAY**. Nos estudos de acompanhamento, toque o teclado junto com o KS.

## Experimentos

| Experimento do Handbook | Páginas impressas | Realização digital |
|---|---:|---|
| 1 · Intensidade | 14 | Nível de seno comparado nas saídas e no monitor B. |
| 2 · Envoltórias | 14–16 | Oscilador 1 via D3, saída ENV SIGNAL e contorno separado em B11. |
| 3 · Percepção de altura | 17 | Seno com base ajustável para comparar alturas. |
| 4 · Timbre | 18–19 | Comparação de seno e rampa com Shape. |
| 5 · Análise de frequência | 21 | Rampa à esquerda e pulso à direita para comparar os espectros. |
| 6 · Ruído | 23–24 | Duas referências comparáveis no mesmo gerador, uma aberta e outra escura. |
| 7 · Ruído filtrado | 23–24 | H7 e saídas do filtro. Cutoff e Response isolam a banda. |
| 8 · Trapezoide | 27–28 | Três patches para altura, corte e ganho em P. O CV de nível do firmware usa outro sinal de controle que o ajuste analógico invertido. |
| 9 · Controle por tensão | 29–33 | Sete exemplos de modulação. Inclui vibrato por VCO 2, altura com ruído, ataque por joystick, decay variável, mola modulada, filtro duplo e movimento entre saídas. No último exemplo, um LFO controla o canal esquerdo para aproximar o movimento original. |
| 10 · Carrilhões | 35 | Dois osciladores afinados pelo teclado alimentam anel, filtro e envoltória. A mola prolonga as caudas. |
| 11 · Reverberação | 41 | Canal seco e retorno da mola ajustável. |
| 12 · Teclado e sequenciador | 48–50 | Dois presets. Um explora dinâmica do teclado em O/P. O outro usa o KS, linha I16 em modo EMS e frase autoral. |
| 13 · Sequência e teclado | 52 | Voz KS à esquerda e acompanhamento com afinação ao vivo e dinâmica P9 à direita. |
| 14 · Teclado como controle | 53 | Duas variações do anel. A primeira usa o CV da entrada F. A segunda combina essa entrada, o ruído e uma portadora de apoio. |
| 15 · Eco de fita | 56 | Ensaio de retorno com a mola realimentada. O tempo e a resposta diferem de um gravador de duas cabeças. |
| 16 · Entradas | 57–59 | Estudos de filtro, limiar em C e anel. Geradores internos fornecem fontes para tocar diretamente no SMK. |
| 17 · Prestopatch Battle | 65 | Ruído filtrado, envoltória, mola e joystick para textura e mix. |
| 18 · Prestopatch Keyboard | 65–67 | Sequência no Oscilador 2, percussão de anel e voz independente à direita. Inclui frase autoral e modo EMS. |
| 19 · Prestopatch Guitar | 69–70 | O Oscilador 1 ocupa a entrada usada pela guitarra. O patch estuda filtragem, nova envoltória, mola e realimentação do anel. |

Os presets de Handbook preservam os caminhos documentados onde o controle correspondente existe. Os valores de frequência, segundos, níveis e panoramas foram ajustados ao DSP. Os 23 timbres de **Teclas e baixos** e **Exploração** são autorais. Incluem marimba, celesta, metais, síntese por modulação cruzada, percussão, drones com filtro ressonante e sequências KS com pausas.

Referência de circuitos, procedimentos e nomes dos experimentos. Grogono, Peter. *The Synthi Educational Handbook*. Electronic Music Studios of America, abril de 1972, capítulos II–VI. [Digitalização no arquivo Vasulka](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf)

## English

[Português](#presets-do-zivisynthi-aks--handbook-presets)

The nine physical banks contain **128 instrument presets**. The table above lists the exact counts and zero-based Bank Select 0 Program Change ranges. Groups 1–9 are Studio/Lab, Performance, Textures, KS Sequences, Instrument, Handbook I, Handbook II, Keys/Basses and Exploration. An additional **USB FX** bank uses CC0 = 10 and Program Change 0–3. K4 in **Overview** cycles instrument presets. In **PATCH**, use **< / >** for banks and pads to load, or K5 to select and PARA to load. Banks 6–9 also accept local Program Change on Bank Select 6–9.

The Studio shows playing instructions by preset. On the Pro, F1 controls global synth gain, F3 cutoff and F4 reverb mix. Noise, panning and cycling envelope patches can sound immediately. KS phrases load paused. Play them with **SEQ → REPEAT → PLAY**.

The Handbook studies cover experiments 1–19 in the preceding table. Printed page numbers refer to Grogono’s 1972 handbook. [Handbook scan](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf)

| Experiments | Digital realization |
|---|---|
| 1–5 | Sine level on both outputs and meter B, envelope in D3 with B11 contour, pitch comparison, sine/ramp shape and left ramp/right pulse spectra. |
| 6–8 | Open and dark noise, filtered noise through H7, and trapezoid control of pitch, cutoff and output P. |
| 9–11 | VCO 2 vibrato, noise pitch, joystick attack, variable decay, modulated reverb, dual filters and output motion. Ring modulation with reverb produces chimes; another study isolates dry and spring paths. |
| 12–14 | Separate keyboard dynamics and KS examples, including EMS routing I16 with an original phrase. Another study sends KS left and keyboard right with P9 dynamics. Two ring studies use the F input. |
| 15–16 | Spring feedback as a tape echo study and filter, C threshold and ring routing with internal sources. |
| 17–19 | Prestopatch Battle, Keyboard and Guitar adapted using filtered noise, ring feedback, spring reverb, joystick and independent voices where applicable. Guitar input is modeled using Oscillator 1. |

The 23 patches in Keys/Basses and Exploration are original works, including marimba, celesta, brass, cross-modulation, percussion, resonant filter drones and KS phrases with rests. DSP values for frequency, levels, pan and time are adapted to this instrument.
