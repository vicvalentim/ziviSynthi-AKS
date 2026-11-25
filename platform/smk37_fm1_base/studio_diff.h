/* The variable KS phrase is kept apart so its size never shifts SEQ or routing fields. */
enum {
    DIFF_PANEL = 2 * (PANEL_AKS + 1),
    DIFF_FIXED = DIFF_PANEL + STUDIO_BASE + STUDIO_EXT,
    DIFF_SEQUENCE = KS_CAPACITY * 5,
    DIFF_LIMIT = 2 + 4 * DIFF_FIXED + DIFF_SEQUENCE
};
static uint8_t studio_diff_fixed[DIFF_FIXED] PLAT_POOL;
static uint8_t studio_diff_sequence[DIFF_SEQUENCE] PLAT_POOL;
static uint8_t studio_diff_wire[DIFF_LIMIT] PLAT_POOL;
static unsigned studio_diff_revision, studio_diff_base, studio_diff_patch_revision;
static unsigned studio_diff_seq_size, studio_diff_size, studio_diff_served;
static unsigned studio_diff_crc;

static void studio_diff_range(unsigned offset, const uint8_t *source, unsigned length, unsigned *used)
{
    uint8_t *prior = studio_diff_fixed + offset;
    for (unsigned i = 0; i < length;) {
        if (prior[i] == source[i]) { ++i; continue; }
        unsigned first = i++;
        while (i < length && i - first < 127 && prior[i] != source[i]) ++i;
        uint8_t *entry = studio_diff_wire + *used;
        studio_septets(entry, offset + first, 2);
        entry[2] = i - first;
        memcpy(entry + 3, source + first, i - first);
        memcpy(prior + first, source + first, i - first);
        *used += 3 + i - first;
    }
}

static bool studio_diff_capture(void)
{
    unsigned used = 2, next_seq = 0x3fff;
    bool patch_ready = !studio_expected && !studio_read_size &&
        zivi.ks.mode != KS_RECORDING && !zivi.sequencer.recording;
    if (!studio_diff_patch_revision && !patch_ready) return false;
    if (patch_ready) {
        studio_sync_observe();
        if (studio_diff_patch_revision != studio_sync.revision) {
            bool omitted;
            unsigned size = studio_read_patch(true, true, &omitted);
            unsigned sequence_size = size - STUDIO_BASE - STUDIO_EXT;
            studio_diff_range(DIFF_PANEL, studio_patch, STUDIO_BASE, &used);
            studio_diff_range(DIFF_PANEL + STUDIO_BASE, studio_patch + STUDIO_BASE + sequence_size, STUDIO_EXT, &used);
            if (sequence_size != studio_diff_seq_size ||
                memcmp(studio_diff_sequence, studio_patch + STUDIO_BASE, sequence_size) != 0) {
                next_seq = sequence_size;
                studio_diff_seq_size = sequence_size;
                memcpy(studio_diff_sequence, studio_patch + STUDIO_BASE, sequence_size);
            }
            studio_diff_patch_revision = studio_sync.revision;
        }
    }
    uint8_t panel[DIFF_PANEL];
    studio_panel_read(panel, PANEL_AKS + 1);
    /* Do not overwrite the replayable stream for a poll with no changes. */
    if (used == 2 && next_seq == 0x3fff &&
        memcmp(panel, studio_diff_fixed, sizeof panel) == 0 && studio_diff_revision) return true;
    studio_diff_range(0, panel, sizeof panel, &used);
    studio_septets(studio_diff_wire, next_seq, 2);
    if (next_seq != 0x3fff) {
        memcpy(studio_diff_wire + used, studio_diff_sequence, next_seq);
        used += next_seq;
    }
    studio_diff_base = studio_diff_revision;
    studio_diff_revision = studio_diff_revision % 0x1fffffu + 1;
    studio_diff_size = used;
    studio_diff_crc = studio_crc(studio_diff_wire, used);
    return true;
}

static unsigned studio_diff_command(const uint8_t *p, unsigned n, uint8_t *answer, unsigned *size)
{
    if (n == 2) {
        unsigned offset = studio_u14(p);
        if (offset >= studio_diff_served) return STUDIO_INCOMPLETE;
        unsigned count = studio_diff_served - offset;
        if (count > STUDIO_PART_MAX) count = STUDIO_PART_MAX;
        memcpy(answer, studio_diff_wire + offset, count);
        *size = count;
        return STUDIO_OK;
    }
    if (n != 3) return STUDIO_INVALID;
    const unsigned known = studio_u21(p);
    const bool ready = studio_diff_capture();
    if (ready) studio_xy = true;
    answer[0] = 0;
    studio_septets(answer + 1, ready ? studio_diff_revision : known, 3);
    *size = 4;
    studio_diff_served = 0;
    if (!ready || known == studio_diff_revision) return STUDIO_OK;
    if (known && known == studio_diff_base && studio_diff_size) {
        answer[0] = 1;
        studio_diff_served = studio_diff_size;
    } else {
        answer[0] = 2;
        memcpy(studio_diff_wire, studio_diff_fixed, DIFF_FIXED);
        memcpy(studio_diff_wire + DIFF_FIXED, studio_diff_sequence, studio_diff_seq_size);
        studio_diff_size = 0;
        studio_diff_served = DIFF_FIXED + studio_diff_seq_size;
        studio_diff_crc = studio_crc(studio_diff_wire, studio_diff_served);
    }
    studio_septets(answer + 4, studio_diff_served, 2);
    studio_septets(answer + 6, studio_diff_crc, 3);
    if (studio_diff_served <= STUDIO_PART_MAX - 9) {
        memcpy(answer + 9, studio_diff_wire, studio_diff_served);
        *size = 9 + studio_diff_served;
    } else *size = 9;
    return STUDIO_OK;
}

/* Scalar writes for the parameter groups that previously shared one full command.
 * Validate the entire proposed group before touching any live state. */
static bool studio_fields(const uint8_t *p, unsigned n)
{
    if (n < 4 || (n - 1) % 3) return false;
    const unsigned group = *p++;
    const Ks *ks = &zivi.ks;
    Arp *arp = &zivi.arp;
    StepSequencer *seq = &zivi.sequencer;
    unsigned values[11], count;
    switch (group) {
    case 0: {
        const unsigned current[] = {ks->realtime_spread, ks->sequence_spread, ks->speed,
            (unsigned)(ks->transpose + 24), ks->limit_seconds, ks->loop, ks->sequence_trigger};
        count = sizeof current / sizeof *current;
        memcpy(values, current, sizeof current);
        break;
    }
    case 1: {
        const unsigned current[] = {arp->enabled, arp->hold, arp->free_run, arp->bpm, arp->gate,
            arp->order, arp->division, arp->chance, scale_root, scale_mode};
        count = sizeof current / sizeof *current;
        memcpy(values, current, sizeof current);
        break;
    }
    case 2:
        if (daw_mode || seq->recording) return false;
        values[0] = seq->length;
        values[1] = seq->bpm;
        values[2] = seq->timing_bpm ? seq->timing_bpm : seq->bpm;
        values[3] = seq->division;
        count = 4;
        break;
    default:
        return false;
    }
    unsigned changed = 0;
    for (unsigned left = n - 1; left; left -= 3, p += 3) {
        if (*p >= count) return false;
        const unsigned value = studio_u14(p + 1);
        if (values[*p] != value) changed |= 1u << *p;
        values[*p] = value;
    }
    if (!changed) return true;
    if (group == 0) {
        const uint16_t lo[] = {25, 25, 25, 0, 1, 0, 0}, hi[] = {200, 200, 400, 48, 30, 1, 1};
        for (unsigned i = 0; i < count; ++i) if (values[i] < lo[i] || values[i] > hi[i]) return false;
        ks_controls(&zivi.ks, values[0], values[1], values[2], (int)values[3] - 24, values[4], values[5], values[6]);
    } else if (group == 1) {
        const uint16_t lo[] = {0, 0, 0, 30, 10, 0, 0, 0, 0, 0};
        const uint16_t hi[] = {1, 1, 1, 300, 95, ARP_ORDER_COUNT - 1, ARP_DIVISION_COUNT - 1, 100, 11, SCALE_COUNT - 1};
        for (unsigned i = 0; i < count; ++i) if (values[i] < lo[i] || values[i] > hi[i]) return false;
        if (changed & 0x79) synth_arp(&zivi, values[0], values[3], values[4], (enum ArpOrder)values[5], (enum ArpDivision)values[6]);
        if (changed & 0x86) synth_arp_modes(&zivi, values[1], values[2], values[7]);
        if (changed & 0x300) set_scale(values[8], (enum ScaleMode)values[9]);
        if (arp->enabled && arp->free_run && !daw_mode) zivi_device.muted = zivi_muted = false;
    } else {
        if (!values[0] || values[0] > SEQ_STEPS || values[1] < 30 || values[1] > 300 ||
            values[2] < 30 || values[2] > 300 || values[3] >= ARP_DIVISION_COUNT) return false;
        seq->division = (enum ArpDivision)values[3];
        seq->timing_bpm = values[2];
        synth_seq_timing(&zivi, values[0], values[1]);
    }
    zivi_dirty = 1;
    return true;
}
