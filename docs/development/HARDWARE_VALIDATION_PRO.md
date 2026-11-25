# Validação física · Hardware validation

[Português](#português) · [English](#english)

## Português

A gravação e a operação física da 3.9.5 aguardam ensaio no SMK‑37 Pro. Registre por sessão o modelo e a revisão, as versões de origem e destino, a alimentação e o cabo, o sistema operacional, a DAW e a cadeia de áudio.

1. Siga o [guia de instalação](../usage/INSTALL.md). Após o reinício, confirme a identidade USB. Verifique presets pelos pads e por Program Change, além da gravação e leitura de um slot USER.
2. Compare a saída P2 em L/R, a captura USB e a transmissão Bluetooth em rodadas distintas. Use o mesmo patch, nota e nível. Para USB, teste os dois sentidos e confira o roteamento de entrada.
3. Alterne as cenas, gire os encoders, edite pinos pelo painel e pelo Studio e confira a leitura após reconectar. Grave uma frase com pausas, edite o SEQ e confira o clock da DAW após iniciar, parar e reconectar.
4. Em **SCALE → MIDI / AUDIO**, pressione **PARA** para reiniciar a janela de medição. Alternativamente, use **Diagnóstico de desempenho → Iniciar medição** no Studio. Execute por 15 minutos com patch denso e repita nos três modos de LED. Anote o pico do DSP, a reserva PCM e os incrementos de MISS, DMA e USB DROP.

O prazo nominal é de **2,901 ms** por bloco. Registre os cenários executados e as evidências antes de declarar um resultado físico. Testes no hospedeiro medem o comportamento digital e não aferem latência do codec ou nível analógico do P2.

## English

Flashing and physical operation of 3.9.5 still need bench testing on the SMK‑37 Pro. For each session, record model and hardware revision, source and destination firmware, power and cable, OS, DAW and audio chain.

1. Follow the [installation guide](../usage/INSTALL.en.md). Confirm the USB identity after restart. Load patches from the pads and over Program Change, then save and reload a USER slot.
2. Compare P2 left/right, USB capture and Bluetooth transmission in separate runs. Use the same patch, note and level. Test audio in both USB directions and verify input routing.
3. Change scenes, turn encoders, edit pins from the device and the Studio, and check state after reconnecting. Record a phrase with rests, edit SEQ and verify DAW clock after starting, stopping and reconnecting.
4. In **SCALE → MIDI / AUDIO**, press **PARA** to reset the profiling window. Alternatively, start the measurement in the Studio’s performance diagnostics. Run a dense patch for 15 minutes and repeat for each LED mode. Record peak DSP time, PCM reserve and increments in MISS, DMA and USB DROP.

The nominal deadline is **2.901 ms** per block. Record the scenarios you ran and their evidence before marking them as verified. Host tests cover digital behavior. Codec latency and analog P2 level require physical measurement.
