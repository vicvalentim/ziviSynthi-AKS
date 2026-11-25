#include "ks.h"

#include <string.h>

static int clamp_int(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}

void ks_init(Ks *ks, uint32_t rate)
{
    memset(ks, 0, sizeof *ks);
    ks->rate = rate;
    ks->note = 69;
    ks->random_state = 0x4269d331u;
    ks_controls(ks, 100, 100, 100, 0, 12, true, false);
}

void ks_controls(Ks *ks, int realtime_spread, int sequence_spread, int speed,
                 int transpose, int limit_seconds, bool loop, bool sequence_trigger)
{
    ks->realtime_spread = (unsigned)clamp_int(realtime_spread, 25, 200);
    ks->sequence_spread = (unsigned)clamp_int(sequence_spread, 25, 200);
    ks->speed = (unsigned)clamp_int(speed, 25, 400);
    ks->transpose = clamp_int(transpose, -24, 24);
    ks->limit_seconds = (unsigned)clamp_int(limit_seconds, 1, 30);
    ks->loop = loop;
    ks->sequence_trigger = sequence_trigger;
}

static void rewind(Ks *ks)
{
    ks->position = 0;
    ks->next_event = 0;
    ks->velocity = 0;
    ks->note = 69;
    ks->attack = false;
}

void ks_stop(Ks *ks)
{
    if (ks->mode == KS_RECORDING) {
        ks->length = ks->count ? ks->record_frame : 0;
        if (ks->count && ks->length <= ks->events[ks->count - 1].frame) {
            ks->length = ks->events[ks->count - 1].frame + 1;
        }
    }
    ks->mode = KS_STOPPED;
    rewind(ks);
}

static uint32_t frames_from_ms(uint32_t rate, unsigned ms)
{
    return (rate / 1000) * ms + ((rate % 1000) * ms + 500) / 1000;
}

static void ks_start_load(Ks *ks, unsigned count, unsigned length_ms)
{
    ks_stop(ks);
    ks->length = frames_from_ms(ks->rate, length_ms);
    ks->count = count;
    ks->limit = KS_LIMIT_NONE;
}

bool ks_load_ms(Ks *ks, const KsTimedEvent *events, unsigned count, unsigned length_ms)
{
    if (!events || count < 2 || count > KS_CAPACITY || count % 2 || !length_ms || length_ms > 30000) return false;
    unsigned previous = 0;
    for (unsigned i = 0; i < count; i += 2) {
        const KsTimedEvent *on = &events[i], *off = &events[i + 1];
        if ((i && on->ms <= previous) || on->ms >= off->ms || off->ms >= length_ms
            || on->note > 127 || on->note != off->note || !on->velocity || on->velocity > 127
            || off->velocity) return false;
        previous = off->ms;
    }
    ks_start_load(ks, count, length_ms);
    for (unsigned i = 0; i < count; ++i) {
        const KsTimedEvent *event = &events[i];
        ks->events[i] = (KsEvent){ frames_from_ms(ks->rate, event->ms), event->note,
                                  event->velocity, event->velocity != 0 };
    }
    return true;
}

bool ks_load_states(Ks *ks, const KsTimedState *states, unsigned count, unsigned length_ms)
{
    if (!states || !count || count > KS_CAPACITY || !length_ms || length_ms > 30000 ||
        !states[0].velocity || !states[0].attack) return false;
    for (unsigned i = 0; i < count; ++i) {
        const KsTimedState *event = &states[i];
        if (event->ms >= length_ms || (i && event->ms <= states[i - 1].ms) ||
            event->note > 127 || event->velocity > 127 || (event->attack && !event->velocity)) return false;
    }
    ks_start_load(ks, count, length_ms);
    for (unsigned i = 0; i < count; ++i) {
        const KsTimedState *event = &states[i];
        ks->events[i] = (KsEvent){ frames_from_ms(ks->rate, event->ms), event->note,
                                  event->velocity, event->attack };
    }
    return true;
}

void ks_record(Ks *ks)
{
    ks_stop(ks);
    ks->mode = KS_RECORDING;
    ks->count = 0;
    ks->length = 0;
    ks->record_frame = 0;
    ks->record_limit = ks->rate * ks->limit_seconds;
    ks->limit = KS_LIMIT_NONE;
}

void ks_capture(Ks *ks, unsigned note, unsigned velocity, bool attack)
{
    if (ks->mode != KS_RECORDING) return;
    KsEvent event = {ks->record_frame, (uint8_t)note, (uint8_t)velocity, attack};
    if (ks->count == 0 && velocity == 0) return;
    if (ks->count) {
        KsEvent *last = &ks->events[ks->count - 1];
        if (!attack && last->note == note && last->velocity == velocity) return;
        if (last->frame == event.frame) {
            event.attack |= last->attack && velocity != 0;
            *last = event;
            if (ks->count == 1 && velocity == 0) ks->count = 0;
            return;
        }
    }
    if (ks->count == KS_CAPACITY) {
        ks->limit = KS_LIMIT_MEMORY;
        ks_stop(ks);
        return;
    }
    ks->events[ks->count++] = event;
}

bool ks_play(Ks *ks)
{
    ks_stop(ks);
    if (ks->count == 0) return false;
    ks->mode = KS_PLAYING;
    ks->clock_fraction = 0;
    return true;
}

bool ks_play_from(Ks *ks, uint32_t frame)
{
    if (!ks_play(ks)) return false;
    if (frame >= ks->length) {
        if (!ks->loop || !ks->length) {
            ks_stop(ks);
            return false;
        }
        frame %= ks->length;
    }
    ks->position = frame * 100u;
    return true;
}

void ks_prepare_sample(Ks *ks)
{
    ks->attack = false;
    if (ks->mode != KS_PLAYING) return;
    uint32_t end = ks->length * 100u;
    if (ks->position >= end) {
        if (!ks->loop) {
            ks_stop(ks);
            return;
        }
        uint32_t remainder = ks->position % end;
        rewind(ks);
        ks->position = remainder;
    }
    while (ks->next_event < ks->count && ks->events[ks->next_event].frame * 100u <= ks->position) {
        const KsEvent *event = &ks->events[ks->next_event++];
        ks->note = event->note;
        ks->velocity = event->velocity;
        ks->attack = event->attack && event->velocity != 0;
    }
}

void ks_advance(Ks *ks)
{
    if (ks->mode == KS_PLAYING) {
        if (ks->clock_speed_q8) {
            uint32_t advance = ks->clock_speed_q8 + ks->clock_fraction;
            ks->position += advance >> 8;
            ks->clock_fraction = advance & 255u;
        } else ks->position += ks->speed;
    } else if (ks->mode == KS_RECORDING && ks->count && ++ks->record_frame == ks->record_limit) {
        ks->limit = KS_LIMIT_TIME;
        ks_stop(ks);
    }
}

void ks_random(Ks *ks, bool down)
{
    if (down && !ks->random_held) {
        uint32_t seed = ks->random_state;
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        ks->random_state = seed;
        ks->random_note = (uint8_t)(48 + seed % 30);
    }
    ks->random_held = down;
}
