#ifndef ZIVI_KS_H
#define ZIVI_KS_H

#include <stdbool.h>
#include <stdint.h>

enum { KS_CAPACITY = 256 };
enum KsMode { KS_STOPPED, KS_RECORDING, KS_PLAYING };
enum KsLimit { KS_LIMIT_NONE, KS_LIMIT_TIME, KS_LIMIT_MEMORY };

typedef struct {
    uint32_t frame;
    uint8_t note;
    uint8_t velocity;
    bool attack;
} KsEvent;

typedef struct {
    uint16_t ms;
    uint8_t note, velocity;
} KsTimedEvent;

typedef struct {
    uint16_t ms;
    uint8_t note, velocity;
    bool attack;
} KsTimedState;

typedef struct {
    KsEvent events[KS_CAPACITY];
    uint32_t rate, length, record_frame, record_limit, position;
    unsigned count, next_event;
    uint32_t random_state;
    unsigned realtime_spread, sequence_spread, speed, limit_seconds;
    uint32_t clock_speed_q8, clock_fraction;
    int transpose;
    enum KsMode mode;
    enum KsLimit limit;
    uint8_t note, velocity, random_note;
    bool loop, sequence_trigger, random_held, attack;
} Ks;

void ks_init(Ks *ks, uint32_t rate);
void ks_record(Ks *ks);
bool ks_load_ms(Ks *ks, const KsTimedEvent *events, unsigned count, unsigned length_ms);
bool ks_load_states(Ks *ks, const KsTimedState *states, unsigned count, unsigned length_ms);
void ks_capture(Ks *ks, unsigned note, unsigned velocity, bool attack);
bool ks_play(Ks *ks);
bool ks_play_from(Ks *ks, uint32_t frame);
void ks_stop(Ks *ks);
void ks_prepare_sample(Ks *ks);
void ks_advance(Ks *ks);
void ks_random(Ks *ks, bool down);
void ks_controls(Ks *ks, int realtime_spread, int sequence_spread, int speed,
                 int transpose, int limit_seconds, bool loop, bool sequence_trigger);

#endif
