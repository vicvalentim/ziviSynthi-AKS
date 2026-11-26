# EMS Synthi AKS · History and emulation

## From the VCS3 to the AKS

Electronic Music Studios (EMS) introduced the VCS3 in 1969. David Cockerell designed its electronics. The Synthi A, introduced in 1971 and known during development as the Portabella, put the same basic board set and specifications into a carrying case. Gerry Rogers is credited with the Synthi A's industrial design. In 1972 EMS paired that portable synthesizer with the KS keyboard and sequencer. The combined instrument was the Synthi AKS. The EMS product history places the keyboard and sequencer in the case lid. [EMS product history](https://emssynthesisers.co.uk/emsprods.html) · [Sound On Sound, *All About EMS, Part 2*](https://www.soundonsound.com/music-business/all-about-ems-part-2)

The instrument combines three voltage-controlled oscillators, a noise source with colour control, a resonant filter with a self-oscillation region, a ring modulator, a trapezoid envelope shaper, spring reverb, input amplifiers and two output channels with tone, level and pan. The third oscillator also covers low-frequency control. The joystick supplies two continuously variable control voltages. The original provides line and low-level inputs, monitor speakers and a meter for signal levels and control voltages. [EMS, *Synthi Users Manual*, §§3–4](https://whitefiles.org/rwz/zxe/1970_ems_synthi_users_manual.pdf) · [Grogono, *The Synthi Educational Handbook*, chs. II–III](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf)

### Patching as composition

The 16 × 16 matrix makes the relationship between modules visible. Each row presents a signal source and each column a destination. Inserting a pin connects the two. The same principle routes audible signals and control voltages, so an oscillator can become a modulator and a module output can be fed back into the patch. Grogono's handbook teaches through such connections, from making a first sound to running a recorded phrase alongside a live voice. The AKS appears in progressive rock and electronic music. Engineer Alan Parsons described David Gilmour entering the sequence for Pink Floyd's “On the Run” into a Synthi AKS for live performance. The ziviSynthi AKS preset is an interpretation of that approach. [Grogono, chs. II–IV](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf) · [Alan Parsons, interview with *Keyboard*, 2009](https://www.pinkfloydz.com/interviews/alan-parsons-interview-keyboard-online-april-2009/)

### The KS keyboard and sequencer

The KS combines a 30-note touch keyboard with a digital memory of up to 256 events. It separates live pitch, keyboard dynamics and envelope triggering from the recorded sequence. A clock controls the recording and playback time base. The player can transpose a phrase, change its pitch spread and use the Random touch control. Experiment 12 documents two patching arrangements. With the keyboard, **I8** feeds live pitch to oscillator 1. With the sequencer, **I16** feeds sequencer pitch to that oscillator. **D3** connects oscillator 1 to the envelope shaper and **A12** takes the shaped signal to output 1. [Grogono, ch. IV and Experiment 12](https://www.vasulka.org/archive/RightsIntrvwInstitMediaPolicies/IntrvwInstitKaldron/Amherst%2CMassEMS/SynthiEducHandbook.pdf) · [EMS, *Synthi AKS Circuit Diagrams and Layouts*](https://medias.audiofanzine.com/files/ems-synthi-aks-service-manual-481665.pdf)

## How ziviSynthi AKS models the instrument

Victor Valentim developed ziviSynthi AKS around the routing and playing practices described in those manuals. An original C DSP core runs on the SMK‑37 Pro through **fm1-base** and **felucca-bsp**. Web Studio has a JavaScript synthesis engine and shares the parameter, patch and preset definitions used to build the editor. In browser mode it generates audio locally. In USB MIDI mode it also exchanges changes with the Pro, so editing can begin with either the hardware patch or the browser patch.

| Circuit or interface | Digital implementation |
|---|---|
| Matrix and pins | Preserves the 256 coordinates and uses a compiled routing graph. Feedback and sources calculated later in the processing order use a one-sample delay. White and red represent nominal gain. Green uses half gain. |
| Oscillators and filter | Models the waveforms and their shape controls, interpolates the reference positions for frequency, and models the filter with a nonlinear three-pole cascade and self-oscillation. |
| Envelope and reverb | Separates the trapezoid control signal from the shaped audio. Spring-style reverb uses dispersive delay lines. |
| KS and the joystick | Records note events against the audio clock. In standard mode KS pitch has its own internal route. EMS mode makes the I16 patch available. The Pro's PITCH and MOD controls act as Stick X and Stick Y. |
| Stereo and audio routing | Uses two pannable output channels with separate output and capture limiting on the Pro. USB return has its own routes into the matrix alongside live pitch and velocity. |
| Performance and editing | Adds a 64-step sequencer, ARP, scale quantization, MIDI Clock, three USER slots on the Pro, and portable JSON patches. |

The model follows the controls and routing of the historical instrument. Its normalized signals, digital feedback timing, spring approximation and USB/MIDI extensions are design choices for this implementation. [Pin matrix](MATRIX_REFERENCE.md#english) · [KS operation](KS_SEQUENCER.md#english) · [Controller and Studio manual](SMK37_FELUCCA_BANCADA.en.md)

## Authorship and AI assistance

Victor Valentim directs the project and defines the musical behavior, interface and firmware requirements. The project uses AI-assisted iterative coding, also known as *vibe coding*, for implementation, code review, interface work, documentation and build automation. The identifiable assistant for this revision was **OpenAI Codex, based on GPT‑6**. This attribution applies to the current revision. Model identifiers for earlier iterations were not recorded in the project source.

## Human verification on the SMK‑37 Pro

Victor Valentim reported installing the custom firmware on a physical **M‑VAVE SMK‑37 Pro** and confirmed that its USB audio output worked in use. For version **3.8**, he confirmed Bluetooth audio playback through his wireless headphones. In the same development sequence, he reported very low level at the analog P2 output even after restoring the factory V16. These are the reported observations from his unit. The 3.8 Bluetooth result is attributed to 3.8, and the report about the P2 jack remains an observation without a diagnosed cause.
