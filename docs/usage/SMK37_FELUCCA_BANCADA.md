# ziviSynthi AKS · Felucca Pro 3.9.5

[English](SMK37_FELUCCA_BANCADA.en.md) · [Instalação](INSTALL.md) · [Entrada USB](ENTRADA_AUDIO_USB.md) · [Presets do Handbook](PRESETS_HANDBOOK.md)

Firmware de bancada para o **SMK-37 Pro**, na base **fm1-base + felucca-bsp**. A versão 3.9.5 tem identidade `SMK-37 Pro_99400`.

## Editor USB e presets pessoais

1. Conecte o SMK-37 Pro por USB com o Felucca Pro 3.9.5 instalado.
2. Abra o editor em Chrome ou Edge por localhost ou HTTPS. Do pacote extraído, execute o comando abaixo e abra [127.0.0.1:8000](http://127.0.0.1:8000/). A compilação do código-fonte também disponibiliza `make serve`.
3. Selecione **Autorizar USB MIDI**, permita SysEx, escolha as duas portas USB do SMK e clique em **Vincular SMK**.
4. Use **Carregar do SMK** para editar o timbre do teclado ou **Aplicar patch e editar ao vivo** para enviar o patch aberto no navegador. Os botões do painel continuam funcionais.
5. Abra **Presets** pelo menu ou pela seta. Dê um nome ao patch e clique em **Salvar como novo**. Selecione-o em **Meus presets** para recuperar, atualizar o conteúdo ou renomear pelo campo **Nome**. **Exportar JSON** e **Importar JSON** transportam os timbres entre perfis ou computadores.

```sh
python3 -m http.server 8000 --bind 127.0.0.1 --directory web
```

O editor lembra as portas após confirmação pelo protocolo. Se a porta USB voltar depois de uma interrupção, ele tenta vincular novamente. O vínculo reaberto permite ler o SMK ou aplicar o rascunho do editor. Confirme o sentido da transferência para retomar a edição ao vivo.

Depois de carregar ou aplicar um patch, o Studio sincroniza em ambos os sentidos os parâmetros de síntese, a matriz de pinos e as rotas KS e USB, os eventos KS, a grade SEQ, ARP, escala e os 24 endereços de CC. Na 3.9.5, o retorno envia só os bytes alterados nos campos de síntese, pinos, rotas, painel e sequências. A leitura completa serve para obter a primeira referência e recuperar uma revisão perdida. Durante a gravação, a sequência é transmitida na conclusão. As versões 3.9.4 e 3.9.3 usam os protocolos incrementais anteriores. O firmware representa pinos brancos e vermelhos pelo mesmo ganho. O editor conserva a indicação vermelha em seu JSON enquanto a rota permanece válida.

**Controles do painel SMK** sincroniza Master, nível do sintetizador, retorno USB, fonte de áudio USB, canais MIDI de entrada e saída, destino, clock interno/externo, transmissão de clock, referência e saída de notas do KS, USB Thru, modo de CC dos knobs e LEDs dos pads. Escolha o modo relativo no editor ou pressione **FX** na tela **SCALE → MIDI / AUDIO** em modo controlador. O modo absoluto permanece o padrão ao ligar. Serviço Bluetooth e associação de portas continuam operações do próprio painel.

**Perfil DSP** seleciona a calibração AKS ou o comportamento legado 3.9.4 para a sessão. No teclado, use **SCALE → MIDI / AUDIO → KBANK + FX**. O perfil AKS é o padrão de inicialização e a referência do banco 3.9.5. Para comparar exatamente os presets originais da 3.9.4, use a reversão ou os JSON daquela versão com o perfil legado.

Em **Mapeamento de CC**, associe um módulo a cada banco A/B. Os padrões são OSC 1 em A e OSC 3 em B. Os CC configurados para K1–K8 seguem o módulo escolhido. O giro relativo usa complemento de dois e parte da posição atual do parâmetro. Pitch Bend e CC1 continuam controlando os eixos do joystick pelo MIDI.

A coleção de presets persiste no mesmo perfil e endereço do navegador. Use **Gravar preset do editor** para enviar o patch aberto e salvá-lo em USER 1–3. **Gravar estado do SMK** salva o estado atual do instrumento. **Carregar slot** recupera o slot físico para o SMK e para o editor. Para editar pelo teclado, use **PATCH → KNOB BANK**, escolha um dos pads **1–3** e pressione **REC** para gravar ou **PARA** para recuperar. A identificação na memória física é pelo número do slot. Os presets da coleção web têm nome próprio. Pare os transportes, solte as teclas e encerre a gravação antes de salvar.

A gravação em flash é manual. Cada slot possui duas cópias alternadas com CRC. O build usa somente os setores `0xEE000–0xF3FFF` e verifica se o setor escolhido está vazio ou pertence ao ziviSynthiA antes de apagar. A memória salva matriz, controles, rotas USB, KS, grade SEQ, ARP/SCALE e os CC programáveis. A gravação em flash pode interromper brevemente o áudio.

## Painel AKS no navegador

O painel reúne os módulos e concentra Noise Generator e Output Filter na mesma faixa. A matriz segue a folha de patch AKS. As fontes ficam à esquerda, os destinos acima da grade, as letras A–P no rodapé e as linhas 1–16 à direita. O foco ilumina a linha e a coluna. O guia identifica três famílias de pinos. Branco comum e vermelho de precisão operam com nível digital nominal. O verde opera com atenuação digital de 50%. A bandeja de 21 pinos segue o conjunto EMS documentado em serviço.

Os mostradores de frequência usam uma curva contínua calibrada pelo manual de serviço. OSC 1 e 2 têm 261 Hz na posição 6, 2.088 Hz na posição 8 e 10 kHz na posição 10. No OSC 3, a posição 8 corresponde a 63 Hz e a 10 corresponde a 500 Hz. As teclas junto ao knob aplicam as referências de 1 kHz e 10 kHz, ou 0,05 Hz e 500 Hz no OSC 3. Os nós entre os pontos do manual usam interpolação exponencial. Consulte o [manual de serviço, ajustes dos osciladores](https://medias.audiofanzine.com/files/ems-synthi-aks-service-manual-481665.pdf#page=56).

No filtro, a posição 5 produz cerca de 261 Hz em auto-oscilação com **Response** no máximo. O parâmetro digital de corte nessa posição é 150,855 Hz. A posição 5 do botão **Response** indica a região de início da auto-oscilação. Os valores gravados nos presets permanecem em unidades do DSP. [Manual de serviço, teste Filter/Oscillator](https://medias.audiofanzine.com/files/ems-synthi-aks-service-manual-481665.pdf#page=50)

O medidor do painel web acompanha a soma das rotas da coluna **B** no áudio gerado pelo navegador. Em **Control voltage**, a leitura é bipolar, com zero central. Use B11 para acompanhar TRAPEZOID ou B15 para testar Stick X. Em **Signal levels**, a escala começa à esquerda e mostra o pico retificado. Use B4 para medir o oscilador 2. A escala mostra porcentagem normalizada. Para ler o nível estéreo em dBFS, abra **Monitor de sinal**.

O LCD trabalha em 240 × 240. O pacote inclui prévias de 256 × 256 com margem em torno da captura nativa. A barra B no LCD mostra o pico da coluna do medidor.

O joystick X/Y atua pelas linhas 15 e 16. Arraste a alavanca ou use as setas com o joystick em foco. Home e Centralizar retornam ao centro. Range X e Range Y ajustam a atenuação. A partir da 3.9.4, em edição ao vivo, os ranges do navegador e do teclado ficam sincronizados. No SMK, abra **SCALE → MIDI / AUDIO**, mantenha **KBANK** e gire K1 para X ou K2 para Y. Os ranges valem para a sessão. Na atribuição EMS, o KS assume a linha 16 e o eixo Y fica desabilitado. O painel oferece Attack manual, indicação de pico, osciloscópio, teclado de C3 a F5 e pads de transposição do KS. As tomadas de CV e os circuitos dos alto-falantes do AKS pertencem ao instrumento analógico. Os pinos web mantêm intensidades digitais. Consulte o [Synthi Educational Handbook](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf) e a [folha de patch AKS](https://medias.audiofanzine.com/files/ems-synthi-aks-service-manual-481665.pdf).

## SEQ, KS e gravação

**SEQ** abre o editor monofônico de passos, com 16 posições por página. Presets com frase KS carregam automaticamente uma cópia editável. Os ataques, pausas internas, durações e legatos do KS são preservados na importação. O andamento ajusta a duração relativa do padrão. Ao carregar um preset sem frase, o padrão editável atual continua disponível. [Controles do Felucca](https://github.com/hugelton/Felucca/wiki/Controls) · [Manual do SMK-37 Pro](https://device.report/manual/20287069)

| Controle | Ação |
| --- | --- |
| **K1** | Define o comprimento ativo de 1 a 64 passos. Ao voltar a aumentar, recupera os passos já preenchidos fora do comprimento anterior. |
| **K2**, **K3**, **K4**, **K5** | Nota MIDI, velocidade, andamento e cursor. |
| **<** e **>** | Navegam entre as páginas. PLAY ativa o acompanhamento da posição. |
| **Pads 1–16** e **PARA** | Ligam ou silenciam passos. Um passo novo recebe a última nota tocada e sua velocidade. |
| **FBANK** | Alterna **STEP** e **LIVE**. |
| **REC em STEP parado** | Arma a entrada. Toque para preencher e avançar. Mantenha um pad pressionado e toque uma tecla para escrever naquele passo. |
| **REC em LIVE ou durante a reprodução** | Inicia uma nova tomada de um ciclo, substituindo o padrão da sessão. Captura teclado, entrada MIDI ou a saída do ARP, conforme o modo ativo. REC conclui antes do fim e volta ao início em reprodução. PLAY encerra o transporte. |
| **PLAY** | Inicia ou encerra a reprodução. |
| **FX** | Importa para SEQ a frase KS atual, se couber em 64 passos. |
| **KBANK** | Copia o padrão editado para KS, até 30 segundos. Use **Carregar do SMK** no editor web para obter o timbre, a frase e o padrão em separado. |
| **REPEAT** | Acessa o KS. **REC** arma o KS e **PLAY** reproduz. **<** para. **PARA** escolhe a fonte do disparo e **REPEAT** alterna o loop. Segure **FX** para Random. |

Uma gravação KS concluída no painel ou por STOP no editor também carrega os passos automaticamente quando cabe no SEQ. Um passo guarda uma nota monofônica. Em LIVE, um ataque extra dentro de um passo ocupado não cria outro passo. Um som sustentado pode ocupar vários passos ligados. O SEQ e o KS seguem MIDI Clock quando **MIDI / AUDIO → K5 EXT** está selecionado. O formato portátil do KS usa resolução de 1 ms. A grade SEQ preserva seus tempos em quadros do DSP. No Studio, abra **Execução → SEQ** para ajustar comprimento, BPM, divisão, nota e velocidade. Com a edição ao vivo vinculada à 3.9, a faixa superior mostra transporte e clock. A borda azul indica o passo em edição e a barra âmbar acompanha o passo informado pelo SMK. A leitura do patch recupera a grade sem precisar publicá-la em KS.

No editor, **Ler SEQ do SMK** lê a grade ativa. **Enviar SEQ ao SMK** aplica comprimento, andamento e até 64 passos à grade em execução no teclado. Para apagar um trecho, selecione de e até, aplique **Apagar trecho** e envie. **Apagar sequência no editor** limpa todos os 64 passos. **Gravar SEQ no slot selecionado** envia a grade e atualiza somente o SEQ de um USER ocupado. Um slot vazio precisa receber um preset primeiro.

A biblioteca de sequências oferece entradas de grade completa e de trecho e usa JSON próprio. Para atualizar um trecho salvo, cole-o na grade, edite, selecione o intervalo e use **Atualizar**. Trechos preservam a divisão e as durações em quadros. Para inserir um trecho de outra base temporal, use uma grade vazia. As operações de biblioteca preparam o rascunho e desativam a edição ao vivo dos passos. A transferência independente exige firmware 3.9.1. No firmware 3.9, o SEQ continua disponível pela transferência do patch completo.

## ARP e SCALE

**ARP** abre diretamente o arpejador e retorna à tela anterior ao ser pressionado novamente. **SCALE** abre tônica e escala. Os pads 1–12 de SCALE selecionam a tônica, K2 percorre as 32 escalas, K3 salta oito e PARA retorna à cromática. A escala se aplica às teclas locais, inclusive no MIDI Out, e às notas recebidas pelo ARP a partir de MIDI externo.

| Controle no ARP | Ação |
| --- | --- |
| **PARA** | Liga ou desliga o ARP. |
| **K1**, **K2**, **K3**, **K4** | Andamento de 30 a 300 BPM, ordem, gate de 10% a 95% e divisão. |
| **K5** | Chance de cada pulso tocar, de 0% a 100%. Pulsos descartados viram pausas na gravação. |
| **Pads 1–8** | Escolhem divisão de semínima a fusa, incluindo tercinas. |
| **Pad 9** | HOLD sustenta o último grupo de notas. Um novo grupo o substitui. |
| **Pad 10** | FREE percorre uma oitava da escala escolhida quando nenhuma tecla ou acorde em HOLD fornece notas. Com a ordem aleatória, sorteia as notas da escala. |
| **REC** | Captura no SEQ as notas que o ARP realmente emite, com o andamento, a divisão e o gate em uso. |

O editor web oferece **KS**, **SEQ**, **ARP** e **Scale**. Recebe a configuração do painel e permite alterar chance, HOLD e FREE ao vivo. O padrão de passos é editável no painel e no Studio. Os ajustes do ARP e de SCALE são incluídos nos slots USER e no JSON de patch. No modo controlador, o botão ARP envia CC110.

## GLOBAL / MODULES

**GLOBE** abre o painel de módulos. Use **<**, **>** ou os **pads 1–10** para escolher um módulo. Cada célula no LCD mostra o encoder físico, o parâmetro e seu valor. **FX** alterna entre módulos e osciloscópio. No osciloscópio, **PARA** congela a leitura. Em GLOBAL, **REC** continua sustentando o Attack manual.

| Pad | Módulo | K1–K4 | K5–K8 |
| --- | --- | --- | --- |
| 1 | Oscillator 1 | Frequência, forma do seno, nível do seno, nível da rampa | Transposição fina, mistura seno/rampa, livre, master |
| 2 | Oscillator 2 | Frequência, forma, nível do pulso, nível da triangular | Livres, master em K8 |
| 3 | Oscillator 3 | Frequência, forma, nível do pulso, nível da triangular | Livres, master em K8 |
| 4 | Noise Generator | Cor espectral, nível, livre, livre | Livres, master em K8 |
| 5 | Ring Modulator | Nível, livre, livre, livre | Livres, master em K8 |
| 6 | Filter / Oscillator | Corte, Response, Drive, nível | Livres, master em K8 |
| 7 | Envelope Shaper | Attack, On, Decay, Off | Nível do sinal, nível do trapezoide, modo, trigger |
| 8 | Spring Reverb | Mistura, nível, tempo, livre | Livres, master em K8 |
| 9 | Input Amplifiers | Nível da entrada 1, nível da entrada 2, livre, livre | Livres, master em K8 |
| 10 | Output Channels | Nível 1, tom 1, panorama 1, nível 2 | Tom 2, panorama 2, nível do patch, master |

No Envelope Shaper, use **PARA + K8** para alterar o master físico. Nos demais módulos, em outras telas do instrumento e no osciloscópio, **K8** controla o master diretamente. Os níveis de entrada correspondem às linhas 8 e 9 do motor.

As famílias visuais seguem o painel de referência. **Azul** sinaliza frequência e conteúdo espectral, **verde** ajustes secundários, **marfim** níveis, **amarelo** funções de filtragem e **vermelho** a envoltória. A interface web exibe os mesmos rótulos e números de encoder. O master físico continua no teclado. [EMS Synthi AKS de 1973](https://soundgas.com/products/ems-synthi-aks-1973) · [Manual do Synthi V, seção 4.1.2](https://downloads.arturia.net/products/synthi-v/manual/Synthi_V_Manual_1_0_1_EN.pdf)

## Synth e áudio

- **PATCH** abre a grade de presets 4×4. Use **<** e **>** para navegar pelos nove bancos de timbres e o banco **USB FX**. Os pads carregam patches. **K5** move o cursor e **PARA** carrega. O K4 do Overview percorre os 128 timbres. Em MIDI Bank Select 0, os índices 0–127 continuam na mesma ordem. Envie **CC0 = 10** e Program Change 0–3 para acessar **USB FX**.
- Na matriz **16×16**, **K5** move a linha, **K6** a coluna e **K7** muda o tipo de pino. **FX** alterna a ampliação. Pressionar um pad edita a célula correspondente na região 4×4. A fileira física de cima contém os pads 01–08 e a de baixo contém 09–16.
- A matriz usa círculo âmbar para pino cheio, traço branco para meio pino e contorno para ponto disponível. O quadrado azul marca o foco. Os LEDs dos pads mantêm as cores dos estados. **PITCH** controla Stick X na linha 15 e **MOD** controla Stick Y na linha 16. O modo **EMS** atribui a linha 16 à afinação do KS.
- **BT** abre BLE MIDI. K1 à direita ou PLAY liga o rádio. Com o rádio pronto, PLAY busca dispositivos, K6 seleciona e PARA conecta. **<** desconecta o dispositivo MIDI remoto.
- Na tela **BT**, pressione **>** para **BLUETOOTH AUDIO**. Coloque o fone ou a caixa em pareamento e use **PLAY** para buscar, **K6** para selecionar e **PARA** para conectar. **FX** alterna o áudio entre Bluetooth e saída local quando conectado. **<** encerra a transmissão. O perfil A2DP/SBC envia áudio. A recepção A2DP e a persistência de pareamentos aguardam implementação.
- Em **GLOBE**, mantenha **REC** pressionado para sustentar o Attack manual até soltar. O pedal também sustenta a envoltória.
- **MIDI / AUDIO** reúne os canais TX e RX, a rota USB/BLE/TRS, o relógio MIDI e os indicadores de falta de PCM (`MISS`) e perdas de DMA (`DMA`). **PARA** zera o pico de renderização. **FBANK** alterna o encaminhamento USB para TRS nessa tela.

| Controle | Ação no modo instrumento |
| --- | --- |
| K8 | Volume da saída física com rampa. Em Envelope Shaper, use PARA + K8. A captura USB é independente. |
| F1 | Nível global do sintetizador, depois do nível do patch e antes do limiter. |
| F2 | Nível de áudio vindo do computador. K4 em MIDI / AUDIO ajusta o mesmo ganho. REPEAT escolhe saída física ou matriz nesse menu. |
| F3 | Corte do filtro do patch ativo. |
| F4 | Mistura de reverberação do patch ativo. |

O estágio da síntese acrescenta 6 dB antes do limiter estéreo da captura USB. A saída física usa outro limiter após a soma do sintetizador e do retorno USB. O teto digital dos dois estágios é −1 dBFS, com aproximadamente 2,9 ms de antecipação por estágio. O codec mantém 44.118 Hz nominais. O áudio USB estéreo de entrada e saída mantém 44.100 Hz e 16 bits. O build inclui Bluetooth MIDI periférico e central e transmissão A2DP/SBC.

## LEDs dos pads e diagnóstico de áudio

A opção **SCALE → > → REC** alterna os modos abaixo. Todos os modos iluminados limitam cada canal RGB a um oitavo da escala original. O firmware envia no máximo uma atualização a cada 96 ms e exige quatro blocos de áudio prontos antes de iniciar a transferência dos LEDs. O firmware adia essa transferência se o DMA do LCD estiver ocupado.

| Modo | Comportamento |
| --- | --- |
| **FIXO** | Padrão ao ligar. O painel indica a seleção em cor fixa e o LCD mostra a posição. As transferências de LED aguardam o fim da atividade do sintetizador, incluindo a cauda detectada no PCM. |
| **ANIM.** | Permite realçar a posição e o gate, com brilho e frequência limitados. |
| **OFF** | Envia o quadro escuro e mantém os LEDs dos pads apagados. |

Para comparar em bancada, reproduza a mesma frase com volume idêntico em **FIXO** e **OFF**, primeiro na saída física e depois na captura USB. Confira **MISS** e **DMA** durante a reprodução. O simulador cobre o fluxo de áudio digital. A interferência elétrica dos LEDs exige medição no Pro.

## Controlador MIDI externo

Use **GLOBE + PATCH** para alternar entre instrumento e controlador. O perfil implementa dois bancos independentes por grupo e encaminha o MIDI pelo destino configurado em **MIDI / AUDIO**. Os CC abaixo são do firmware ziviSynthiA. O menu **Mapeamento de CC** do editor permite ajustar os 24 CC dos knobs e faders nos bancos A/B. A tabela abaixo mostra os valores iniciais. A paridade exata com um perfil do MIDI Suite depende da tabela exportada do equipamento.

| Controle | Banco A | Banco B | Como operar |
| --- | --- | --- | --- |
| Teclas 1–37 | Notas 48–84 inicialmente | — | Velocidade e Note Off. OCTAVE −/+ desloca 12 semitons. |
| Pads 1–16 | Notas 36–51 | Notas 52–67 | **KBANK + FBANK** troca o banco. Envia velocidade e pressão polifônica. |
| K1–K8 | CC16–23 | CC24–31 | **KBANK** troca o banco. Use **PARA + K8** para transmitir K8 no banco ativo. Manter **KBANK + K8** envia CC31. |
| F1–F4 | CC70–73 | CC76–79 | **FBANK** troca o banco. |
| Pitch Bend | 14 bits | — | Centro 8192. |
| Modulação | CC1 | — | Faixa 0–127. |
| Sustain | CC64 | — | 127 pressionado, 0 solto. |
| SEQ PLAY | CC119 e Start/Stop | — | Start/Stop na tela principal do controlador. |

Os 18 botões enviam 127 ao pressionar e 0 ao soltar.

| CC | Botão | CC | Botão |
| ---: | --- | ---: | --- |
| 102 | FX | 111 | OCTAVE − |
| 103 | SCALE | 112 | OCTAVE + |
| 104 | CHORD | 113 | KBANK |
| 105 | REPEAT | 114 | FBANK |
| 106 | PARA | 115 | SEQ REC |
| 107 | GLOBE | 116 | < |
| 108 | PATCH | 117 | > |
| 109 | BT | 118 | SEQ |
| 110 | ARP | 119 | SEQ PLAY |

## Diagnóstico de desempenho

No Studio USB, abra **Diagnóstico de desempenho** e clique em **Iniciar medição**. No teclado, **PARA** em **SCALE → MIDI / AUDIO** reinicia a mesma janela. Toque ou reproduza um SEQ com um patch denso, gire os knobs, altere pinos e mantenha áudio USB ativo. Repita com LEDs fixos, animados e apagados, com áudio Bluetooth em uma rodada própria. Confira o maior tempo do DSP, blocos no limite, menor fila PCM e incrementos de MISS, DMA e USB DROP. A leitura do Studio informa ainda o custo máximo de processamento de um comando. O prazo nominal é **2,901 ms** por bloco de 128 quadros a 44.118 Hz. Registre os resultados por cenário para comparar as cargas na mesma unidade. [Roteiro de bancada](../development/HARDWARE_VALIDATION_PRO.md)

## Memória e expansão

A verificação extrai o tamanho de `app.bin` e as seções do mapa de enlace. Consulte o relatório de inspeção da imagem gerado pela compilação para os valores e a comparação com 3.9.4. Flash, RAM para código, RAM estática e pool de buffers têm regiões distintas. A folga de memória mede capacidade de enlace. O custo por bloco de DSP de expansões precisa ser medido no instrumento a 128 amostras por bloco.

## Instalação e compilação

No editor distribuído, abra **Firmware** pelo menu. Conecte o SMK‑37 Pro diretamente por um cabo USB de dados e use Chrome ou Edge em um computador. Feche outros atualizadores e aplicações MIDI com acesso ao teclado.

1. Clique em **Autorizar USB MIDI**, selecione a porta USB e clique em **Identificar teclado**.
2. Escolha **Felucca 3.9.5** ou um `.fwsc` reconhecido. A reversão 3.9.4 pode ser fornecida como arquivo externo ou incorporada em um build opcional. O editor confere o SHA‑256, os CRC do pacote e o tamanho da área de aplicação. O botão indica **Instalar Felucca**, **Atualizar firmware**, **Reverter firmware** ou **Reinstalar firmware**, conforme a identidade USB e o pacote.
3. Confira os dados e as cópias na confirmação. **Exportar patch do editor** gera o JSON do patch ativo no navegador. Preserve os presets USER e as sequências que quiser manter antes de gravar.
4. Mantenha o cabo e a aba abertos. A edição via Studio USB é pausada até o encerramento da operação. O editor verifica o carregador e confirma a identidade esperada depois do reinício. Para voltar ao Studio, vincule a porta novamente e leia o patch do teclado.

Se a gravação for interrompida e o teclado permanecer no carregador do Pro, mantenha a aba, identifique o teclado e use o mesmo pacote em **Retomar gravação**. Se o teclado não enumerar por USB, siga o procedimento de recuperação da base. A restauração do **V016 original**, com seus recursos de fábrica, usa o procedimento M‑VAVE disponível pelo link no módulo. [M‑VAVE · Atualizações e recuperação](https://github.com/m-vave/smk37_open#updates-and-recovery)

As versões Felucca Pro de bancada `9800` a `99400` usam o mapa com início da VM em `0x9D000`. A Open 0.8.2-alpha2 usa VM a partir de `0x72000`, dentro da área gravada pelo Felucca. A migração de uma unidade com a base Open exige recuperação específica do Pro com backup válido. A 3.9.1 e a 3.9.2 respondem com o mesmo identificador USB. O editor confere a versão do arquivo por SHA‑256 e o identificador compartilhado ao reiniciar nessas versões.

| Componente | Revisão |
| --- | --- |
| fm1-base (`smk37-inhouse`) | `eafac162b4d7bc59176919a36eed179955315f93` |
| felucca-bsp | `a702b840ae2bb7c4fbbd0a168ed509e129b2e9f7` |
| Ferramentas | Python 3.12, Pillow, JieLi 20260730.1 e SDK AC79 V1.2.1 |

```sh
make test
make test-smk-felucca FM1_BASE=../fm1-base
make smk-felucca-firmware FM1_BASE=../fm1-base AC79_SDK=/caminho/ac79 \
  JIELI_TOOLCHAIN=/caminho/jieli-linux-toolchains-20260730.1
python3 tests/verify_felucca_image.py build/smk37-felucca ../fm1-base
make web-release FIRMWARE_ROLLBACK=/caminho/ziviSynthiA-SMK37-Pro-Felucca-3.9.4.fwsc
node --test tests/test_firmware_web.cjs
```

A compilação e o protocolo foram validados em host e por inspeção da imagem. A escuta analógica, o comportamento do USB no computador e a instalação na unidade física exigem teste de bancada.
