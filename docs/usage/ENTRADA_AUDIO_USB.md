# Entrada de áudio USB · USB audio input

[English](#english)

O Synthi AKS da EMS recebe áudio analógico nas entradas 1 e 2. O manual descreve entradas de baixo nível para microfone e fontes de nível de linha. O áudio entra na matriz pelas linhas 8 e 9. [Synthi Users Manual](https://whitefiles.org/rwz/zxe/1970_ems_synthi_users_manual.pdf) · [The Synthi Educational Handbook, capítulos I e V](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf)

No SMK-37 Pro, a adaptação usa a reprodução USB estéreo de 44,1 kHz e 16 bits da base Felucca. O firmware entrega o áudio ao DSP no ritmo do codec, por meio da reamostragem do BSP. Cada canal alimenta rotas próprias da matriz.

| USB | Matriz | Escuta direta pelo patch |
|---|---|---|
| Esquerdo, canal 1 | USB L | A → OUT 1, totalmente à esquerda |
| Direito, canal 2 | USB R | C → OUT 2, totalmente à direita |

## Operação

1. No computador, selecione o SMK‑37 Pro como destino de áudio. Para voz ou instrumento, conecte a fonte à interface de áudio e encaminhe a faixa da DAW às saídas USB 1 e 2 do SMK. A DAW precisa ter acesso tanto à interface quanto ao SMK. Uma gravação ou a saída de um instrumento virtual também pode alimentar o patch.
2. No SMK, abra **SCALE → > → MIDI / AUDIO**. Pressione **NOTE REPEAT** até aparecer **USB > MATRIZ**. Use **F2** ou **K4 nessa tela** para acertar o ganho de entrada.
3. Em **PATCH**, navegue para o banco **USB FX** e use os pads 1–4. O pad 1 carrega o patch estéreo e ativa o modo MATRIZ. Os JSON equivalentes estão em `presets-usb`.
4. Ouça na saída física do SMK ou grave o retorno processado que o mesmo dispositivo oferece como entrada de áudio USB. Na DAW, deixe desligado o envio dessa faixa de retorno às saídas USB do SMK para evitar realimentação.

Em **USB > MATRIZ**, as rotas USB L/R somam-se aos destinos escolhidos sem ocupar IN 1 e IN 2. As linhas 8 e 9 preservam a afinação e a velocidade usadas pelos timbres. Em **MIDI / AUDIO → FX**, use K1 para escolher USB L ou R e os pads para alternar entre 0%, 100% e 50% por destino. O Web Studio oferece as mesmas rotas. Para portar um patch de efeitos da 3.7, use **Mover os pinos IN 1/2 para USB L/R**. **NOTE REPEAT** retorna a **MONITOR**, o modo de partida que envia o áudio do computador diretamente à saída física.

| Arquivo | Tratamento |
|---|---|
| `usb-stereo.json` | Roteamento estéreo direto com níveis iguais. |
| `usb-filter.json` | Soma de entrada de ganho reduzido e filtro ressonante ajustável em F3 e no módulo Filter. |
| `usb-spring.json` | Sinal estéreo direto somado ao retorno da mola. F4 ajusta o retorno. |
| `usb-ring.json` | Áudio esquerdo modula o oscilador 1. Ajuste a frequência do oscilador para mudar o timbre. |

A matriz pode encaminhar essas fontes para a envoltória ou usar o monitor B para o disparo por limiar. Os quatro patches USB também estão no painel. A lista consultada por K4 no Overview contém os 128 timbres de fábrica.

## English

[Português](#entrada-de-áudio-usb--usb-audio-input)

The EMS Synthi AKS has line and low-level microphone inputs. On the SMK‑37 Pro, the Felucca base receives 44.1 kHz, 16-bit stereo USB playback, which the BSP resamples for the codec clock. USB L/R have independent matrix routes. IN 1/2 retain keyboard pitch and velocity for instrument patches. [Synthi Users Manual](https://whitefiles.org/rwz/zxe/1970_ems_synthi_users_manual.pdf)

1. Route audio from the computer or DAW to USB channels 1/2 of the Pro. For a live microphone or instrument, connect it to a computer audio interface and route that track to the Pro.
2. On the Pro, use **SCALE → > → MIDI / AUDIO → NOTE REPEAT** to select **USB > MATRIZ**. Set input gain with F2 or K4 on that screen.
3. Open **PATCH → USB FX** and choose a pad from 1–4. The stereo patch assigns USB L to A → OUT 1 and USB R to C → OUT 2. Under **MIDI / AUDIO → FX**, K1 selects L/R and pads cycle gain among off, 100% and 50%. Studio exposes the same routes. Press NOTE REPEAT again for direct monitor.
4. Listen on the instrument or record the processed USB return. Disable sends from the DAW return track to the Pro to avoid a feedback loop.

| Patch | Processing |
|---|---|
| `usb-stereo.json` | Direct stereo routing with matched levels. |
| `usb-filter.json` | Reduced-gain input sum and resonant filter. Adjust with F3. |
| `usb-spring.json` | Direct stereo plus spring return. Adjust mix with F4. |
| `usb-ring.json` | Left channel modulates oscillator 1. Adjust oscillator frequency to change the spectrum. |
