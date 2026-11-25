static uint32_t last_pad_light[NPADS], applied_pad_light[NPADS];

static uint32_t pad_colour(unsigned pad, bool beat)
{
    if (daw_mode) return daw_pads & (1u << pad) ? 0x408018 : daw_pad_bank ? 0x242044 : 0x101830;
    if (view == VIEW_USB_ROUTES) {
        unsigned amount = zivi.usb_routes[usb_route_channel][pad];
        return amount == PIN_FULL ? 0x985810 : amount == PIN_HALF ? 0x484848 : 0x142A40;
    }
    if (view == VIEW_USER)
        return pad < APP_USER_SLOTS ? (pad == user_slot ? 0xA85C0C : 0x183830) : 0;
    if (view == VIEW_PRESETS) {
        if (pad >= bank_size(page)) return 0;
        if (bank_start(page) + pad == selected_preset) return 0xA85C0C;
        return cursor == pad ? 0x236CA0 : 0x102B46;
    }
    if (view == VIEW_SEQ) {
        const StepSequencer *seq = &zivi.sequencer;
        unsigned index = (pad_led_mode == PAD_LED_DIM ? seq_visible_page() : seq_page) * NPADS + pad;
        if (index >= seq->length) return 0;
        if (pad_led_mode == PAD_LED_DIM && seq->playing && index == seq->index) return 0xA85C0C;
        if (index == seq_cursor) return 0x486C90;
        return seq->steps[index].velocity ? 0x236CA0 : 0x101830;
    }
    if (view == VIEW_SCALE && !scale_settings && pad < 12) {
        if (pad == scale_root) return 0xA85C0C;
        return synth_scale_note(60 + pad, scale_root, scale_mode) == 60 + pad ? 0x236CA0 : 0x101830;
    }
    if (pad == 15 && ((view == VIEW_SCALE && scale_settings) || view == VIEW_ARP) && midi_clock.external)
        return beat ? 0xA85C0C : midi_clock.locked ? 0x234820 : 0x101830;
    if (view == VIEW_ARP && pad < ARP_DIVISION_COUNT) {
        if (pad == (unsigned)zivi.arp.division)
            return pad_led_mode == PAD_LED_DIM && zivi.arp.velocity ? 0xA85C0C : 0x236CA0;
        return 0x101830;
    }
    if (view == VIEW_ARP && (pad == 8 || pad == 9))
        return (pad == 8 ? zivi.arp.hold : zivi.arp.free_run) ? 0xA85C0C : 0x101830;
    if (view == VIEW_FADERS && pad < 4) {
        static const uint32_t colours[] = {0x408018, 0x236CA0, 0xA85C0C, 0x785898};
        return colours[pad];
    }
    if (view == VIEW_PLAY && zivi_module_page && !zivi_scope && pad < RACK_COUNT) {
        static const uint32_t colours[] = {0x236CA0, 0x408018, 0x484848, 0x82500A, 0x702020};
        return pad == rack_page ? 0xA85C0C : colours[rack_panels[pad].hue];
    }
    if (view != VIEW_PLAY || zivi_module_page) return 0;
    if (zivi_ks_page) {
        if (zivi.ks.mode == KS_RECORDING && !zivi.ks.count) return pad ? 0x201410 : 0xA85C0C;
        if (zivi.ks.mode == KS_STOPPED || !zivi.ks.count) return 0;
        if (pad_led_mode != PAD_LED_DIM) return 0x101830;
        unsigned active = zivi_ks_progress * NPADS / 213u;
        return pad == active ? 0xA85C0C : pad < active ? 0x236CA0 : 0x101830;
    }
    unsigned row, col;
    matrix_pad_position(pad, &row, &col);
    if (!synth_pin_supported(row, col)) return 0;
    switch (zivi.matrix[row][col]) {
    case PIN_FULL: return 0x985810;
    case PIN_HALF: return 0x484848;
    default: return row == zivi_row && col == zivi_col ? 0x164C80 : 0x080E15;
    }
}

static void update_pad_lights(void)
{
    bool beat = pad_led_mode == PAD_LED_DIM && sync_running && plat_ms() - sync_beat_ms < 128u;
    for (unsigned pad = 0; pad < NPADS; ++pad) {
        uint32_t color = pad_led_mode == PAD_LED_OFF ? 0 : pad_colour(pad, beat);
        last_pad_light[pad] = color;
        uint32_t limited = (color >> 3) & 0x1f1f1fu;
        if (limited != applied_pad_light[pad]) {
            plat_pad_rgb((int)pad, limited);
            applied_pad_light[pad] = limited;
        }
    }
}

static void draw_presets(void)
{
    char label[32];
    zivi_header("PRESETS");
    cv_text(8, 34, &FONT_XS, synth_factory_bank(page)->label, INK);
    snprintf(label, sizeof label, "%02u / %02u", page + 1, synth_factory_bank_count());
    cv_text(231 - text_w(&FONT_T, label), 36, &FONT_T, label, GRAPHITE);
    for (unsigned pad = 0; pad < NPADS; ++pad) {
        int x = 8 + (int)(pad % 4) * 56, y = 53 + (int)(pad / 4) * 33;
        unsigned index = bank_start(page) + pad;
        bool available = pad < bank_size(page);
        bool active = available && selected_preset == index;
        bool focused = available && cursor == pad;
        if (!available) {
            cv_rect(x, y + 30, 51, 1, SILVER);
            continue;
        }
        uint16_t ink = active || focused ? WHITE : INK;
        cv_rect(x, y, 52, 31, active ? DARK_BLUE : focused ? BLACK : SILVER);
        if (focused) cv_rect(x, y, 52, 2, BLUE);
        if (active) cv_rect(x + 44, y + 4, 4, 4, AMBER);
        snprintf(label, sizeof label, "%02u", pad + 1);
        cv_text(x + 4, y + 1, &FONT_T, label, ink);
        const char *name = synth_factory_display(index);
        unsigned count = 0, space = 0;
        while (name[count] && count + 1 < sizeof label) {
            label[count] = name[count];
            label[count + 1] = 0;
            if (text_w(&FONT_T, label) > 44) break;
            if (name[count] == ' ') space = count;
            ++count;
        }
        bool wrapped = name[count] != 0;
        if (wrapped && space) count = space;
        label[count] = 0;
        cv_text(x + 4, y + 11, &FONT_T, label, ink);
        if (wrapped) {
            const char *rest = name + count + (name[count] == ' ');
            unsigned next = 0;
            while (rest[next] && next + 1 < sizeof label) {
                label[next] = rest[next];
                label[next + 1] = 0;
                if (text_w(&FONT_T, label) > 44) break;
                ++next;
            }
            label[next] = 0;
            cv_text(x + 4, y + 20, &FONT_T, label, ink);
        }
    }
    unsigned focus = bank_start(page) + cursor;
    cv_text(9, 186, &FONT_B, synth_factory_display(focus), INK);
    const char *status = selected_preset == focus ? zivi_preset < 0 ? "EDITADO" : "CARREGADO" : "EM FOCO";
    cv_text(9, 202, &FONT_T, status, GRAPHITE);
    zivi_footer_pair("PADS/PARA  CARREGA    < >  BANCOS", "KBANK  USER");
}

static void draw_user(void)
{
    char label[32];
    zivi_header("PRESETS USER");
    for (unsigned slot = 0; slot < APP_USER_SLOTS; ++slot) {
        int y = 44 + (int)slot * 36;
        cv_rect(10, y, 220, 29, slot == user_slot ? DARK_BLUE : SILVER);
        snprintf(label, sizeof label, "PAD %u    %s", slot + 1, user_slot_present(slot) ? "GRAVADO" : "VAZIO");
        cv_text(18, y + 6, &FONT_S, label, slot == user_slot ? WHITE : INK);
    }
    if (user_message) cv_text(10, 177, &FONT_XS, user_message, INK);
    zivi_footer_pair("REC  GRAVA    PARA  CARREGA", "KBANK  VOLTA");
}

static void draw_scale(void)
{
    static const char *const ports[] = {"ALL", "TRS", "USB", "BLE"};
    char line[48];
    zivi_header("MIDI / AUDIO");
    snprintf(line, sizeof line, "K1 TX %u   K2 RX %u", tx_channel + 1, (unsigned)zivi.midi_channel);
    cv_text(10, 37, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "K3 %s   K4 USB %u%%", ports[port_mode], usb_play_gain * 100u / 4096u);
    cv_text(10, 57, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "K5 %s   K6 KS %s   K7 CLK %s",
             midi_clock.external ? midi_clock.locked ? "LOCK" : "EXT" : "INT",
             ks_notes ? "ON" : "OFF", midi_clock.external ? "--" : clock_out ? "ON" : "OFF");
    cv_text(10, 77, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "K8 MASTER %u%%   F1 SYNTH %u%%", (unsigned)(plat_master() * 100u / 4096u),
             zivi_device.gain_q12 * 100u / 4096u);
    cv_text(10, 97, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "USB DROP %u", usb_input_dropped);
    cv_text(10, 114, &FONT_T, line, INK);
    snprintf(line, sizeof line, "FILA MIN %u   ATR %u", profile_min_ready == ZIVI_SMK_SLOTS ? 0 : profile_min_ready,
             (unsigned)profile_over_budget);
    cv_text(136, 114, &FONT_T, line, INK);
    snprintf(line, sizeof line, "MISS %u   DMA %u", zivi_device.underruns, (unsigned)plat_xruns());
    cv_text(10, 130, &FONT_T, line, INK);
    snprintf(line, sizeof line, "DSP %u US  IRQ %u%%", (unsigned)(peak_ticks / 24u), (unsigned)plat_cpu_pct());
    cv_text(112, 130, &FONT_T, line, INK);
    if (midi_clock.external) {
        static const char *const sources[] = {"USB", "BLE", "BLE DEV"};
        const char *source = clock_source < NO_CLOCK ? sources[clock_source] : "--";
        if (midi_clock.locked) snprintf(line, sizeof line, "%s %u BPM   %s", source, sync_bpm(), sync_running ? "PLAY" : "STOP");
        else snprintf(line, sizeof line, "%s   AGUARDA MIDI CLOCK", source);
        cv_text(10, 148, &FONT_T, line, INK);
    }
    snprintf(line, sizeof line, "KB+K5 KS %u", ks_clock_reference);
    cv_text(158, 148, &FONT_T, line, INK);
    snprintf(line, sizeof line, "REPEAT %s  FX %s", usb_matrix ? "MATRIZ" : "MONITOR",
             daw_mode ? daw_knobs_relative ? "K REL" : "K ABS" : "ROTAS");
    cv_text(10, 173, &FONT_XS, line, usb_matrix ? BLUE : INK);
    snprintf(line, sizeof line, "RANGE %u/%u  THRU %s  KB+FX %s", stick_range[0] * 100u / 4096u,
             stick_range[1] * 100u / 4096u, usb_thru ? "ON" : "OFF", zivi.modules.aks_profile ? "AKS" : "LEGADO");
    cv_text(10, 193, &FONT_T, line, INK);
    static const char *const led_modes[] = {"FIXO", "ANIM.", "OFF"};
    snprintf(line, sizeof line, "< SCALE   REC LED %s", led_modes[pad_led_mode]);
    zivi_footer(line);
}

static void draw_usb_routes(void)
{
    char line[30];
    zivi_header(usb_route_channel ? "USB R" : "USB L");
    for (unsigned col = 0; col < 16; ++col) {
        int x = 8 + (int)(col % 4) * 56, y = 41 + (int)(col / 4) * 32;
        unsigned amount = zivi.usb_routes[usb_route_channel][col];
        bool focus = col == usb_route_col;
        uint16_t colour = amount ? DARK_BLUE : focus ? BLACK : SILVER;
        cv_rect(x, y, 51, 27, colour);
        snprintf(line, sizeof line, "%c %s", (char)('A' + col), amount == PIN_FULL ? "100" : amount == PIN_HALF ? "50" : "--");
        cv_text(x + 5, y + 7, &FONT_T, line, amount || focus ? WHITE : INK);
    }
    snprintf(line, sizeof line, "K5   %s", synth_dest_name(usb_route_col));
    cv_text(10, 176, &FONT_XS, line, INK);
    zivi_footer("K1 L/R  PADS/PARA  FX VOLTA");
}

static void draw_radio(void)
{
    static const char *const states[] = {"OFF", "STARTING", "READY", "FAILED"};
    static const char *const central[] = {"IDLE", "SCAN", "CONNECT", "GATT", "READY"};
    char line[48], name[21];
    unsigned state = (unsigned)plat_ble_state(), link = (unsigned)plat_ble_device();
#if FM1B_BT_AUDIO
    if (bta_scene) {
        static const char *const audio_states[] = {
            "OFF", "BUSCANDO", "ENCONTRADO", "SEM DISPOSITIVO", "CONECTANDO", "CONECTADO",
            "CONFIGURANDO", "TRANSMITINDO", "RECUSADO", "SEM PAR", "FALHA", "PRONTO"
        };
        unsigned audio_state = (unsigned)plat_bta_state();
        zivi_header("BLUETOOTH AUDIO");
        snprintf(line, sizeof line, "A2DP   %s", audio_state < 12 ? audio_states[audio_state] : "-");
        cv_rect(8, 35, 224, 33, audio_state == PLAT_BTA_STREAMING ? DARK_BLUE : BLACK);
        cv_rect(8, 35, 3, 33, audio_state == PLAT_BTA_STREAMING ? AMBER : BLUE);
        cv_text(16, 43, &FONT_S, line, WHITE);
        if (plat_bta_found((int)bta_peer, name)) {
            snprintf(line, sizeof line, "K6 %u  %s", bta_peer + 1, name);
            cv_text(12, 83, &FONT_S, line, INK);
        }
        cv_text(12, 135, &FONT_XS, plat_bta_out_device() ? "FX SAIDA BT" : "FX SAIDA LOCAL", INK);
        cv_text(12, 165, &FONT_XS, "< DESLIGA   > BLE MIDI", INK);
        zivi_footer_pair("PLAY  BUSCA", "PARA  CONECTA");
        return;
    }
#endif
    zivi_header("BLUETOOTH MIDI");
    snprintf(line, sizeof line, "RADIO %s", state < 4 ? states[state] : "-");
    cv_text(12, 40, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "PERIPHERAL %s", plat_ble_linked() ? "LINK" : "-");
    cv_text(12, 63, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "CENTRAL %s", link < 5 ? central[link] : "-");
    cv_text(12, 86, &FONT_XS, line, INK);
    if (plat_ble_found((int)peer, name, NULL)) {
        snprintf(line, sizeof line, "%u  %s", peer + 1, name);
        cv_text(12, 113, &FONT_S, line, INK);
    }
    int passkey = plat_ble_passkey();
    if (passkey >= 0) {
        snprintf(line, sizeof line, "%06u", (unsigned)passkey);
        cv_text(12, 150, &FONT_B, line, INK);
    }
    zivi_footer_pair("PLAY  BUSCA   PARA  CONECTA", ">  AUDIO");
}

static void draw_scale_notes(void)
{
    static const char *const names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *const modes[SCALE_COUNT] = {
#define SCALE(symbol, mask, name, lcd, group) [SCALE_##symbol] = lcd,
#include "scales.def"
#undef SCALE
    };
    char line[40];
    zivi_header("SCALE");
    snprintf(line, sizeof line, "K1 TONICA    %s", names[scale_root]);
    cv_text(12, 40, &FONT_S, line, INK);
    snprintf(line, sizeof line, "K2   %s", modes[scale_mode]);
    cv_text(12, 67, &FONT_S, line, INK);
    for (unsigned i = 0; i < 12; ++i) {
        int x = 10 + (int)(i % 6) * 37, y = 103 + (int)(i / 6) * 32;
        bool active = synth_scale_note(60 + i, scale_root, scale_mode) == 60 + i;
        bool root = i == scale_root;
        cv_rect(x, y, 34, 27, active ? DARK_BLUE : SILVER);
        if (root) cv_rect(x, y, 34, 2, AMBER);
        rack_text_center(x, y + 8, 34, names[i], active ? WHITE : INK);
    }
    snprintf(line, sizeof line, "%02u / %u    K3 SALTA 8", (unsigned)scale_mode + 1u, (unsigned)SCALE_COUNT);
    cv_text(12, 177, &FONT_XS, line, INK);
    zivi_footer_pair("PADS  TONICA", ">  MIDI / AUDIO");
}

static void draw_arp(void)
{
    static const char *const orders[] = {"CRESCENTE", "DECRESCENTE", "ORDEM TOCADA", "ALEATORIO"};
    static const char *const divisions[ARP_DIVISION_COUNT] = {
        "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T"
    };
    const Arp *arp = &zivi.arp;
    char line[40];
    zivi_header("ARP");
    cv_rect(8, 36, 224, 26, arp->enabled ? DARK_BLUE : BLACK);
    cv_rect(8, 36, 3, 26, arp->enabled ? GREEN : GRAPHITE);
    cv_text(15, 40, &FONT_S, arp->enabled ? "ARP LIGADO" : "ARP DESLIGADO", WHITE);

    cv_rect(8, 68, 109, 24, SILVER);
    cv_rect(123, 68, 109, 24, SILVER);
    if (midi_clock.locked) snprintf(line, sizeof line, "EXT %u BPM", zivi.clock_bpm);
    else snprintf(line, sizeof line, "K1 BPM %u", arp->bpm);
    cv_text(12, 72, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "K5 CHANCE %u%%", arp->chance);
    cv_text(127, 72, &FONT_XS, line, INK);
    cv_rect(8, 96, 224, 20, SILVER);
    snprintf(line, sizeof line, "K2  %s", orders[arp->order]);
    cv_text(12, 98, &FONT_XS, line, INK);
    cv_rect(8, 120, 109, 20, SILVER);
    cv_rect(123, 120, 109, 20, SILVER);
    snprintf(line, sizeof line, "K3 GATE %u%%", arp->gate);
    cv_text(12, 122, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "K4 DIV %s", divisions[arp->division]);
    cv_text(127, 122, &FONT_XS, line, INK);
    cv_rect(8, 145, 109, 16, arp->hold ? DARK_BLUE : SILVER);
    cv_rect(123, 145, 109, 16, arp->free_run ? DARK_BLUE : SILVER);
    cv_text(12, 146, &FONT_T, "PAD 9 HOLD", arp->hold ? WHITE : INK);
    cv_text(127, 146, &FONT_T, "PAD 10 FREE", arp->free_run ? WHITE : INK);
    static const char *const scales[] = {
#define SCALE(symbol, mask, name, lcd, group) [SCALE_##symbol] = lcd,
#include "scales.def"
#undef SCALE
    };
    snprintf(line, sizeof line, "SCALE %u %s", arp->root + 1u, scales[arp->scale]);
    cv_text(12, 164, &FONT_XS, line, INK);
    if (zivi.sequencer.recording) snprintf(line, sizeof line, "REC SEQ  %02u / %02u", zivi.sequencer.index + 1, zivi.sequencer.length);
    else if (arp->velocity) snprintf(line, sizeof line, "NOTA MIDI %u", arp->note);
    else snprintf(line, sizeof line, "PADS 1-8 DIVISAO");
    cv_text(12, 185, &FONT_XS, line, INK);
    zivi_footer_pair("PARA  LIGA/DESLIGA", "REC  GRAVA SEQ");
}

static void draw_steps(void)
{
    const StepSequencer *seq = &zivi.sequencer;
    unsigned page = seq_visible_page(), first = page * NPADS;
    char line[40];
    zivi_header(seq_live ? "SEQ / LIVE" : "SEQ / STEP");

    const char *transport = seq->recording ? "REC" : seq->playing ? "PLAY" : "PARADO";
    cv_rect(8, 34, 53, 17, seq->recording ? RED : seq->playing ? DARK_BLUE : BLACK);
    rack_text_center(8, 36, 53, transport, WHITE);

    snprintf(line, sizeof line, "K1 %02u PASSOS", seq->length);
    cv_text(68, 36, &FONT_T, line, INK);
    snprintf(line, sizeof line, "%s %u", seq->clock_bpm ? "EXT" : midi_clock.external ? "EXT?" : "BPM",
             seq->clock_bpm ? seq->clock_bpm : seq->bpm);
    cv_text(230 - text_w(&FONT_XS, line), 34, &FONT_XS, line, seq->clock_bpm ? BLUE : INK);

    cv_rect(8, 56, 224, 103, BLACK);
    for (unsigned i = 0; i < 4; ++i) {
        unsigned index = first + 4 * i;
        if (index >= seq->length) break;
        int x = 8 + (int)i * 56;
        cv_rect(x, 58, 55, 98, i % 2 ? BLACK : DARK_BLUE);
        if (i) cv_rect(x, 59, 1, 95, PANEL_GRID);
    }
    for (int y = 67; y <= 147; y += 16) cv_rect(8, y, 224, 1, GRAPHITE);
    int selected_note = seq->steps[seq_cursor].velocity ? seq->steps[seq_cursor].note : seq_entry_note;
    for (unsigned pad = 0; pad < NPADS; ++pad) {
        unsigned index = first + pad;
        if (index >= seq->length) break;
        int x = 8 + (int)pad * 14;
        bool current = seq->playing && index == seq->index;
        const SeqStep *step = &seq->steps[index];
        if (current) {
            cv_rect(x + 1, 57, 12, 2, seq->recording ? RED : AMBER);
            cv_rect(x + 1, 61, 1, 91, PANEL_GRID);
        }
        if (step->velocity) {
            int row = clamp((int)step->note - selected_note + 5, 0, 11);
            cv_rect(x + 2, 149 - row * 8, 10, 5, current ? AMBER : WHITE);
        }
        if (index == seq_cursor) cv_rect(x + 1, 154, 12, 3, PANEL_CYAN);
        if (!(pad % 4)) {
            snprintf(line, sizeof line, "%02u", index + 1);
            cv_text(x + 2, 160, &FONT_T, line, GRAPHITE);
        }
    }
    snprintf(line, sizeof line, "K5 %02u  K2 N%03u  K3 V%03u", seq_cursor + 1, (unsigned)selected_note,
             seq->steps[seq_cursor].velocity);
    cv_text(9, 176, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "PAG %u / %u", page + 1, (seq->length - 1) / NPADS + 1);
    cv_text(231 - text_w(&FONT_T, line), 178, &FONT_T, line, GRAPHITE);
    cv_text(9, 196, &FONT_T, seq->recording ? "REC CONCLUI / PLAY INTERROMPE" :
            seq_entry ? "PAD + TECLA / TOQUE" : seq_message ? seq_message : "PADS  PASSOS    < >  PAGINAS", INK);
    zivi_footer_pair("FBANK  STEP/LIVE", "FX  IMPORTA KS   KBANK  ENVIA");
}

static void draw_fader(int x, int gauge, unsigned value, const char *unit, uint16_t color)
{
    cv_rect(x + 24, 75, 3, 75, GRAPHITE);
    for (unsigned i = 0; i <= 4; ++i) cv_rect(x + 35, 77 + (int)i * 17, 4, 1, GRAPHITE);
    if (gauge >= 0) {
        int y = 145 - 68 * gauge / 127;
        cv_rect(x + 14, y, 22, 6, INK);
        cv_rect(x + 15, y + 1, 20, 4, color);
    }
    char line[16];
    if (gauge < 0) snprintf(line, sizeof line, "--");
    else snprintf(line, sizeof line, "%u%s", value, unit);
    cv_rect(x + 3, 154, 45, 18, IVORY);
    rack_text_center(x, 156, 51, line, INK);
}

static void draw_synth_faders(void)
{
    static const char *const labels[] = {"SYNTH", "USB", "CORTE", "REVERB"};
    static const uint16_t colours[] = {GREEN, BLUE, AMBER, RED};
    unsigned rates[] = {synth_gain * 100u / 4096u, usb_play_gain * 100u / 4096u,
                        (unsigned)zivi.cutoff, (unsigned)(zivi.modules.p[M4_REVERB_MIX] * 100.0f + 0.5f)};
    unsigned gauge[] = {synth_gain * 127u / 4096u, usb_play_gain * 127u / 4096u,
                       (unsigned)clamp((int)(15.875f * log(zivi.cutoff / 55.0f) / log(2.0f)), 0, 127),
                       (unsigned)(zivi.modules.p[M4_REVERB_MIX] * 127.0f + 0.5f)};
    zivi_header("FADER BANK / SYNTH");
    char line[20];
    for (unsigned i = 0; i < 4; ++i) {
        int x = 8 + (int)i * 56;
        cv_rect(x, 44, 51, 134, SILVER);
        snprintf(line, sizeof line, "F%u", i + 1u);
        rack_text_center(x, 46, 51, line, INK);
        rack_text_center(x, 60, 51, labels[i], INK);
        draw_fader(x, (int)gauge[i], rates[i], i == 2 ? " Hz" : "%", colours[i]);
    }
    zivi_footer_pair("FBANK  VOLTA", "KBANK  MODULOS");
}

static void draw_controller(void)
{
    char line[48];
    const char *title = daw_scene == DAW_KNOBS ? daw_knobs_relative ? "KNOB BANK / REL" : "KNOB BANK / DAW" :
                        daw_scene == DAW_FADERS ? "FADER BANK / DAW" : "PAD BANK / DAW";
    zivi_header(title);
    unsigned knob_bank = daw_active_knob_bank(buttons_before);
    snprintf(line, sizeof line, "TX %u  K %c  F %c  PAD %c", tx_channel + 1,
             knob_bank ? 'B' : 'A', daw_fader_bank ? 'B' : 'A', daw_pad_bank ? 'B' : 'A');
    cv_text(8, 36, &FONT_XS, line, INK);
    if (daw_scene == DAW_KNOBS) {
        for (unsigned i = 0; i < 8; ++i) {
            int x = 8 + (int)(i % 4) * 56, y = 55 + (int)(i / 4) * 61;
            unsigned value = daw_values[knob_bank][i];
            cv_rect(x, y, 51, 55, SILVER);
            snprintf(line, sizeof line, "K%u CC%u", i + 1u, midi_cc[8u * knob_bank + i]);
            rack_text_center(x, y + 1, 51, line, INK);
            rack_knob(x + 25, y + 26, 10, knob_bank ? AMBER : BLUE, daw_knobs_relative ? 0.5f : value / 127.0f);
            if (daw_knobs_relative) snprintf(line, sizeof line, "REL");
            else snprintf(line, sizeof line, "%u", value);
            rack_text_center(x, y + 40, 51, line, INK);
        }
    } else if (daw_scene == DAW_FADERS) {
        for (unsigned i = 0; i < 4; ++i) {
            int x = 8 + (int)i * 56;
            unsigned value = daw_fader_values[daw_fader_bank][i];
            cv_rect(x, 54, 51, 122, SILVER);
            snprintf(line, sizeof line, "F%u CC%u", i + 1u, midi_cc[16u + 4u * daw_fader_bank + i]);
            rack_text_center(x, 57, 51, line, INK);
            draw_fader(x, (daw_fader_seen[daw_fader_bank] & (1u << i)) ? (int)value : -1, value, "",
                       daw_fader_bank ? AMBER : BLUE);
        }
    } else for (unsigned p = 0; p < NPADS; ++p) {
        int x = 8 + (int)(p % 4) * 56, y = 54 + (int)(p / 4) * 30;
        bool down = (daw_pads & (1u << p)) != 0;
        cv_rect(x, y, 53, 27, down ? DARK_BLUE : SILVER);
        snprintf(line, sizeof line, "%02u  N%u", p + 1, PAD_BASE_NOTE + (daw_pad_bank ? NPADS : 0u) + p);
        cv_text(x + 5, y + 5, &FONT_XS, line, down ? WHITE : INK);
    }
    if (midi_clock.external) {
        if (midi_clock.locked) snprintf(line, sizeof line, "EXT %u BPM  %s", sync_bpm(), sync_running ? "PLAY" : "STOP");
        else snprintf(line, sizeof line, "EXT   AGUARDA MIDI CLOCK");
    } else snprintf(line, sizeof line, "TRANSPORT %s", daw_playing ? "PLAY" : "STOP");
    cv_text(8, 190, &FONT_XS, line, INK);
    zivi_footer_pair("KBANK+FBANK  PADS", "GLO+PATCH  INSTRUMENTO");
}

static void draw_overlays(void)
{
    if (view == VIEW_PRESETS) draw_presets();
    else if (view == VIEW_SCALE) {
        if (scale_settings) draw_scale();
        else draw_scale_notes();
    }
    else if (view == VIEW_RADIO) draw_radio();
    else if (view == VIEW_ARP) draw_arp();
    else if (view == VIEW_SEQ) draw_steps();
    else if (view == VIEW_FADERS) draw_synth_faders();
    else if (view == VIEW_USER) draw_user();
    else if (view == VIEW_USB_ROUTES) draw_usb_routes();
    else draw_controller();
    cv_blit(0, 0);
}
