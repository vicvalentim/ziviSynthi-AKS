#include "device.c"

#include <assert.h>

static uint32_t test_time, test_ticks;
static bool block_lcd;
static unsigned submitted;
static uint16_t lcd[H][W];

uint32_t fm1_millis(void) { return test_time; }
uint32_t fm1_device_ticks(void) { return test_ticks += 24000; }
void smk37_scan_read(void) {}
void smk37_rgb_set(unsigned id, unsigned rgb) { (void)id; (void)rgb; }
void smk37_rgb_service(void) {}
void fm1_control_request(void) {}
void fm1_device_fence(void) {}
uint32_t fm1_device_irq_save(void) { return 0; }
void fm1_device_irq_restore(uint32_t state) { (void)state; }
unsigned fm1_midi_ready_ports(void) { return 7; }
int fm1_midi_send_port(unsigned port, const uint8_t *data, size_t bytes)
{
    assert(port < 3 && data && bytes >= 1 && bytes <= 3);
    return 0;
}
int groove_master_update(unsigned adc) { assert(adc <= 1023); return 0; }

void fm1_audio_stats_read(struct fm1_audio_stats *stats) { *stats = (struct fm1_audio_stats){0}; }
void fm1_ble_get_status(struct fm1_ble_status *status)
{
    *status = (struct fm1_ble_status){.role = FM1_BLE_CENTRAL, .state = FM1_BLE_READY, .count = 1};
}
void fm1_bluetooth_get_status(struct fm1_bluetooth_status *status)
{
    *status = (struct fm1_bluetooth_status){.features = FM1_BT_MIDI | FM1_BT_AUDIO, .audio_connected = 1};
}
int fm1_ble_get_device(unsigned index, struct fm1_ble_device *device)
{
    assert(index == 0);
    *device = (struct fm1_ble_device){.name = "ZIVI MIDI", .midi = 1};
    return 0;
}
unsigned groove_settings_radio_flags(void) { return GROOVE_RADIO_ALL; }
unsigned groove_settings_role_value(void) { return FM1_BLE_CENTRAL; }
int groove_settings_radio(unsigned flags, unsigned role) { (void)flags; (void)role; return 0; }
int fm1_ble_scan(void) { return 0; }
int fm1_ble_connect(unsigned index) { (void)index; return 0; }
int fm1_ble_disconnect(void) { return 0; }
int fm1_ble_forget_bonds(void) { return 0; }

int fm1_screen_pending(void) { return block_lcd; }
int fm1_screen_submit(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t *pixels, size_t count)
{
    assert(!block_lcd && x+w <= W && y+h <= H && count == w*h);
    block_lcd = true;
    ++submitted;
    for (unsigned row = 0; row < h; ++row) memcpy(&lcd[y+row][x], &pixels[row*w], w*sizeof *pixels);
    return 0;
}

static void pump_lcd(void)
{
    for (unsigned i = 0; i <= BANDS; ++i) {
        block_lcd = false;
        assert(screen_idle());
        screen_send_stripe();
    }
    assert(!block_lcd);
}

static void press(unsigned button)
{
    smk37_control_event(SMK37_BUTTON, button, 1);
    service_buttons();
    smk37_control_event(SMK37_BUTTON, button, 0);
}

static void capture(const char *dir, const char *page)
{
    pump_lcd();
    if (!dir) return;
    char path[512];
    size_t need = strlen(dir) + strlen(page) + sizeof "/.ppm";
    assert(need < sizeof path);
    strcpy(path, dir);
    strcat(path, "/");
    strcat(path, page);
    strcat(path, ".ppm");
    FILE *image = fopen(path, "wb");
    assert(image);
    fputs("P6\n240 240\n255\n", image);
    for (unsigned y = 0; y < H; ++y) {
        for (unsigned x = 0; x < W; ++x) {
            unsigned p = lcd[y][x];
            unsigned char rgb[] = {(unsigned char)((p >> 11) * 255 / 31),
                (unsigned char)(((p >> 5) & 63) * 255 / 63), (unsigned char)((p & 31) * 255 / 31)};
            assert(fwrite(rgb, 1, sizeof rgb, image) == sizeof rgb);
        }
    }
    assert(fclose(image) == 0);
}

static void test_recovery_and_profile(void)
{
    app_init();
    daw_mode = false;
    view = VIEW_INSTRUMENT;
    audio_ready = 1;
    reset_profile();
    int32_t frame[FM1_BLOCK_WORDS];
    const uint8_t note[] = {0x90, 69, 100}, program[] = {0xc0, 0}, other[] = {0xc0, 13};
    zivi_open_midi(FM1_MIDI_USB, note, sizeof note);

    /* A note already in the MIDI queue must sound in the very first block
     * after refill, even if a previous LCD transfer is still pending. */
    block_lcd = true;
    assert(audio_reserve() == 0);
    groove_screen_control();
    assert(audio_reserve() == 8 && audio_profile.minimum_reserve == 0);
    assert(!zivi_open_audio(frame, FM1_BLOCK_FRAMES));
    bool audible = false;
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) audible |= frame[i] != 0;
    assert(audible);

    unsigned consumed = 1;
    for (unsigned ms = 10; ms <= 6000; ms += 10) {
        unsigned target = (uint64_t)ms * ZIVI_SMK_RATE / (1000 * ZIVI_SMK_FRAMES);
        while (consumed < target) {
            assert(!zivi_open_audio(frame, FM1_BLOCK_FRAMES));
            ++consumed;
        }
        test_time = ms;
        block_lcd = ms % 400 < 20;
        if (ms % 700 == 0) zivi_open_midi(FM1_MIDI_USB, program, sizeof program);
        if (ms % 700 == 350) zivi_open_midi(FM1_MIDI_BLE, other, sizeof other);
        /* Deliberately miss a control wake (20 ms between refills). */
        if (ms % 190) groove_screen_control();
    }
    assert(zivi_device.underruns == 0);
    assert(profile_p95_us() == 1500);

    reset_profile();
    for (unsigned i = 0; i < 95; ++i) profile_render(12000);
    for (unsigned i = 0; i < 5; ++i) profile_render(AUDIO_DEADLINE_TICKS + 1);
    assert(profile_p95_us() == 1000 && audio_profile.histogram[6] == 5);
    profile_render(96001);
    assert(profile_p95_us() == 4001 && audio_profile.histogram[6] == 6);
    puts("Controle simulado: primeira nota no primeiro bloco, seis segundos com programação, LCD e pausas de controle de 20 ms, histograma e pico verificados.");
}

int main(int argc, char **argv)
{
    const char *screens = argc > 1 ? argv[1] : NULL;
    app_init();
    memset(output_note, 255, sizeof output_note);
    app_ui_init();
    capture(screens, "matriz");
    assert(lcd[35][6] == display_palette[BLACK]);
    press(BTN_PATCH);
    assert(view == VIEW_PRESETS && page_size(0) == 14);
    draw_presets();
    capture(screens, "presets");
    press(BTN_LEFT);
    assert(preset_page == 3 && page_size(3) == 16);
    pad_press(15, 100);
    assert(zivi_preset == 65);
    unsigned count = zivi.ks.count;
    assert(count > 0);
    press(BTN_SEQ);
    assert(view == VIEW_INSTRUMENT && zivi_ks_page);
    press(BTN_PLAY);
    assert(zivi.ks.mode == KS_PLAYING && zivi.ks.count == count);
    zivi_draw_ks();
    capture(screens, "ks");
    press(BTN_BT);
    assert(view == VIEW_BT);
    draw_controls();
    capture(screens, "bluetooth");
    press(BTN_SCALE);
    assert(view == VIEW_MIDI);
    draw_controls();
    capture(screens, "diagnostico");

    smk37_control_event(SMK37_BUTTON, BTN_GLOBE, 1);
    press(BTN_PATCH);
    smk37_control_event(SMK37_BUTTON, BTN_GLOBE, 0);
    assert(daw_mode && zivi_device.muted);
    press(BTN_SCALE);
    assert(daw_mode && view == VIEW_MIDI);
    const uint8_t info[] = {0xb0, 0, 4}, program[] = {0xc0, 15};
    zivi_open_midi(FM1_MIDI_USB, info, sizeof info);
    zivi_open_midi(FM1_MIDI_USB, program, sizeof program);
    service_midi();
    assert(zivi_preset == 65 && zivi_device.muted);
    press(BTN_SCALE);
    assert(daw_mode && view == VIEW_INSTRUMENT);
    draw_controls();
    capture(screens, "controlador");
    assert(submitted > 0);
    test_recovery_and_profile();
    puts("Painel Open simulado: matriz, presets, KS, menu BT/SCALE, controlador e Program Change.");
}
