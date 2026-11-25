# Auditoria do circuito EMS Synthi AKS · Felucca Pro 3.9.5

[English](#english)

## Escopo e referência

A auditoria cobre as três placas de síntese, a matriz de pinos, o painel de controle e a unidade KS. A numeração abaixo é a página do visualizador do manual de serviço. A documentação reúne variantes do Synthi A e do VCS3, além dos teclados e do KS. O AKS tem revisões de placa. Uma comparação de componentes ou de valores elétricos precisa da revisão e do número de série da unidade medida.

- Electronic Music Studios. *Service Manual. Synthi A/VCS3, DK1/2 Keyboard, KS Sequencer*. [Manual de serviço, páginas 4–29 e seção KS a partir da página 34](https://www.manualslib.com/manual/4172789/Ems-Synthi-A.html).
- Grogono, Peter. *The Synthi Educational Handbook*. Electronic Music Studios of America, abril de 1972. [Capítulos III e IV, experimentos 8–14](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf).
- Electronic Music Studios (London). *Synthi Users Manual*. [Seções 3 e 4](https://whitefiles.org/rwz/zxe/1970_ems_synthi_users_manual.pdf).
- EMS. [Folha de patch Synthi AKS](https://medias.audiofanzine.com/files/ems-synthi-aks-service-manual-481665.pdf).
- Graham Hinton. [Distribuição das placas e revisões](https://hinton-instruments.co.uk/ems/essential.html) e [desenho de manutenção do KS](https://www.hinton-instruments.co.uk/ems/emsmisc.html).

## Placas de síntese e matriz

| Bloco EMS | Correspondência no ziviSynthiA | Diferença verificável e próxima medição |
|---|---|---|
| A · alimentação e referência, pp. 7–9 | A BSP inicializa SMK, codec e periféricos. O DSP opera com sinais normalizados. | O circuito analógico de alimentação é específico do AKS. Medir a unidade original antes de modelar ruído de alimentação ou desvio térmico. |
| A · amplificadores de saída, pp. 10–11 | A/C recebem o áudio, O/P recebem modulação. Cada canal tem ganho, tom, retorno à matriz e pan. Master e limiters pertencem ao SMK. | O tom usa um filtro digital e os ganhos são normalizados. Medir curvas por canal, diafonia e ganho por posição. |
| A · mola, pp. 12–13 | G envia o áudio, M modula a mistura e a linha 14 retorna a soma direta/reverberada. O DSP usa dois atrasos dispersivos. | É uma aproximação do tanque. Medir resposta impulsiva, decaimento espectral e curva de mistura para a revisão física. |
| B · Envelope Shaper, pp. 14–15 | D recebe o áudio, L modula o decaimento, 11 gera Trapezoid, 12 fornece ENV SIGNAL. Há disparos manual, de teclado/KS e por limiar em C. | Os modos e curvas são digitais. Medir Attack/On/Decay/Off, contorno com carga e limiar em C. |
| B · Filter/Oscillator, pp. 16–17 | H recebe áudio, N controla corte e a linha 10 devolve o sinal. A cascata tem três polos, realimentação não linear e auto-oscilação. | Comparar inclinação e frequência de auto-oscilação em diferentes revisões. O ganho de entrada e a resposta operam em escala digital. |
| B · Ring Modulator, pp. 18–19 | E e F recebem os sinais. A linha 13 entrega o produto multiplicado pelo nível de saída. | A multiplicação preserva bandas laterais e duplicação com sinais iguais. Vazamento de portadora e ajuste de balanceamento precisam de medição. |
| B · entradas, pp. 20–21 | Linhas 8 e 9 recebem entradas externas quando fornecidas pelo hospedeiro. Sem sinal externo, recebem afinação ao vivo e velocidade do KS/teclado. | A disponibilidade de sinal externo depende do alvo. Cargas, sensibilidades de microfone e acoplamento DC pertencem ao equipamento analógico da revisão medida. |
| C · VCO 1, pp. 22–23 | Linha 3 reúne seno deformável e rampa antialias. I controla frequência. | Medir a relação entre Shape, espectro, fase e ganho da rampa do VCO montado. |
| C · VCO 2, pp. 24–25 | Linha 4 reúne pulso e onda inclinada modelada como triangular de simetria variável. J controla frequência. | Comparar com a forma rotulada como *ramp* na documentação de alinhamento. A triangular de simetria variável aproxima o comportamento sem reproduzir a etapa elétrica. |
| C · VCO 3, pp. 26–27 | Pulso na linha 5, triangular na 6, K controla frequência. Opera em registros baixos e de áudio. | Medir tracking e limites efetivos de frequência da revisão. |
| C · ruído e medidor, pp. 28–29 | Ruído de banda larga pseudoaleatório com ajuste de cor na linha 7. B mede o pico do barramento. | Comparar densidade espectral e distribuição do ruído. A resposta do medidor analógico e seus modos de leitura AC e DC exigem tratamento próprio para equivalência. |
| Patchboard de 16 × 16 | Os 256 endereços aceitam conexões de 100% e 50%. A soma é limitada. Rotas de retorno e conexões que ainda não foram avaliadas usam uma amostra de atraso. | Pinos vêm de uma rede resistiva e a carga depende do circuito. Os ganhos do DSP devem ser lidos como intensidades do patch. Medir carregamento e caminhos de realimentação antes de calibrar por ohms. |
| Joystick e KS na linha 16 | PITCH e MOD controlam X/Y em 15/16. O modo EMS coloca KS em 16 e suspende o roteamento interno. | As duas alavancas são independentes. A curva de tensão do joystick original e os potenciômetros Range precisam de medição. |

## Unidade KS

| Estágio no AKS | Implementação no SMK |
|---|---|
| Endereçamento e toque | O circuito original varre contatos e retém uma nota. O SMK recebe notas e velocidade do teclado ou MIDI. A voz é monofônica com prioridade da última nota. |
| DAC ao vivo e DAC do sequenciador | Cálculo de afinação e spread independentes. Em modo EMS, I8 aciona o oscilador ao vivo e I16 usa o KS. |
| Dinâmica e trigger | Entrada 2 substitui a função de dinâmica pela velocidade. Trigger escolhe teclado ou sequenciador. A linha 11 permanece independente de ENV SIGNAL. |
| Registro, memória, transporte e relógio | A gravação mantém ataque, soltura, pausas e legato em até 256 estados. O relógio de áudio marca a posição. MIDI Clock de 24 pulsos por semínima é a integração moderna para a DAW. |
| Random, transposição e difusão da afinação | Random e a transposição estão no KS digital. O range da transposição é de ±24 semitons, sem a dependência do circuito de contatos e chaves do AKS. |
| Operação simultânea de teclado e KS | O patch KS usa destinos separados para as duas afinações e pode usar A/C como pares de saída independentes. |

A regressão do motor contém verificação de estéreo, matriz, realimentação, filtro, mola, envelope e registro KS. A revisão não inclui equivalência eletrônica componente a componente, medições do tanque nem captura de uma unidade AKS. Esses resultados dependem do hardware e da revisão identificada.

## Operação com DAW

1. Abra **SCALE → > → MIDI / AUDIO** no SMK. Gire **K5** à direita para **EXT**. Gire à esquerda para **INT**.
2. Na DAW, habilite o envio de **MIDI Clock** e transporte para a saída MIDI USB do **ziviSynthiA Pro**. O painel mostra a fonte detectada, BPM e estado de transporte.
3. Para dirigir o padrão STEP, abra **SEQ** antes do Start. Para dirigir uma frase temporal, abra **KS**. O ARP acompanha o BPM em EXT. O Start e o Continue aguardam o próximo pulso. O Song Position Pointer posiciona KS/STEP em unidades de seis pulsos. Depois de 500 ms sem tráfego da fonte, o programa libera o lock e restaura os BPM internos.
4. A referência do KS é **120 BPM**. Ajuste **KBANK + K5** na página MIDI / AUDIO para frases gravadas em outro andamento. Para *On the Run*, selecione **165 BPM**. Esse ajuste vale para a sessão.
5. O envio de clock pelo SMK usa **INT** e **K7 CLK ON** quando KS ou STEP toca. Em **EXT**, os pulsos da DAW não são reenviados.

O alinhamento inicial parte do próximo F8 e o andamento é estimado pelos timestamps. O firmware mantém a fase musical durante ajustes de andamento. A latência absoluta inclui a fila de áudio e o timestamp do transporte. A validação física deve medir offset e jitter na DAW.

[MIDI Association, mensagens de tempo e transporte](https://midi.org/summary-of-midi-1-0-messages)

## Conferência do catálogo na 3.9.4

Os bancos do teclado e o painel web usam o mesmo catálogo `platform/module_panel.def`. A codificação USB foi verificada para os 41 controles. A tabela identifica a atribuição física e as lacunas de calibração por módulo.

| Módulo | Knobs no SMK | Conferência técnica |
|---|---|---|
| Oscillator 1 | K1 Frequency, K2 Shape, K3 Sine, K4 Ramp, K5 Tune, K6 Blend | Frequência nas posições do manual. K5/K6 acrescentam afinação e mistura digitais. Falta medir forma e amplitude no original. |
| Oscillator 2 | K1 Frequency, K2 Shape, K3 Pulse, K4 Triangle | Vernier nas posições do manual. A forma inclinada digital tem simetria variável. Falta comparar o ciclo e a amplitude com o circuito da revisão física. |
| Oscillator 3 | K1 Frequency, K2 Shape, K3 Pulse, K4 Triangle | Vernier em 63 Hz na posição 8 e 500 Hz na 10. Os meios de curso de Shape geram pulso simétrico e triangular simétrica no modelo. Falta medir pontos extremos e sensibilidade sob CV. |
| Noise Generator | K1 Colour, K2 Level | Filtro de cor e ganho operam em unidades normalizadas. Uma captura do original é necessária para calibrar a curva espectral por posição. |
| Ring Modulator | K1 Level | Multiplicação bipolar, nível normalizado. Falta medir vazamento de portadora e resposta em níveis baixos no circuito analógico. |
| Filter / Oscillator | K1 Frequency, K2 Response, K3 Drive, K4 Level | Posição 5 calibrada com o teste de auto-oscilação. O ponto 5 de Response indica o limiar teórico do modelo de três polos. Drive é digital. Faltam medir ganho do circuito e inclinação sob modulação. |
| Envelope Shaper | K1 Attack, K2 On, K3 Decay, K4 Off, K5 Signal, K6 Trapezoid, K7 Mode, K8 Trigger | Tempos parametrizados e saídas separadas. Mode/Trigger selecionam comportamento digital. Faltam medir transientes, limiar e trajetória de repetição do AKS. |
| Reverberation | K1 Mix, K2 Level, K3 Time | K3 estende o tempo até 4 s. O original especifica até 2 s. Faltam resposta impulsiva e curva de mistura elétrica. |
| Input Amplifiers | K1 Input 1, K2 Input 2 | Ganhos normalizados nos canais digitais. Sensibilidade em volts e diferença entre linha e microfone dependem do circuito original. |
| Output Channels e Output Filter | K1 Level 1, K2 Tone 1, K3 Pan 1, K4 Level 2, K5 Tone 2, K6 Pan 2, K7 Patch | Stereo L/R validado em host. K7 é um nível digital de patch. Faltam medir lei do pan, tom e tracking de ganho dos canais originais. |

Range X/Y foram verificados separadamente. São os ajustes de alcance dos sinais das linhas 15 e 16 e podem ser ajustados no painel físico e no Web Studio. A coluna B conserva modos de sinal e tensão de controle no editor. A balística do medidor e as tensões originais ainda precisam de comparação de bancada.

A calibração de 261 Hz foi aferida no DSP compilado para o simulador Felucca e no modelo Web. Esse teste estabelece o resultado do modelo. A equivalência das curvas analógicas depende de uma unidade EMS com revisão identificada.

## Calibração 3.9.5

O perfil AKS é o padrão no SMK e no editor. O perfil Legado 3.9.4 conserva as curvas DSP anteriores para abrir patches daquela versão. O valor de referência de 1,28 V é uma convenção do modelo normalizado que mantém o rastreamento de quatro oitavas por unidade para OSC 1 e OSC 2.

| Ponto de calibração | Implementação | Limite de equivalência |
|---|---|---|
| Destinos de CV | OSC 1/2 com 0,32 V/oitava, OSC 3 com 0,26, filtro com 0,20, Decay com 0,40. Stick com ±1,5 V nominais antes do Range. | A matriz continua usando ganhos digitais. A carga dos pinos e a deriva dos módulos precisam ser aferidas com o circuito original. |
| Vernier | OSC 1 e 2 chegam a 10 kHz no DSP à taxa nominal do codec. | Forma de onda, amplitudes em volts e tracking da unidade física requerem captura. |
| Envelope | Em modo cíclico, Off no máximo impede a repetição. Em modo temporizado, On manual dura duas vezes o On automático, até 5 s. O teste simula um novo disparo com Off no máximo. | A comutação do circuito, o tempo de subida e o vazamento residual precisam ser medidos. |
| Mola | Região seca até 2 no Mix, faixa nominal de CV de −2 V a +2 V e tempo interno limitado a 2 s. | A resposta dispersiva digital não caracteriza o tanque físico ou o ganho dos amplificadores. |
| O/P Level | CV positivo abre a saída mesmo com o Level em zero. CV negativo reduz o ganho. O DSP soma a modulação ao ganho nominal e limita o resultado à faixa digital. | A lei em dB/V e o tom dos canais dependem de medição analógica. |
| Banco de presets | OUT 1 com pan −100% e OUT 2 com +100%. O ganho foi ajustado de acordo com o RMS em estéreo, mantendo ENV SIGNAL, REVERB, pinos e sequências. | A medição usa o motor C e o perfil AKS. A saída P2, a sensibilidade física dos canais e a audição em Bluetooth não foram medidas neste pacote. |

[Manual de serviço, teste da envoltória](https://medias.audiofanzine.com/files/ems-synthi-aks-service-manual-481665.pdf#page=48) · [The Synthi Educational Handbook, capítulo III](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf)

A aferição de OUT 1 e OUT 2 usou renderizações de 6 s a 48 kHz com notas de referência 48, 60 e 72 para os 128 presets de instrumento. Os quatro efeitos USB receberam o mesmo seno de 220 Hz nos dois canais. As colunas antes e depois comparam os ganhos de fábrica anteriores e os revisados no mesmo motor AKS. Os 49 presets com correções mantêm suas rotas. Em dois estudos com voz à direita acionada por CV, o ganho de Input 2 foi ajustado para preservar o silêncio sem nota. Em *Névoa estéreo*, o balanço muda com a nota e permanece dentro de 2,20 dB nos três registros testados. Nos demais presets, a diferença ficou abaixo de 1,50 dB nos sinais de teste. A aferição registra níveis em dBFS e diferenças entre canais por renderização.

## English

[Português](#auditoria-do-circuito-ems-synthi-aks--felucca-pro-395)

The audit covers the three synthesis boards, matrix, control panel and KS. Component-level comparison needs a specific board revision and serial number. Numbers in the Portuguese tables are viewer pages in the service manual. [EMS Service Manual](https://www.manualslib.com/manual/4172789/Ems-Synthi-A.html) · [Synthi Users Manual](https://whitefiles.org/rwz/zxe/1970_ems_synthi_users_manual.pdf) · [The Synthi Educational Handbook](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf)

| EMS section | Digital implementation | Measurement still needed |
|---|---|---|
| Board A, power/reference | BSP initializes board and codec; the DSP uses normalized signals. | Power-related noise and thermal drift. |
| Board A, outputs | A/C carry audio, O/P modulate channel levels, with tone, return and pan per channel. The Pro has separate master and limiters. | Gain curves, tone and crosstalk. |
| Board A, spring | G input, M mix CV, row 14 return. Two dispersive delay stages approximate the tank. | Impulse response and decay/mix curves. |
| Board B, envelope | D input, L decay CV, row 11 trapezoid and row 12 processed signal. Manual, keyboard/KS and C threshold triggers. | Attack, On, Decay, Off, load and threshold. |
| Board B, filter | H input, N frequency CV, row 10 output. Three-pole digital cascade with nonlinear feedback and self-oscillation. | Slope and oscillation across circuit revisions. |
| Board B, ring and inputs | E/F multiply to row 13. Rows 8/9 receive host input or supply pitch/velocity. | Carrier leakage, balance, analog input sensitivity and DC coupling. |
| Board C, VCOs | I/J/K tune the oscillators. Row 3 sums sine/ramp, row 4 pulse/variable-symmetry triangle, rows 5/6 separate OSC 3 pulse/triangle. | Waveform, amplitude, tracking and phase of the identified board. |
| Board C, noise and meter | Row 7 supplies color-adjustable noise. Column B drives the meter. | Noise spectrum and meter ballistics in signal and CV modes. |
| Matrix and joystick | Full and half gain, digital bus limits and one-sample delayed feedback. PITCH/MOD drive X/Y at 15/16. | Resistive loading, original joystick/range curves and feedback gain. |

The shared control catalog in `platform/module_panel.def` has USB encoding tests for all 41 controls. OSC verniers use service reference positions. Filter frequency position 5 was verified at about 261 Hz in self-oscillation in the simulator and browser. Tune, Blend, Drive, Mode, Trigger and Patch level are digital additions. See the [bench manual](../usage/SMK37_FELUCCA_BANCADA.en.md#global--modules) for encoder assignments.

### KS and DAW clock

The digital KS keeps one effective voice, uses last-note priority and stores attack, release, rests and legato in up to 256 events. Live and recorded pitch use separate spreads. I8 routes live pitch and I16 routes KS in EMS mode. Trigger selects keys or sequence. Random and ±24-semitone transposition are digital controls. A patch can send the two voices independently to A and C.

Select **SCALE → > → MIDI / AUDIO → K5 EXT** and send USB MIDI Clock and transport from the DAW. Enter SEQ or KS before Start. ARP follows external BPM. Start and Continue align to the next pulse. Song Position Pointer uses six-pulse units. After 500 ms without traffic, the firmware releases clock lock and returns to internal BPM. KS defaults to a 120 BPM reference. Adjust with **KBANK + K5** and use 165 BPM for *On the Run*. In INT, **K7 CLK ON** sends clock during KS or STEP playback. Absolute offset and jitter still need physical measurement.

### AKS profile in 3.9.5

| Calibration | DSP behavior |
|---|---|
| CV | 0.32 V/octave for OSC 1/2, 0.26 for OSC 3, 0.20 for filter and 0.40 for Decay. Nominal Stick ±1.5 V before Range. 1.28 V is the model normalization reference. |
| Verniers | OSC 1/2 reach 10 kHz at the nominal codec rate. |
| Envelope | Maximum Off disables cycling. In timed mode, manual On lasts twice the automatic On, up to 5 s. |
| Spring | Dry region through 2 on Mix, nominal CV ±2 V and internal time capped at 2 s. |
| Output level | Positive CV can open a channel with Level at zero. Negative CV reduces gain. |
| Preset balance | OUT 1 hard left, OUT 2 hard right. Adjusted gains in 49 factory patches while preserving ENV SIGNAL, REVERB, pins and sequences. |

The AKS profile is the default. Legacy 3.9.4 restores the earlier curves for comparison. Stereo checks used six-second 48 kHz renders of all 128 instrument presets at MIDI notes 48, 60 and 72, and a 220 Hz sine in both channels for the four USB effects. Under those signals, the *Névoa estéreo* patch was within 2.20 dB, and the other patches within 1.50 dB. Those readings describe DSP output. Physical P2, channel sensitivity and Bluetooth still require bench measurements.
