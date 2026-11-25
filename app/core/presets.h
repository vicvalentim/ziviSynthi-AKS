#ifndef ZIVI_PRESETS_H
#define ZIVI_PRESETS_H

#include "synth.h"

#ifdef ZIVI_FELUCCA_CATALOG
typedef struct {
    const char *label;
    uint16_t start;
    uint8_t size;
} FactoryBank;

unsigned synth_factory_bank_count(void);
const FactoryBank *synth_factory_bank(unsigned page);
#endif

unsigned synth_factory_count(void);
int synth_factory_find(const char *key);
const char *synth_factory_display(unsigned index);
bool synth_factory_has_phrase(unsigned index);
bool synth_factory_apply(Synth *synth, unsigned index);

#endif
