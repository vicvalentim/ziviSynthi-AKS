#include "presets.h"

#include <string.h>

/* Bits 0–3: column. Bits 4–8: row (16/17 = USB). Bit 9: half-level pin. */
typedef uint16_t FactoryPin;

typedef struct {
    const char *key, *display;
    struct { ZiviReal tune, shape, level, mix, lfo, cutoff; } basic;
    const FactoryPin *pins;
    const uint8_t *control_ids;
    const ZiviReal *control_values;
    uint8_t pin_count, control_count;
    bool ks_matrix;
    const KsTimedEvent *phrase;
    uint16_t length_ms, event_count, ks_full, ks_half;
} FactoryPreset;

_Static_assert(M4_COUNT <= 256, "Preset control index must fit in uint8_t");

#ifdef ZIVI_FELUCCA_CATALOG
#include "../../build/felucca-presets.inc"

unsigned synth_factory_bank_count(void)
{
    return sizeof factory_banks / sizeof factory_banks[0];
}

const FactoryBank *synth_factory_bank(unsigned page)
{
    return &factory_banks[page];
}
#else
#include "../../build/factory-presets.inc"
#endif

unsigned synth_factory_count(void)
{
    return sizeof factory_presets / sizeof factory_presets[0];
}

int synth_factory_find(const char *key)
{
    for (unsigned i = 0; i < synth_factory_count(); ++i) {
        if (strcmp(factory_presets[i].key, key) == 0) return (int)i;
    }
    return -1;
}

const char *synth_factory_display(unsigned index)
{
    return index < synth_factory_count() ? factory_presets[index].display : NULL;
}

bool synth_factory_has_phrase(unsigned index)
{
    return index < synth_factory_count() && factory_presets[index].phrase;
}

bool synth_factory_apply(Synth *synth, unsigned index)
{
    if (index >= synth_factory_count()) return false;
    const FactoryPreset *preset = &factory_presets[index];
    if (preset->phrase && !ks_load_ms(&synth->ks, preset->phrase, preset->event_count, preset->length_ms)) {
        return false;
    }
    synth_safe_patch(synth);
    synth_clear_matrix(synth);
    synth_ks_matrix(synth, preset->ks_matrix);
    for (unsigned col = 0; col < MATRIX_SIZE; ++col) {
        unsigned bit = 1u << col;
        if (preset->ks_full & bit) synth_ks_route(synth, col, PIN_FULL);
        if (preset->ks_half & bit) synth_ks_route(synth, col, PIN_HALF);
    }
    for (unsigned i = 0; i < preset->pin_count; ++i) {
        FactoryPin pin = preset->pins[i];
        unsigned row = (pin >> 4) & 31u, col = pin & 15u;
        enum Pin amount = pin & 512u ? PIN_HALF : PIN_FULL;
        if (row < MATRIX_SIZE) synth_pin(synth, row, col, amount);
        else synth_usb_route(synth, row - MATRIX_SIZE, col, amount);
    }
    synth_controls(synth, preset->basic.tune, preset->basic.shape, preset->basic.level);
    synth_patch_controls(synth, preset->basic.mix, preset->basic.lfo, preset->basic.cutoff);
    for (unsigned i = 0; i < preset->control_count; ++i) {
        synth_module_control(synth, preset->control_ids[i], preset->control_values[i]);
    }
    if (preset->phrase) {
        synth->ks.sequence_trigger = true;
        synth->ks.speed = synth->ks.sequence_spread = 100;
        synth->ks.transpose = 0;
        synth->ks.loop = true;
    }
    return true;
}
