# EMS Synthi AKS · História e emulação

## Do VCS3 ao AKS

A Electronic Music Studios (EMS) lançou o VCS3 em 1969, com eletrônica projetada por David Cockerell. Em 1971, o Synthi A, chamado Portabella durante o desenvolvimento, acomodou em uma maleta o mesmo conjunto básico de placas e as especificações do VCS3. Gerry Rogers assina o desenho industrial do Synthi A. Em 1972, a EMS reuniu o sintetizador portátil e o teclado-sequenciador KS, instalado na tampa da maleta. O conjunto recebeu o nome Synthi AKS. [Histórico de produtos da EMS](https://emssynthesisers.co.uk/emsprods.html) · [Sound On Sound, *All About EMS, Part 2*](https://www.soundonsound.com/music-business/all-about-ems-part-2)

O instrumento reúne três osciladores controlados por tensão, gerador de ruído com ajuste de cor, filtro ressonante com região de auto-oscilação, modulador em anel, envoltória trapezoidal, reverberação de mola, amplificadores de entrada e dois canais de saída com tom, nível e panorama. O terceiro oscilador também opera em baixas frequências para modulação. O joystick fornece dois eixos de tensão de controle. O equipamento dispõe de entradas de linha e de baixo nível, alto-falantes para monitoração e um medidor com modos de nível de sinal e tensão de controle. [EMS, *Synthi Users Manual*, seções 3 e 4](https://whitefiles.org/rwz/zxe/1970_ems_synthi_users_manual.pdf) · [Grogono, *The Synthi Educational Handbook*, caps. II e III](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf)

### A matriz como prática de composição

Na matriz de 16 × 16, cada linha oferece uma fonte e cada coluna recebe um sinal. O pino liga os dois pontos. Uma conexão pode encaminhar áudio, usar a saída de um oscilador para modulação ou realimentar um módulo. O manual de Peter Grogono ensina por meio dessas conexões, desde a produção do primeiro som até o uso de uma sequência gravada ao lado de uma voz ao vivo. O AKS aparece no rock progressivo e na música eletrônica. O engenheiro Alan Parsons relatou que David Gilmour programava no Synthi AKS a sequência de “On the Run”, do Pink Floyd, para executá-la ao vivo. O preset do ziviSynthi AKS é uma interpretação desse procedimento. [Grogono, caps. II a IV](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf) · [Alan Parsons, entrevista à revista *Keyboard*, 2009](https://www.pinkfloydz.com/interviews/alan-parsons-interview-keyboard-online-april-2009/)

### O teclado e sequenciador KS

O KS reúne um teclado sensível ao toque de 30 notas e uma memória digital para até 256 eventos. O circuito separa a afinação ao vivo, a dinâmica do teclado e o disparo da envoltória da afinação gravada. O relógio estabelece a base de tempo da gravação e da reprodução. É possível transpor a frase, variar a largura dos intervalos e usar o controle Random. No Experimento 12, o manual descreve duas montagens. Para tocar com o teclado, **I8** leva a afinação ao vivo ao oscilador 1. Para tocar com o sequenciador, **I16** leva a afinação gravada ao mesmo oscilador. **D3** encaminha o oscilador 1 à envoltória e **A12** leva o sinal resultante à saída 1. [Grogono, cap. IV e Experimento 12](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf) · [EMS, *Synthi AKS Circuit Diagrams and Layouts*](https://medias.audiofanzine.com/files/ems-synthi-aks-service-manual-481665.pdf)

## Como o ziviSynthi AKS modela o instrumento

Victor Valentim desenvolveu o ziviSynthi AKS a partir das rotas e práticas de execução descritas nos manuais. Um núcleo DSP autoral em C roda no SMK‑37 Pro sobre **fm1-base** e **felucca-bsp**. O Web Studio tem um motor de síntese em JavaScript e compartilha as definições de parâmetros, patches e presets usadas para gerar o editor. No navegador, o áudio é produzido localmente. Pela conexão USB MIDI, as alterações circulam nos dois sentidos e a edição pode partir do patch do teclado ou do navegador.

| Circuito ou interface | Implementação digital |
|---|---|
| Matriz e pinos | Preserva os 256 endereços e compila as rotas. Realimentações e fontes calculadas mais adiante na cadeia usam um atraso de uma amostra. Branco e vermelho têm ganho nominal. O verde tem meio ganho. |
| Osciladores e filtro | Modela as formas de onda e os controles Shape, interpola as posições de referência de frequência e usa uma cascata não linear de três polos com auto-oscilação no filtro. |
| Envoltória e reverberação | Separa o sinal de controle Trapezoid do áudio processado pela envoltória. A reverberação inspirada em mola usa linhas de atraso dispersivas. |
| KS e joystick | Registra os eventos no relógio do áudio. No modo padrão, o KS tem uma rota interna própria. No modo EMS, a rota I16 fica disponível. PITCH e MOD do Pro operam Stick X e Stick Y. |
| Estéreo e roteamento | Mantém dois canais de saída com panorama e limitadores separados para saída e captura no Pro. O retorno USB tem rotas próprias, ao lado da afinação e da dinâmica ao vivo. |
| Performance e edição | Acrescenta sequenciador de 64 passos, ARP, quantização por escala, MIDI Clock, três slots USER no Pro e patches portáveis em JSON. |

O modelo acompanha a lógica dos controles e das conexões do instrumento histórico. A normalização de níveis, o atraso digital nas realimentações, a aproximação da mola e as extensões USB/MIDI são escolhas desta implementação. [Matriz de pinos](MATRIX_REFERENCE.md) · [Operação do KS](KS_SEQUENCER.md) · [Manual do controlador e do Studio](SMK37_FELUCCA_BANCADA.md)

## Autoria e uso de IA

Victor Valentim dirige o projeto e define o comportamento musical, a interface e os requisitos do firmware. O projeto emprega desenvolvimento iterativo assistido por IA, também conhecido como *vibe coding*, na implementação, revisão de código, interface, documentação e automação de compilação. O assistente identificável nesta revisão foi **OpenAI Codex, baseado em GPT‑6**. Essa declaração se refere à revisão atual. Os identificadores dos modelos usados nas iterações anteriores não ficaram registrados no código-fonte.

## Conferência humana no SMK‑37 Pro

Victor Valentim relatou a instalação do firmware alternativo em um **M‑VAVE SMK‑37 Pro** físico e confirmou a saída de áudio USB em uso. Na versão **3.8**, confirmou a reprodução de áudio Bluetooth em seu fone sem fio. Na mesma sequência de desenvolvimento, relatou nível muito baixo no conector P2 mesmo depois de restaurar o firmware de fábrica V16. Esses registros correspondem à unidade usada pelo autor. A confirmação de áudio Bluetooth refere-se à 3.8 e o relato sobre o P2 permanece sem diagnóstico confirmado.
