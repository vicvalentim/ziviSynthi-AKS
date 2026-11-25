/* Control-task state. The audio interrupt only consumes the published PCM. */
enum { VIEW_PLAY, VIEW_PRESETS, VIEW_SCALE, VIEW_RADIO, VIEW_ARP, VIEW_SEQ, VIEW_FADERS, VIEW_USER, VIEW_USB_ROUTES };
static unsigned usb_route_channel, usb_route_col;
static unsigned user_slot;
static bool bta_scene;
static unsigned bta_peer;
#if FM1B_BT_AUDIO
static uint32_t bta_memory[5120] PLAT_POOL;
#endif
static const char *user_message;
enum { USB_INPUT, BLE_PERIPHERAL_INPUT, BLE_CENTRAL_INPUT, NO_CLOCK = ZIVI_SMK_MIDI_PORTS };
enum { PAD_BASE_NOTE = 36, NO_NOTE = 255 };
static unsigned page, cursor, view, octave, peer, selected_preset, scale_root;
static enum ScaleMode scale_mode;
static bool scale_settings;
enum { PAD_LED_QUIET, PAD_LED_DIM, PAD_LED_OFF, PAD_LED_COUNT };
static unsigned pad_led_mode;
static unsigned arp_return_view;
static unsigned seq_cursor, seq_page, seq_entry_note = 60, seq_entry_velocity = 100;
static bool seq_entry, seq_follow, seq_live, ks_recording_seen;
static const char *seq_message;
static unsigned tx_channel, port_mode, previous_pitch = 8192, previous_mod, previous_pedal;
static unsigned synth_gain = 4096, usb_play_gain = 4096, usb_render_gain = 4096;
static uint32_t buttons_before, pads_before, peak_ticks, usb_session, clock_activity;
static uint32_t profile_blocks, profile_over_budget, profile_studio_peak;
static uint32_t profile_start_miss, profile_start_dma, profile_start_usb;
static unsigned profile_min_ready = ZIVI_SMK_SLOTS;
static uint64_t keys_before;
static uint8_t bank[ZIVI_SMK_MIDI_PORTS][16], key_output[ZIVI_SMK_KEYS];
static uint8_t fader_positions[4], pad_pressure[NPADS], pad_output[NPADS], daw_values[2][8];
static uint8_t output_notes[128];
static uint16_t thru_channels, thru_reset;
static uint32_t daw_pads, daw_button_down;
static const uint8_t daw_cc[NB] = {
    [B_FX] = 102, [B_SEL] = 103, [B_ENV] = 104, [B_LFO] = 105,
    [B_EDIT] = 106, [B_GLO] = 107, [B_HOME] = 108, [B_SAVE] = 109,
    [B_ARP] = 110, [B_OCTDN] = 111, [B_OCTUP] = 112,
    [B_KBANK] = 113, [B_FBANK] = 114, [B_REC] = 115,
    [B_LEFT] = 116, [B_RIGHT] = 117, [B_SEQ] = 118, [B_PLAY] = 119,
};
static int daw_master_delta;
enum { DAW_PADS, DAW_KNOBS, DAW_FADERS };
static unsigned daw_scene;
static uint8_t daw_fader_values[2][4], daw_fader_seen[2];
static const uint8_t default_midi_cc[] = {
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    70, 71, 72, 73, 76, 77, 78, 79
};
static uint8_t midi_cc[sizeof default_midi_cc];
static bool daw_mode, daw_playing, daw_fader_bank, faders_started;
static bool daw_knob_bank, daw_pad_bank, daw_bank_chord, daw_knob_shift;
static bool daw_knobs_relative;
static const uint32_t daw_mode_mask = (1u << B_GLO) | (1u << B_HOME);
static const uint32_t daw_bank_mask = (1u << B_KBANK) | (1u << B_FBANK);
static bool usb_thru, panel_attack, web_attack;
static uint32_t web_attack_ms;
static bool ble_peripheral, ble_central, ks_notes, clock_out, running_clock, factory_phrase;
static unsigned clock_source = NO_CLOCK, ks_output_note = NO_NOTE;
static uint32_t clock_prior, clock_phase;
static SmkMidiClock midi_clock;
static uint32_t sync_position, sync_beat_ms;
static unsigned ks_clock_reference = 120;
static bool sync_running, sync_steps, sync_arp_only;
static volatile bool audio_live;
_Static_assert(APP_BLOCK == MASTER_FRAMES, "Master limiter block size");
static MasterLimiter master_limiter = {.gain = UNITY_Q12, .limit = UNITY_Q12};
static unsigned stick_range[2] = {4096, 4096};
static float stick_position[2];

static void stick_set(unsigned axis, float position)
{
    stick_position[axis] = fmax(-1.0f, fmin(1.0f, position));
    float value = stick_position[axis] * (stick_range[axis] * (1.0f / 4096.0f));
    if (axis) synth_stick_y(&zivi, value);
    else synth_stick_x(&zivi, value);
}

static void stick_range_set(unsigned axis, unsigned range)
{
    stick_range[axis] = range;
    stick_set(axis, stick_position[axis]);
    zivi_dirty = 1;
}

static void profile_reset(void)
{
    peak_ticks = profile_blocks = profile_over_budget = profile_studio_peak = 0;
    profile_min_ready = ZIVI_SMK_SLOTS;
    profile_start_miss = zivi_device.underruns;
    profile_start_dma = plat_xruns();
    profile_start_usb = usb_input_dropped;
}

static void aks_profile_set(bool enabled)
{
    if (zivi.modules.aks_profile == enabled) return;
    modules_set_profile(&zivi.modules, zivi.rate, enabled);
    zivi_device.patch_transition = true;
    zivi_dirty = 1;
}

static void send_to(unsigned targets, unsigned status, unsigned a, unsigned b)
{
    unsigned cin = status >= 0xf8 ? 15u : status == 0xf2 ? 3u : status >> 4;
    plat_midi_send(cin | (status << 8) | (a << 16) | (b << 24), targets);
}

static void send(unsigned status, unsigned a, unsigned b)
{
    static const unsigned routes[] = {TO_USB | TO_BLE | TO_TRS, TO_TRS, TO_USB, TO_BLE};
    send_to(routes[port_mode], status, a, b);
}

static void note_out(unsigned status, unsigned a, unsigned b)
{
    send(status | tx_channel, a, b);
}

static void output_press(unsigned note, unsigned velocity)
{
    if (output_notes[note]++ == 0) note_out(0x90, note, velocity);
}

static void output_release(unsigned note)
{
    if (output_notes[note] && --output_notes[note] == 0) note_out(0x80, note, 0);
}

static void release_outputs(void)
{
    memset(key_output, NO_NOTE, sizeof key_output);
    memset(output_notes, 0, sizeof output_notes);
    daw_pads = 0;
    ks_output_note = NO_NOTE;
    note_out(0xb0, 64, 0);
    note_out(0xb0, 123, 0);
    for (unsigned i = 0; i < NB; ++i)
        if (daw_button_down & (1u << i)) note_out(0xb0, daw_cc[i], 0);
    daw_button_down = 0;
    if (daw_playing || running_clock) send(0xfc, 0, 0);
    daw_playing = running_clock = false;
}

static void controls_init(void)
{
    memset(key_output, NO_NOTE, sizeof key_output);
    memset(daw_values, 64, sizeof daw_values);
    memcpy(midi_cc, default_midi_cc, sizeof midi_cc);
    int pitch = plat_wheel(PLAT_WHEEL_PITCH);
    previous_pitch = (unsigned)(pitch + 8192);
    previous_mod = (unsigned)plat_wheel(PLAT_WHEEL_MOD);
    int mod = (int)previous_mod - 64;
    stick_set(0, pitch / (pitch >= 0 ? 8191.0f : 8192.0f));
    stick_set(1, mod / (mod >= 0 ? 63.0f : 64.0f));
    octave = 4;
    view = VIEW_PLAY;
    selected_preset = NO_NOTE;
}

static unsigned bank_start(unsigned p) { return synth_factory_bank(p)->start; }
static unsigned bank_size(unsigned p) { return synth_factory_bank(p)->size; }

static void reveal_factory(unsigned index)
{
    for (page = 0; index >= bank_start(page) + bank_size(page); ++page) {}
    cursor = index - bank_start(page);
    selected_preset = index;
}

static void import_ks(void)
{
    if (synth_seq_from_ks(&zivi)) {
        seq_cursor = seq_page = 0;
        seq_entry = seq_follow = false;
        seq_message = "FRASE KS CARREGADA";
    } else {
        seq_message = "SEM FRASE EDITAVEL";
    }
    zivi_dirty = 1;
}

static void load_factory(unsigned index)
{
    if (index >= synth_factory_count()) return;
    ks_stop(&zivi.ks);
    bool previous_phrase = factory_phrase;
    zivi_load_preset(4 + (int)index);
    if (index >= FACTORY_PROGRAMS && !usb_matrix) usb_matrix_set(true);
    factory_phrase = synth_factory_has_phrase(index);
    if (factory_phrase) import_ks();
    if (previous_phrase && !factory_phrase) {
        zivi.ks.count = 0;
        zivi.ks.length = 0;
    }
    if (daw_mode) zivi_device.muted = zivi_muted = true;
    reveal_factory(index);
}

static unsigned sync_bpm(void)
{
    return midi_clock.locked ? (640000u + midi_clock.period_q8 / 2u) / midi_clock.period_q8 : 0;
}

static void stop_sync(void)
{
    if (sync_running && !daw_mode) {
        if (sync_steps && !zivi.sequencer.recording) synth_seq_play(&zivi, false);
        if (!sync_steps && !sync_arp_only && zivi.ks.mode == KS_PLAYING) ks_stop(&zivi.ks);
    }
    sync_running = midi_clock.pending_start = false;
    zivi_dirty = 1;
}

static void reset_clock(void)
{
    stop_sync();
    midi_clock = (SmkMidiClock){.external = midi_clock.external};
    clock_source = NO_CLOCK;
    sync_position = 0;
    zivi.ks.clock_speed_q8 = 0;
    synth_sync_tempo(&zivi, 0);
}

static void expire_clock(uint32_t now)
{
    if (clock_source == NO_CLOCK || now - clock_activity <= 500) return;
    stop_sync();
    midi_clock.seen = midi_clock.locked = false;
    clock_source = NO_CLOCK;
    zivi.ks.clock_speed_q8 = 0;
    synth_sync_tempo(&zivi, 0);
}

static void sync_start(void)
{
    sync_running = true;
    if (daw_mode) return;
    if (sync_steps) {
        unsigned bpm = zivi.sequencer.clock_bpm ? zivi.sequencer.clock_bpm : zivi.sequencer.bpm;
        uint32_t beat = (uint32_t)zivi.rate * 60u / bpm;
        if (!zivi.sequencer.recording)
            synth_seq_seek(&zivi, (sync_position / 24u) * beat + (sync_position % 24u) * beat / 24u);
        seq_follow = true;
    } else if (!sync_arp_only && zivi.ks.mode != KS_RECORDING) {
        uint32_t beat = zivi.ks.rate * 60u / ks_clock_reference;
        uint32_t frame = (sync_position / 24u) * beat + (sync_position % 24u) * beat / 24u;
        ks_play_from(&zivi.ks, frame);
    }
    if (view == VIEW_ARP && sync_position == 0)
        zivi.arp.phase = zivi.arp.index = zivi.arp.velocity = 0;
}

static void disconnect_input(unsigned source)
{
    zivi_smk_midi_port_disconnect(&zivi_device, source);
    memset(bank[source], 0, sizeof bank[source]);
    if (clock_source == source) reset_clock();
    if (source == USB_INPUT) {
        thru_reset |= thru_channels;
        thru_channels = 0;
        web_attack = false;
    }
}

static void sync_scene(void)
{
    sync_steps = view == VIEW_SEQ || zivi.sequencer.playing;
    sync_arp_only = view == VIEW_ARP && !sync_steps;
}

static void remote_transport(unsigned source, unsigned status, unsigned a, unsigned b, uint32_t now)
{
    if (status == 0xff) {
        disconnect_input(source);
        return;
    }
    if (!midi_clock.external) return;
    bool advances = status == 0xfa || status == 0xfb || status == 0xf8 || status == 0xf2;
    if (advances && clock_source == NO_CLOCK) clock_source = source;
    if (clock_source != source || (!advances && status != 0xfc)) return;
    if (advances) clock_activity = now;
    switch (status) {
    case 0xfa:
        sync_position = 0;
        sync_scene();
        midi_clock.pending_start = true;
        break;
    case 0xfb:
        if (!sync_position) sync_scene();
        midi_clock.pending_start = true;
        break;
    case 0xfc:
        stop_sync();
        break;
    case 0xf2:
        sync_position = (a | (b << 7)) * 6u;
        if (!sync_running) sync_scene();
        if (sync_running) midi_clock.pending_start = true;
        break;
    case 0xf8:
        if (!midi_clock_measure(&midi_clock, now)) break;
        synth_sync_tempo(&zivi, sync_bpm());
        if (midi_clock.locked) {
            /* Position uses hundredths of a frame; the extra eight bits keep
             * the ratio between the MIDI clock and KS reference fractional. */
            float speed = (2500.0f * 65536.0f * 100.0f) / (midi_clock.period_q8 * ks_clock_reference);
            zivi.ks.clock_speed_q8 = (unsigned)(speed + 0.5f);
        }
        if (midi_clock.pending_start) {
            midi_clock.pending_start = false;
            sync_start();
            zivi_dirty = 1;
        }
        if (sync_running) {
            if (sync_position % 24u == 0) sync_beat_ms = now;
            ++sync_position;
        }
        break;
    }
}

static void poll_midi(void)
{
    uint32_t now = plat_ms(), usb = plat_usb_session();
    bool peripheral = plat_ble_linked() != 0;
    bool central = plat_ble_device() == BLE_DEV_READY;
    if (usb_session && usb_session != usb) disconnect_input(USB_INPUT);
    if (ble_peripheral && !peripheral) disconnect_input(BLE_PERIPHERAL_INPUT);
    if (ble_central && !central) disconnect_input(BLE_CENTRAL_INPUT);
    usb_session = usb;
    ble_peripheral = peripheral;
    ble_central = central;
    expire_clock(now);
    uint32_t packet;
    int port;
    for (unsigned i = 0; i < 32 && plat_midi_in(&packet, &port); ++i) {
        if (port != PORT_USB && port != PORT_BLE) continue;
        unsigned source = port == PORT_USB ? USB_INPUT :
            plat_midi_in_from() == TO_BLE_DEV ? BLE_CENTRAL_INPUT : BLE_PERIPHERAL_INPUT;
        unsigned status = (packet >> 8) & 255u;
        unsigned a = (packet >> 16) & 255u, b = packet >> 24;
        if (a >= 128 || b >= 128) continue;
        if (status >= 0xf0) {
            uint32_t arrival = plat_midi_in_t24();
            uint32_t age = (plat_ticks24() - arrival) / (1000u * PLAT_TICKS_PER_US);
            if (age <= 500) remote_transport(source, status, a, b, now - age);
            continue;
        }
        unsigned ch = status & 15u, type = status & 0xf0;
        if (usb_thru && source == USB_INPUT && !(thru_reset & (1u << ch))) {
            send_to(TO_TRS, status, a, b);
            thru_channels |= (uint16_t)(1u << ch);
        }
        if (zivi.midi_channel && ch + 1 != zivi.midi_channel) continue;
        if (type == 0xc0) {
            unsigned current = bank[source][ch];
            if (!current) load_factory(a);
            else if (current <= synth_factory_bank_count() && a < bank_size(current - 1))
                load_factory(bank_start(current - 1) + a);
        } else if (type == 0xb0 && a == 0) {
            bank[source][ch] = (uint8_t)b;
        } else if (type == 0xb0 && !daw_mode) {
            float value = b / 127.0f;
            switch (a) {
            case 10: synth_module_control(&zivi, M4_OUT1_PAN, 2.0f * value - 1.0f); break;
            case 16:
            case 17: stick_set(a - 16u, ((int)b - 64) / (b >= 64 ? 63.0f : 64.0f)); break;
            case 71: synth_module_control(&zivi, M4_RESPONSE, value); break;
            case 72: synth_module_control(&zivi, M4_DECAY_MS, 8.0f * dsp_exp2(8.0f * value)); break;
            case 73: synth_module_control(&zivi, M4_ATTACK_MS, 0.5f * dsp_exp2(11.0f * value)); break;
            case 74: synth_patch_controls(&zivi, zivi.mix, zivi.lfo, 25.0f * dsp_exp2(8.6f * value)); break;
            case 91: synth_module_control(&zivi, M4_REVERB_MIX, value); break;
            default:
                zivi_smk_midi_port(&zivi_device, source, status, a, b);
                break;
            }
            zivi_dirty = 1;
        } else if (!daw_mode && zivi_smk_midi_port(&zivi_device, source, status, a, b)) {
            if (type == 0x90 && b) zivi_device.muted = zivi_muted = false;
        }
    }
}

static void local_note(unsigned key, unsigned velocity)
{
    unsigned note = velocity ? synth_scale_note(12 * octave + key, scale_root, scale_mode) : key_output[key];
    if (velocity && view == VIEW_SEQ && !daw_mode) {
        seq_entry_note = note;
        seq_entry_velocity = velocity;
        if (seq_entry) {
            synth_seq_set(&zivi, seq_cursor, note, velocity);
            if (!(pads_before & (1u << (seq_cursor % NPADS))))
                seq_cursor = (seq_cursor + 1u) % zivi.sequencer.length;
            seq_page = seq_cursor / NPADS;
        }
        seq_message = NULL;
        zivi_dirty = 1;
    }
    if (!daw_mode) zivi_smk_key_note(&zivi_device, key, velocity, note);
    if (velocity) output_press(note, velocity);
    else if (note < 128) output_release(note);
    key_output[key] = velocity ? (uint8_t)note : NO_NOTE;
    if (velocity && !daw_mode) zivi_device.muted = zivi_muted = false;
}

static void poll_keys(void)
{
    uint64_t keys = plat_keys37();
    uint64_t strikes = plat_keys37_pressed() | (keys & ~keys_before) | plat_keys37_retrig();
    uint64_t releases = (keys_before | strikes) & ~keys;
    for (unsigned k = 0; k < ZIVI_SMK_KEYS && (strikes || releases); ++k) {
        uint64_t bit = UINT64_C(1) << k;
        if (strikes & bit) {
            if (key_output[k] < 128) local_note(k, 0);
            local_note(k, (unsigned)plat_key_vel(k));
            strikes &= ~bit;
        }
        if (releases & bit) {
            local_note(k, 0);
            releases &= ~bit;
        }
    }
    keys_before = keys;
}

static void matrix_pad_position(unsigned pad, unsigned *row, unsigned *col)
{
    *row = zivi_row / MATRIX_TILE * MATRIX_TILE + pad / MATRIX_TILE;
    *col = zivi_col / MATRIX_TILE * MATRIX_TILE + pad % MATRIX_TILE;
}

static unsigned seq_visible_page(void)
{
    const StepSequencer *seq = &zivi.sequencer;
    return seq->playing && seq_follow ? seq->index / NPADS : seq_page;
}

static void set_scale(unsigned root, enum ScaleMode mode)
{
    scale_root = root;
    scale_mode = mode;
    synth_arp_scale(&zivi, root, mode);
    zivi_dirty = 1;
}

static void seq_toggle_record(void)
{
    if (zivi.sequencer.recording) {
        synth_seq_play(&zivi, true);
    } else {
        synth_seq_record(&zivi);
        zivi_device.muted = zivi_muted = false;
    }
    seq_entry = false;
    seq_follow = true;
    seq_message = NULL;
    zivi_dirty = 1;
}

static void toggle_seq_step(void)
{
    const SeqStep *step = &zivi.sequencer.steps[seq_cursor];
    unsigned note = synth_scale_note(seq_entry_note, scale_root, scale_mode);
    synth_seq_set(&zivi, seq_cursor, step->velocity ? step->note : note, step->velocity ? 0 : seq_entry_velocity);
    seq_message = NULL;
    zivi_dirty = 1;
}

static void usb_route_cycle(unsigned col)
{
    if (!synth_pin_supported(SRC_INPUT1, col)) return;
    unsigned amount = (zivi.usb_routes[usb_route_channel][col] + 1) % 3;
    synth_usb_route(&zivi, usb_route_channel, col, (enum Pin)amount);
    if (amount && !usb_matrix) usb_matrix_set(true);
    zivi_device.muted = zivi_muted = false;
    zivi_preset = -1;
    zivi_dirty = 1;
}

static void poll_pads(unsigned pads, unsigned hits)
{
    unsigned releases = (pads_before | hits) & ~pads;
    pads_before = pads;
    if (daw_mode) {
        for (unsigned p = 0; p < NPADS; ++p) {
            unsigned bit = 1u << p;
            unsigned note = pad_output[p];
            if (hits & bit) {
                if (daw_pads & bit) output_release(note);
                note = PAD_BASE_NOTE + (daw_pad_bank ? NPADS : 0u) + p;
                pad_output[p] = (uint8_t)note;
                output_press(note, (unsigned)clamp(plat_pad_vel(p), 1, 127));
                daw_pads |= bit;
                pad_pressure[p] = 0;
            }
            if ((releases & bit) && (daw_pads & bit)) {
                output_release(note);
                daw_pads &= ~bit;
            } else if ((pads & bit) && (daw_pads & bit)) {
                unsigned pressure = (unsigned)plat_pad_press(p);
                if (pressure != pad_pressure[p]) {
                    note_out(0xa0, note, pressure);
                    pad_pressure[p] = (uint8_t)pressure;
                }
            }
        }
    } else for (unsigned p = 0; p < NPADS && hits; ++p) {
        unsigned bit = 1u << p;
        if (!(hits & bit)) continue;
        hits &= ~bit;
        if (view == VIEW_PRESETS && p < bank_size(page)) {
            load_factory(bank_start(page) + p);
        } else if (view == VIEW_USB_ROUTES) {
            usb_route_col = p;
            usb_route_cycle(p);
        } else if (view == VIEW_USER && p < APP_USER_SLOTS) {
            user_slot = p;
            user_message = NULL;
        } else if (view == VIEW_SEQ && !zivi.sequencer.recording) {
            unsigned index = seq_visible_page() * NPADS + p;
            if (index < zivi.sequencer.length) {
                seq_cursor = index;
                seq_page = index / NPADS;
                if (!seq_entry) toggle_seq_step();
                zivi_dirty = 1;
            }
        } else if (view == VIEW_SCALE && !scale_settings && p < 12) {
            set_scale(p, scale_mode);
        } else if (view == VIEW_ARP && p < ARP_DIVISION_COUNT) {
            const Arp *arp = &zivi.arp;
            if (!zivi.sequencer.recording)
                synth_arp(&zivi, arp->enabled, arp->bpm, arp->gate, arp->order, (enum ArpDivision)p);
            zivi_dirty = 1;
        } else if (view == VIEW_ARP && (p == 8 || p == 9)) {
            const Arp *arp = &zivi.arp;
            synth_arp_modes(&zivi, arp->hold != (p == 8), arp->free_run != (p == 9), arp->chance);
            if (arp->free_run && arp->enabled) zivi_device.muted = zivi_muted = false;
            zivi_dirty = 1;
        } else if (view == VIEW_PLAY && zivi_module_page && !zivi_scope && p < RACK_COUNT) {
            rack_select(p);
        } else if (view == VIEW_PLAY && !zivi_ks_page && !zivi_module_page) {
            matrix_pad_position(p, &zivi_row, &zivi_col);
            if (synth_toggle_pin(&zivi, zivi_row, zivi_col, zivi_pin_type)) zivi_preset = -1;
            zivi_zoom = true;
            zivi_dirty = 1;
        }
    }
}

static void poll_faders(uint32_t now)
{
    if (!faders_started && now < 250) return;
    for (unsigned i = 0; i < 4; ++i) {
        int position = clamp(plat_fader(i), 0, 127);
        if (!faders_started) {
            fader_positions[i] = (uint8_t)position;
            continue;
        }
        if (__builtin_abs(position - (int)fader_positions[i]) < 2 &&
            (position == fader_positions[i] || (position != 0 && position != 127))) continue;
        fader_positions[i] = (uint8_t)position;
        if (daw_mode) {
            daw_fader_values[daw_fader_bank][i] = (uint8_t)position;
            daw_fader_seen[daw_fader_bank] |= 1u << i;
            note_out(0xb0, midi_cc[16u + 4u * daw_fader_bank + i], (unsigned)position);
        } else {
            switch (i) {
            case 0: synth_gain = ((unsigned)position * 4096u + 63u) / 127u; break;
            case 1:
                usb_play_gain = ((unsigned)position * 4096u + 63u) / 127u;
                break;
            case 2:
                synth_patch_controls(&zivi, zivi.mix, zivi.lfo, 55.0f * dsp_exp2(8.0f * position / 127.0f));
                zivi_preset = -1;
                break;
            case 3:
                synth_module_control(&zivi, M4_REVERB_MIX, position / 127.0f);
                zivi_preset = -1;
                break;
            }
        }
        zivi_dirty = 1;
    }
    faders_started = true;
}

static void service_manual_attack(uint32_t now)
{
    if (web_attack && now - web_attack_ms >= 750u) web_attack = false;
    bool hold = !daw_mode && (panel_attack || previous_pedal || web_attack);
    synth_manual_hold(&zivi, hold);
    if (hold && zivi.modules.manual_attack) zivi_device.muted = zivi_muted = false;
}

static void poll_wheels(void)
{
    unsigned pitch = (unsigned)(plat_wheel(PLAT_WHEEL_PITCH) + 8192);
    unsigned mod = (unsigned)plat_wheel(PLAT_WHEEL_MOD), pedal = (unsigned)plat_pedal();
    if (pitch != previous_pitch) {
        int x = (int)pitch - 8192;
        stick_set(0, x / (x >= 0 ? 8191.0f : 8192.0f));
        note_out(0xe0, pitch & 127u, pitch >> 7);
        previous_pitch = pitch;
    }
    if (mod != previous_mod) {
        int y = (int)mod - 64;
        stick_set(1, y / (y >= 0 ? 63.0f : 64.0f));
        note_out(0xb0, 1, mod);
        previous_mod = mod;
    }
    if (pedal != previous_pedal) {
        note_out(0xb0, 64, pedal ? 127 : 0);
        previous_pedal = pedal;
    }
}

static void settings_buttons(uint32_t pressed)
{
    int tx = plat_enc(EN_K1), rx = plat_enc(EN_K2);
    if (plat_buttons() & (1u << B_KBANK)) {
        if (tx) stick_range_set(0, (unsigned)clamp((int)stick_range[0] + clamp(tx, -64, 64) * 64, 0, 4096));
        if (rx) stick_range_set(1, (unsigned)clamp((int)stick_range[1] + clamp(rx, -64, 64) * 64, 0, 4096));
        tx = rx = 0;
    }
    unsigned channel = (unsigned)clamp((int)tx_channel + tx, 0, 15);
    unsigned route = (unsigned)clamp((int)port_mode + plat_enc(EN_K3), 0, 3);
    if (channel != tx_channel || route != port_mode) release_outputs();
    tx_channel = channel;
    port_mode = route;
    if (rx) {
        zivi_smk_midi_disconnect(&zivi_device);
        zivi.midi_channel = (uint8_t)clamp((int)zivi.midi_channel + rx, 0, 16);
    }
    int usb = plat_enc(EN_K4);
    if (usb) {
        usb_play_gain = (unsigned)clamp((int)usb_play_gain + usb * 64, 0, 4096);
    }
    int clock = plat_enc(EN_PRESET), notes = plat_enc(EN_SELECT), ticks = plat_enc(EN_ALGO);
    if (clock && (plat_buttons() & (1u << B_KBANK))) {
        ks_clock_reference = (unsigned)clamp((int)ks_clock_reference + clock % 301, 30, 300);
        clock = 0;
    }
    if (clock && midi_clock.external != (clock > 0)) {
        reset_clock();
        midi_clock.external = clock > 0;
    }
    if (notes) ks_notes = notes > 0;
    if (ticks) clock_out = ticks > 0;
    if (pressed & (1u << B_EDIT)) profile_reset();
    if (pressed & (1u << B_FX)) {
        if (plat_buttons() & (1u << B_KBANK)) aks_profile_set(!zivi.modules.aks_profile);
        else if (!daw_mode) view = VIEW_USB_ROUTES;
        else {
            daw_knobs_relative = !daw_knobs_relative;
            zivi_dirty = 1;
        }
    }
    if (pressed & (1u << B_REC)) pad_led_mode = (pad_led_mode + 1u) % PAD_LED_COUNT;
    if (pressed & (1u << B_LFO)) {
        usb_matrix_set(!usb_matrix);
        if (usb_matrix && !daw_mode) zivi_device.muted = zivi_muted = false;
    }
    if (pressed & (1u << B_FBANK)) {
        usb_thru = !usb_thru;
        if (!usb_thru) {
            thru_reset |= thru_channels;
            thru_channels = 0;
        }
    }
}

static void scale_buttons(uint32_t pressed)
{
    if (pressed & ((1u << B_LEFT) | (1u << B_RIGHT))) scale_settings = !scale_settings;
    if (scale_settings) {
        settings_buttons(pressed);
        return;
    }
    int root = plat_enc(EN_K1), mode = plat_enc(EN_K2);
    mode = (mode % SCALE_COUNT + 8 * (plat_enc(EN_K3) % SCALE_COUNT)) % SCALE_COUNT;
    if (pressed & (1u << B_EDIT)) set_scale(0, SCALE_CHROMATIC);
    else if (root || mode)
        set_scale((unsigned)((int)scale_root + root % 12 + 12) % 12,
                  (enum ScaleMode)(((int)scale_mode + mode + SCALE_COUNT) % SCALE_COUNT));
}

static void arp_buttons(uint32_t pressed)
{
    int bpm = plat_enc(EN_K1), order = plat_enc(EN_K2), gate = plat_enc(EN_K3);
    int division = plat_enc(EN_K4), chance = plat_enc(EN_PRESET);
    const Arp *arp = &zivi.arp;
    bool toggle = pressed & (1u << B_EDIT);
    if (!zivi.sequencer.recording && (bpm || order || gate || division || toggle)) {
        synth_arp(&zivi, arp->enabled != toggle, (unsigned)clamp((int)arp->bpm + bpm, 30, 300),
                  (unsigned)clamp((int)arp->gate + gate, 10, 95),
                  (enum ArpOrder)(((int)arp->order + order % ARP_ORDER_COUNT + ARP_ORDER_COUNT) % ARP_ORDER_COUNT),
                  (enum ArpDivision)(((int)arp->division + division % ARP_DIVISION_COUNT + ARP_DIVISION_COUNT) % ARP_DIVISION_COUNT));
        zivi_dirty = 1;
    }
    if (chance) {
        synth_arp_modes(&zivi, arp->hold, arp->free_run, (unsigned)clamp((int)arp->chance + chance, 0, 100));
        zivi_dirty = 1;
    }
    if (arp->free_run && arp->enabled) zivi_device.muted = zivi_muted = false;
    if (pressed & (1u << B_REC)) {
        if (!zivi.sequencer.recording && !arp->enabled)
            synth_arp(&zivi, true, arp->bpm, arp->gate, arp->order, arp->division);
        seq_toggle_record();
    }
}

static void seq_buttons(uint32_t pressed)
{
    StepSequencer *seq = &zivi.sequencer;
    int length = plat_enc(EN_K1), pitch = plat_enc(EN_K2), velocity = plat_enc(EN_K3);
    int bpm = plat_enc(EN_K4), cursor_delta = plat_enc(EN_PRESET);
    if (seq->recording) {
        if (pressed & (1u << B_PLAY)) synth_seq_play(&zivi, false);
        else if (pressed & (1u << B_REC)) seq_toggle_record();
        if (pressed) zivi_dirty = 1;
        return;
    }
    if (length || bpm) {
        synth_seq_timing(&zivi, (unsigned)clamp((int)seq->length + length, 1, SEQ_STEPS),
                         (unsigned)clamp((int)seq->bpm + bpm, 30, 300));
        if (seq_cursor >= seq->length) seq_cursor = seq->length - 1u;
        seq_page = seq_cursor / NPADS;
    }
    if (cursor_delta) {
        seq_follow = false;
        seq_cursor = (unsigned)clamp((int)seq_cursor + cursor_delta, 0, (int)seq->length - 1);
        seq_page = seq_cursor / NPADS;
    }
    if (pressed & ((1u << B_LEFT) | (1u << B_RIGHT))) {
        int delta = (pressed & (1u << B_RIGHT)) ? 1 : -1;
        seq_page = (unsigned)clamp((int)seq_visible_page() + delta, 0, ((int)seq->length - 1) / NPADS);
        seq_follow = false;
        seq_cursor = seq_page * NPADS;
    }
    if (pitch || velocity) {
        const SeqStep *step = &seq->steps[seq_cursor];
        seq_entry_note = (unsigned)clamp((int)(step->velocity ? step->note : seq_entry_note) + pitch, 0, 127);
        seq_entry_velocity = (unsigned)clamp((int)(step->velocity ? step->velocity : seq_entry_velocity) + velocity, 1, 127);
        synth_seq_set(&zivi, seq_cursor, seq_entry_note, seq_entry_velocity);
    }
    if (length || pitch || velocity || bpm || cursor_delta || pressed) {
        seq_message = NULL;
        zivi_dirty = 1;
    }
    if (pressed & (1u << B_FBANK)) {
        seq_live = !seq_live;
        seq_entry = false;
    }
    if (pressed & (1u << B_REC)) {
        if (seq_live || seq->playing) seq_toggle_record();
        else {
            seq_entry = !seq_entry;
            if (seq_entry) seq_follow = false;
        }
    }
    if (pressed & (1u << B_FX)) import_ks();
    if (pressed & (1u << B_KBANK)) {
        bool copied = synth_seq_to_ks(&zivi);
        if (copied) {
            zivi.ks.speed = zivi.ks.sequence_spread = 100;
            zivi.ks.transpose = 0;
            zivi.ks.loop = true;
            zivi.ks.sequence_trigger = true;
            factory_phrase = false;
        }
        seq_message = copied ? "SEQ COPIADA PARA KS" : "SEQ INDISPONIVEL";
    }
    if (pressed & (1u << B_EDIT)) toggle_seq_step();
    if (pressed & (1u << B_PLAY)) {
        synth_seq_play(&zivi, !seq->playing);
        if (seq->playing) zivi_device.muted = zivi_muted = false;
        seq_follow = seq->playing && !seq_entry;
    }
}

static void radio_buttons(uint32_t pressed)
{
#if FM1B_BT_AUDIO
    if (pressed & (1u << B_RIGHT)) bta_scene = !bta_scene;
    if (bta_scene) {
        int turn = plat_enc(EN_SELECT);
        bta_peer = (unsigned)clamp((int)bta_peer + turn, 0, 7);
        if (turn) zivi_dirty = 1;
        if (pressed & (1u << B_PLAY)) {
            unsigned bytes = plat_bta_mem_bytes();
            if (bytes && bytes <= sizeof bta_memory && plat_bta_state() == PLAT_BTA_OFF)
                (void)plat_bta_mem(bta_memory);
            plat_bta_on(1);
            plat_bta_scan(1);
            bta_peer = 0;
        }
        if (pressed & (1u << B_EDIT)) {
            char name[17];
            if (plat_bta_found((int)bta_peer, name)) plat_bta_pick((int)bta_peer);
        }
        if (pressed & (1u << B_LEFT)) plat_bta_on(0);
        if ((pressed & (1u << B_FX)) && plat_bta_state() == PLAT_BTA_STREAMING)
            plat_bta_output(!plat_bta_out_device());
        return;
    }
#endif
    int power = plat_enc(EN_K1);
    if (power) plat_ble_enable(power > 0);
    int choice = plat_enc(EN_SELECT);
    peer = (unsigned)clamp((int)peer + choice, 0, 7);
    if (pressed & (1u << B_PLAY)) {
        if (plat_ble_state() == BLE_READY) {
            peer = 0;
            plat_ble_scan(1);
        } else plat_ble_start();
    }
    if (pressed & (1u << B_EDIT)) {
        char name[21];
        if (plat_ble_found((int)peer, name, NULL)) (void)plat_ble_connect((int)peer);
    }
    if (pressed & (1u << B_LEFT)) {
        plat_ble_scan(0);
        plat_ble_disconnect();
    }
}

static void daw_buttons(uint32_t changed, uint32_t buttons)
{
    for (unsigned i = 0; i < NB; ++i) {
        unsigned bit = 1u << i;
        if (changed & bit && ((buttons | daw_button_down) & bit))
            note_out(0xb0, daw_cc[i], buttons & bit ? 127 : 0);
    }
    daw_button_down = (daw_button_down & ~changed) | (changed & buttons);
    if (changed & buttons & (1u << B_PLAY) && view == VIEW_PLAY) {
        daw_playing = !daw_playing;
        send(daw_playing ? 0xfa : 0xfc, 0, 0);
    }
}

static unsigned daw_active_knob_bank(uint32_t buttons)
{
    return (buttons & (1u << B_KBANK)) && !daw_bank_chord ? 1u : daw_knob_bank;
}

static void daw_banks_and_knobs(uint32_t changed, uint32_t buttons)
{
    uint32_t released = changed & ~buttons;
    if ((buttons & daw_bank_mask) == daw_bank_mask && !daw_bank_chord) {
        daw_bank_chord = true;
        daw_pad_bank = !daw_pad_bank;
        daw_scene = DAW_PADS;
    }
    if (!daw_bank_chord && view == VIEW_PLAY) {
        if (released & (1u << B_KBANK)) {
            if (!daw_knob_shift) {
                daw_knob_bank = !daw_knob_bank;
                daw_scene = DAW_KNOBS;
            }
            daw_knob_shift = false;
        }
        if (released & (1u << B_FBANK)) {
            daw_fader_bank = !daw_fader_bank;
            daw_scene = DAW_FADERS;
        }
    }
    if (!(buttons & daw_bank_mask)) daw_bank_chord = false;
    if (view != VIEW_PLAY) return;
    static const int roles[] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_PRESET, EN_SELECT, EN_ALGO};
    unsigned b = daw_active_knob_bank(buttons);
    for (unsigned i = 0; i < 8; ++i) {
        int turn = i < 7 ? plat_enc(roles[i]) : daw_master_delta;
        if (turn && (buttons & (1u << B_KBANK))) {
            daw_knob_shift = true;
            daw_scene = DAW_KNOBS;
        }
        if (daw_knobs_relative) {
            if (turn) note_out(0xb0, midi_cc[b * 8u + i], (unsigned)clamp(turn, -64, 63) & 127u);
            continue;
        }
        unsigned value = (unsigned)clamp((int)daw_values[b][i] + turn, 0, 127);
        if (value != daw_values[b][i]) {
            note_out(0xb0, midi_cc[b * 8u + i], value);
            zivi_dirty = 1;
        }
        daw_values[b][i] = (uint8_t)value;
    }
    daw_master_delta = 0;
}

static void poll_panel(void)
{
    uint32_t buttons = plat_buttons(), changed = buttons ^ buttons_before;
    uint32_t pressed = changed & buttons;
    buttons_before = buttons;
    unsigned prior = view;
    if ((buttons & daw_mode_mask) == daw_mode_mask && (pressed & daw_mode_mask)) {
        app_panic();
        reset_clock();
        daw_mode = !daw_mode;
        view = VIEW_PLAY;
        pressed &= ~daw_mode_mask;
        changed &= ~daw_mode_mask;
        daw_knob_shift = daw_bank_chord = false;
        daw_scene = DAW_PADS;
        zivi_dirty = 1;
    }
    if (!daw_mode && (pressed & (1u << B_ENV))) app_panic();
    if (pressed & (1u << B_OCTDN)) octave = octave ? octave - 1 : 0;
    if (pressed & (1u << B_OCTUP)) octave = octave < 7 ? octave + 1 : 7;
    if (pressed & (1u << B_SEL)) {
        view = view == VIEW_SCALE ? VIEW_PLAY : VIEW_SCALE;
        if (view == VIEW_SCALE) scale_settings = daw_mode;
    }
    if (pressed & (1u << B_SAVE)) view = view == VIEW_RADIO ? VIEW_PLAY : VIEW_RADIO;
    if (!daw_mode) {
        if ((view == VIEW_PLAY || view == VIEW_FADERS) && (pressed & (1u << B_FBANK))) {
            view = view == VIEW_FADERS ? VIEW_PLAY : VIEW_FADERS;
            pressed &= ~(1u << B_FBANK);
        }
        if (pressed & (1u << B_SEQ)) {
            bool leaving = view == VIEW_SEQ;
            view = leaving ? VIEW_PLAY : VIEW_SEQ;
            if (leaving) zivi_module_page = zivi_ks_page = false;
            pressed &= ~(1u << B_SEQ);
        } else if (view == VIEW_SEQ && (pressed & (1u << B_LFO))) {
            view = VIEW_PLAY;
            zivi_module_page = false;
            zivi_ks_page = true;
            pressed &= ~(1u << B_LFO);
        } else if (pressed & (1u << B_ARP)) {
            if (view == VIEW_ARP) view = arp_return_view;
            else {
                arp_return_view = view;
                view = VIEW_ARP;
            }
        } else {
            if (pressed & (1u << B_HOME)) view = (view == VIEW_PRESETS || view == VIEW_USER) ? VIEW_PLAY : VIEW_PRESETS;
            if (view == VIEW_ARP && (pressed & (1u << B_GLO))) zivi_module_page = false;
            if ((pressed & (1u << B_GLO)) ||
                (view == VIEW_FADERS && (pressed & (1u << B_KBANK)))) view = VIEW_PLAY;
        }
    }
    if (view != VIEW_PLAY && zivi.ks.random_held) synth_ks_random(&zivi, false);

    if (daw_mode) {
        uint32_t controller_changes = changed;
        if (view == VIEW_SCALE && scale_settings && !(daw_button_down & (1u << B_FX)))
            controller_changes &= ~(1u << B_FX);
        daw_buttons(controller_changes, buttons);
        daw_banks_and_knobs(changed, buttons);
    }
    if (!daw_mode && (view == VIEW_PRESETS || view == VIEW_USER) && (pressed & (1u << B_KBANK))) {
        view = view == VIEW_PRESETS ? VIEW_USER : VIEW_PRESETS;
        user_message = NULL;
    }
    if (view == VIEW_USER) {
        int turn = plat_enc(EN_PRESET);
        if (turn) {
            user_slot = (unsigned)clamp((int)user_slot + turn, 0, APP_USER_SLOTS - 1);
            user_message = NULL;
            zivi_dirty = 1;
        }
        if (pressed & ((1u << B_REC) | (1u << B_EDIT))) {
            bool save = (pressed & (1u << B_REC)) != 0;
            unsigned result = user_slot_operation(user_slot, save, false);
            user_message = !result ? (save ? "GRAVADO NO SMK" : "CARREGADO") :
                           result == STUDIO_BUSY ? "PARE A EXECUCAO OU O USB" :
                           result == STUDIO_STORAGE ? "SLOT VAZIO OU PROTEGIDO" : "OPERACAO INDISPONIVEL";
            zivi_dirty = 1;
        }
    } else if (view == VIEW_PRESETS) {
        unsigned old_page = page;
        unsigned banks = synth_factory_bank_count();
        if (pressed & (1u << B_LEFT)) page = (page + banks - 1) % banks;
        if (pressed & (1u << B_RIGHT)) page = (page + 1) % banks;
        if (page != old_page) cursor = 0;
        int turn = plat_enc(EN_PRESET);
        cursor = (unsigned)clamp((int)cursor + turn, 0, (int)bank_size(page) - 1);
        if (pressed & (1u << B_EDIT)) load_factory(bank_start(page) + cursor);
        if (turn) zivi_dirty = 1;
    } else if (view == VIEW_USB_ROUTES) {
        int input = plat_enc(EN_K1), col = plat_enc(EN_PRESET);
        if (input) usb_route_channel = input > 0;
        usb_route_col = (unsigned)clamp((int)usb_route_col + col, 0, 15);
        if (pressed & (1u << B_EDIT)) usb_route_cycle(usb_route_col);
        if (pressed & (1u << B_FX)) {
            scale_settings = true;
            view = VIEW_SCALE;
        }
        if (input || col) zivi_dirty = 1;
    } else if (view == VIEW_SCALE) scale_buttons(pressed);
    else if (view == VIEW_RADIO) radio_buttons(pressed);
    else if (view == VIEW_ARP) arp_buttons(pressed);
    else if (view == VIEW_SEQ) seq_buttons(pressed);
    else if (!daw_mode && view == VIEW_PLAY) {
        const uint32_t reserved = (1u << B_HOME) | (1u << B_SAVE) | (1u << B_SEL) | (1u << B_ENV);
        panel_buttons = buttons & ~reserved;
        panel_pressed = pressed & ~reserved;
        if (pressed & (1u << B_KBANK)) panel_pressed |= 1u << B_ENV;
        if (zivi_ks_page && (pressed & ((1u << B_PLAY) | (1u << B_REC))) && zivi.sequencer.playing)
            synth_seq_play(&zivi, false);
        enum KsMode prior_ks = zivi.ks.mode;
        if (zivi_ks_page && (pressed & (1u << B_LEFT))) ks_stop(&zivi.ks);
        int previous = zivi_preset;
        if (!zivi_zoom && !zivi_ks_page && !zivi_module_page &&
            !(panel_pressed & ((1u << B_FX) | (1u << B_ENV) | (1u << B_GLO)))) {
            int turn = plat_enc(EN_K4);
            if (turn) {
                const int count = FACTORY_PROGRAMS;
                int start = selected_preset >= FACTORY_PROGRAMS ? (turn > 0 ? -1 : 0) : (int)selected_preset;
                load_factory((unsigned)((start + turn % count + 2 * count) % count));
                previous = zivi_preset;
            }
        }
        instrument_input();
        if (prior_ks == KS_RECORDING && zivi.ks.mode != KS_RECORDING) import_ks();
        if (zivi.ks.mode == KS_RECORDING) factory_phrase = false;
        if (zivi_preset != previous && zivi_preset >= 4) {
            unsigned index = (unsigned)zivi_preset - 4;
            factory_phrase = synth_factory_has_phrase(index);
            if (factory_phrase) import_ks();
            reveal_factory(index);
        }
        if (zivi.modules.manual_attack || zivi.ks.random_held) zivi_device.muted = zivi_muted = false;
    }
    if (view != VIEW_PLAY || !zivi_module_page) rack_k8_delta = 0;
    if (view != VIEW_PLAY) {
        for (int role = 0; role < NE; ++role) (void)plat_enc(role);
        daw_master_delta = 0;
    }
    panel_attack = !daw_mode && view == VIEW_PLAY && zivi_module_page && (buttons & (1u << B_REC));
    unsigned pads = plat_pads() & 0xffffu;
    unsigned hits = plat_pads_pressed() | (pads & ~pads_before);
    poll_pads(pads, hits & 0xffffu);
    poll_faders(plat_ms());
    poll_wheels();
    if (view != prior || changed || hits) zivi_dirty = 1;
}

static void service_transport(uint32_t now)
{
    if (thru_reset) {
        unsigned ch = (unsigned)__builtin_ctz(thru_reset);
        send_to(TO_TRS, 0xb0 | ch, 64, 0);
        send_to(TO_TRS, 0xb0 | ch, 123, 0);
        thru_reset &= (uint16_t)~(1u << ch);
    }
    bool ks_recording = zivi.ks.mode == KS_RECORDING;
    if (ks_recording_seen && !ks_recording && zivi.ks.limit != KS_LIMIT_NONE)
        import_ks();
    ks_recording_seen = ks_recording;
    bool steps = zivi.sequencer.playing;
    bool playing = (steps || zivi.ks.mode == KS_PLAYING) && !daw_mode;
    bool tick = playing && clock_out && !midi_clock.external;
    if (playing) zivi_device.muted = zivi_muted = false;
    if (tick != running_clock) {
        send(tick ? 0xfa : 0xfc, 0, 0);
        running_clock = tick;
        clock_phase = 0;
        clock_prior = now;
    }
    if (tick) {
        unsigned delta = now - clock_prior;
        clock_prior = now;
        unsigned bpm_x100 = steps ? zivi.sequencer.bpm * 100u : ks_clock_reference * zivi.ks.speed;
        clock_phase = delta > 250 ? 0 : clock_phase + delta * bpm_x100;
        for (unsigned i = 0; i < 3 && clock_phase >= 250000u; ++i, clock_phase -= 250000u)
            send(0xf8, 0, 0);
        clock_phase %= 250000u;
    }
    unsigned velocity = steps ? zivi.sequencer.velocity : zivi.ks.velocity;
    unsigned note = playing && ks_notes && velocity ? (steps ? zivi.sequencer.note :
        (unsigned)clamp(zivi.ks.note + zivi.ks.transpose, 0, 127)) : NO_NOTE;
    if (note == ks_output_note) return;
    if (ks_output_note < 128) output_release(ks_output_note);
    if (note < 128) output_press(note, velocity);
    ks_output_note = note;
}
