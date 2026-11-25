# ziviSynthiA 0.8.2-alpha2 para SMK-37 Pro Open

[English](#english)

Esta versão histórica de bancada usa o SDK SMK37 Open V01 da M-VAVE, revisão `07f2269d67d5`. A aplicação do ziviSynthiA controla o painel e sintetiza em estéreo. O SDK lê a placa e fornece o codec, USB, MIDI, Bluetooth e o carregador de atualização. O DSP usa a referência nominal de 44.118 Hz do codec, em blocos de 128 quadros com saída PCM Q23. O USB mantém o perfil estéreo de 24 bits e 44.100 Hz do SDK.

## Controles

| Controle | Função |
| --- | --- |
| PATCH, esquerda e direita | Abrir e navegar pelos quatro bancos com os 62 presets. K5 move o foco e PARA carrega |
| Pads, no banco de presets | Selecionar o preset da posição correspondente |
| Pads, na matriz | Editar os contatos da região selecionada |
| GLOBE + PATCH | Alternar entre sintetizador e controlador MIDI |
| SCALE | Abrir MIDI e áudio. Canal TX em K1, canal RX em K2 e saída em K3. K5 seleciona a fonte de clock, K6 habilita notas KS, K7 habilita a saída de clock e K8 ajusta o volume |
| BT | Abrir a página Bluetooth. K1 alterna o rádio, K2 alterna o áudio, K3 alterna o papel BLE e K6 seleciona o dispositivo |
| PLAY, PARA e esquerda na página BT | Buscar, conectar e desconectar um dispositivo BLE em modo Host |
| SHIFT + REC na página BT | Limpar os pareamentos BLE salvos |
| CHORD | Enviar Note Off e limpar notas presas |

O MIDI entra por USB e BLE. As saídas implementadas são USB, BLE e TRS, de acordo com as portas expostas pelo SDK. A saída do sintetizador e o áudio recebido por USB e Bluetooth são misturados pelo SDK e encaminhados ao codec e à captura USB. Os descritores do ELF anunciam PCM estéreo de 24 bits a 44,1 kHz nos dois sentidos USB. A captura contém a mistura que alimenta a saída do instrumento, incluindo áudio recebido via USB e Bluetooth. Use monitoração direta ou desative a monitoração da faixa na DAW se a saída do computador retornar ao mesmo dispositivo. O áudio Bluetooth é recepção A2DP, de um dispositivo pareado para o teclado.

O limiter master atua depois da soma das três fontes, antes do codec e da captura USB. Usa um ganho comum aos canais, com teto de pico digital de −1 dBFS. A antecipação de 128 quadros acrescenta 2,9 ms ao caminho master. O retorno de ganho é gradual, com constante de tempo de aproximadamente 45 ms. O medidor na página do sintetizador continua indicando o nível antes da mistura master.

## Bancada

**Origem aceita pelo atualizador deste candidato:** SMK37 Open V01 oficial para o Pro ou os builds exatos 0.8.0-alpha1, 0.8.1-alpha1 e 0.8.2-alpha1 do projeto. Para recuperação e compatibilidade de flash com a linha 3.9.5, consulte o [manual do alvo atual](../usage/SMK37_FELUCCA_BANCADA.md#instalação-e-compilação).

1. Faça backup dos dados que precisa conservar e confira o modelo na unidade.
2. Extraia o ZIP. Abra o HTML em Chrome ou Edge de computador, conecte o Pro por USB e clique em **Verificar dispositivo**.
3. Permita MIDI/SysEx e marque a confirmação após revisar o backup. Clique em **Instalar firmware** e mantenha alimentação e USB até a confirmação no reinício.
4. Teste a matriz, a saída de áudio, os quatro bancos e o KS. Em **SCALE**, use SHIFT + PARA para zerar a janela de medição de DSP e anote os contadores acumulados antes e depois dos testes de USB, MIDI e Bluetooth. O [roteiro de bancada](HARDWARE_VALIDATION_PRO.md) indica o que registrar.

A gravação e a operação em tempo real deste candidato aguardam teste no aparelho. Para isolar os estalos, comece pela saída analógica, com USB desconectado e Bluetooth desativado. Toque o mesmo preset novamente por USB. Se houver estalos, anote os incrementos de MISS e DMA em SCALE.

## Recompilar no Linux

Extraia o kit do SDK com as pastas `app` e `sdk` na raiz. Em uma cópia dos fontes do ziviSynthiA, use Python 3.10 ou mais recente, Make, o toolchain JieLi Linux `20260730.1` e a `libm.a` da distribuição AC79 `V1.2.1_2023-12-13`.

```sh
make smk-open-firmware \
  SMK37_OPEN_SDK=/caminho/do/smk37-open-v01 \
  JIELI_TOOLCHAIN=/caminho/do/jieli-linux-toolchains-20260730.1 \
  JIELI_NEWLIB=/caminho/do/ac79/include_lib/newlib/pi32v2-lib
```

Os arquivos ficam em `build/smk37-open/`. O gerador usa os recursos do kit e a estrutura de empacotamento do `sdk/Package.cs`, com checagens do manifesto, do vetor de entrada e das rotinas em SRAM. A cadeia oficial Windows está documentada no README do kit da M-VAVE. Os avisos e as licenças do SDK acompanham o código-fonte.

## English

[Português](#zivisynthia-082-alpha2-para-smk-37-pro-open)

This historical 0.8.2-alpha2 bench target uses M‑VAVE SMK37 Open V01 SDK revision `07f2269d67d5`. The SDK drives the board, codec, USB, BLE and bootloader. The DSP processes 128-frame Q23 stereo blocks at a nominal 44,118 Hz. The SDK advertises 24-bit stereo USB audio at 44.1 kHz. This branch receives A2DP audio and mixes it with USB and synth output. The stereo limiter targets −1 dBFS, with 128 frames of lookahead.

| Control | Action |
|---|---|
| PATCH, <, >, K5 and PARA | Navigate four preset banks, move the cursor and load. Pads select patches or edit the selected matrix region. |
| GLOBE + PATCH | Toggle synth/controller mode. |
| SCALE | MIDI/audio. K1 TX, K2 RX, K3 destination, K5 clock, K6 KS note output, K7 clock output and K8 volume. |
| BT | K1 radio, K2 audio, K3 BLE role, K6 device. In Host mode, PLAY scans, PARA connects and < disconnects. SHIFT + REC clears pairings. |
| CHORD | Release stuck notes. |

The original updater accepted the Pro’s official Open V01 or the project’s exact 0.8.0-alpha1, 0.8.1-alpha1 and 0.8.2-alpha1 releases. Escalate to Pro-specific recovery for a change of flash layout. [Compatibility notes for the current target](../usage/SMK37_FELUCCA_BANCADA.en.md#installation-and-recovery)

To rebuild on Linux, extract the SDK with `app` and `sdk` at its root. Use Python 3.10+, Make and toolchain `20260730.1`. Run the preceding command with `SMK37_OPEN_SDK`, `JIELI_TOOLCHAIN` and `JIELI_NEWLIB` pointing to the local SDK, toolchain and AC79 SDK V1.2.1 math library directory. Results go to `build/smk37-open/`. Keep the SDK’s notices and licenses. This target is outside the 3.9.5 release workflow.
