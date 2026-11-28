# ziviSynthi AKS · Guia de uso

Use o ziviSynthi AKS como instrumento no navegador ou instale o firmware 3.9.7 no M‑VAVE SMK‑37 Pro. O princípio de síntese é o mesmo nos dois ambientes. Escolha uma fonte, conecte-a a um destino na matriz de 16 × 16 pinos e salve o resultado como patch.

## Comece pelo Web Studio

1. Abra o [Web Studio](https://vicvalentim.github.io/ziviSynthi-AKS/pt/studio/#try) e selecione **Experimentar / Try** para ativar o áudio.
2. Toque as notas com o mouse, por toque ou pelas teclas **A W S E D F T G Y H U J K**.
3. Escolha um preset de fábrica, altere um pino ou knob e exporte o patch ativo em JSON. Em **Instant Patchers**, escolha **Prestopatch Keyboard** ou **On the Run** para iniciar uma frase KS na hora. Em **Editar frase KS**, altere tempos, notas, velocidades e ataques e depois aplique ou exporte a frase.
4. Na **Patch Matrix**, use **Preset do SMK** para carregar qualquer um dos 128 timbres e dos quatro patches **USB FX** do firmware. Para ouvir um USB FX no Try, escolha um arquivo de áudio e clique em **Tocar arquivo**.

## Conecte o SMK‑37 Pro

1. Consulte o [guia de instalação e recuperação](usage/INSTALL.md) para instalar o firmware. Faça uma cópia dos patches pessoais e das sequências antes de gravar.
2. Conecte o Pro por USB, abra o Web Studio no Chrome ou Edge, autorize o acesso ao MIDI USB e selecione as portas de entrada e saída do Pro.
3. Use **Carregar do SMK** para partir do estado do instrumento ou **Aplicar patch e editar ao vivo** para partir do patch aberto no Studio. Durante a edição ao vivo, as alterações são sincronizadas nos dois sentidos.

No controlador, **K4** no Overview troca o preset. **GLOBE** abre os módulos, **PATCH** abre os bancos e **PITCH** e **MOD** operam **Stick X** e **Stick Y**. **SEQ** abre o editor de passos, **ARP** o arpejador e **SCALE** os controles de tônica e escala.

## Referência

| Para | Guia |
|---|---|
| Citar o software depositado | [Pesquisa, citação e preservação](usage/SCIENTIFIC_PUBLICATION.md) |
| Instalar, atualizar ou recuperar o Pro | [Instalação e recuperação](usage/INSTALL.md) |
| Usar o controlador e o Studio | [Manual do controlador e do Web Studio](usage/SMK37_FELUCCA_BANCADA.md) |
| Encaminhar sinais e escolher pinos | [Referência da matriz](usage/MATRIX_REFERENCE.md) |
| Gravar frases e rotear a afinação do KS | [Sequenciador KS](usage/KS_SEQUENCER.md) |
| Explorar os sons de fábrica | [Presets e patches do Handbook](usage/PRESETS_HANDBOOK.md) |
| Encaminhar o áudio da DAW pela matriz | [Áudio USB](usage/ENTRADA_AUDIO_USB.md) |
| Conhecer a história e o projeto do instrumento | [História, emulação e autoria](usage/HISTORY.md) |
| Escolher o modelo e exportar firmware | [Alvos de firmware M‑VAVE](usage/FIRMWARE_TARGETS.md) |

[Notas da versão 3.9.7](releases/v3.9.7.md#português) · [Avisos e créditos](../NOTICES.md#português)
