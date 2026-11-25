#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FM1_SCREEN_WIDTH 240
#define FM1_SCREEN_HEIGHT 240
#define FM1_SCREEN_DMA_PIXELS (240 * 16)
static int pending;
static bool error_next, busy_next;
static unsigned pushes, area, remaining;
static uint16_t lcd[240][240];

static int fm1_screen_pending(void)
{
    int result = pending;
    if (pending != 1) pending = 0;
    return result;
}

static int fm1_screen_submit(unsigned x, unsigned y, unsigned width, unsigned height,
                             uint16_t *pixels, size_t count)
{
    assert(!pending && x + width <= 240 && y + height <= 240);
    assert(count == width * height && count <= FM1_SCREEN_DMA_PIXELS);
    if (busy_next) {
        busy_next = false;
        return 1;
    }
    ++pushes;
    area += count;
    remaining = (unsigned)count;
    for (unsigned i = 0; i < count; ++i) {
        if (!error_next || i < count / 2) lcd[y + i / width][x + i % width] = pixels[i];
        pixels[i] = (uint16_t)(pixels[i] >> 8 | pixels[i] << 8);
    }
    pending = error_next ? -2 : 1;
    error_next = false;
    return 0;
}

#include "display.h"

static void finish(void)
{
    if (pending == 1) {
        assert(!screen_idle());
        pending = 0;
    }
    assert(screen_idle());
}

static void paint(void)
{
    for (unsigned i = 0; i < BANDS + 1; ++i) {
        finish();
        screen_send_stripe();
    }
    finish();
}

int main(void)
{
    cv_begin(W, H, IVORY);
    cv_blit(0, 0);
    paint();
    assert(area == W * H && pushes == BANDS);
    for (unsigned y = 0; y < H; ++y)
        for (unsigned x = 0; x < W; ++x) assert(lcd[y][x] == display_palette[IVORY]);

    area = pushes = 0;
    cv_blit(0, 0);
    paint();
    assert(area == 0 && pushes == 0);
    cv_rect(41, 35, 3, 1, AMBER);
    cv_blit(0, 0);
    busy_next = true;
    paint();
    assert(pushes == 1 && remaining == 4 && area == 4);
    assert(lcd[35][40] == display_palette[IVORY]);
    for (unsigned x = 41; x < 44; ++x) assert(lcd[35][x] == display_palette[AMBER]);
    assert(lcd[35][44] == display_palette[IVORY]);

    cv_rect(0, 239, 240, 1, BLUE);
    cv_blit(0, 0);
    error_next = true;
    paint();
    assert(screen_faults == 1);
    for (unsigned x = 0; x < W; ++x) assert(lcd[239][x] == display_palette[BLUE]);

    cv_begin(W, H, BLACK);
    cv_text(1, 1, &FONT_B, "A15", WHITE);
    cv_line(-1, 5, 242, 5, RED);
    cv_blit(0, 0);
    paint();
    assert(lcd[5][0] == display_palette[RED] && lcd[5][239] == display_palette[RED]);
    printf("LCD simulado: banda 16 linhas, frame estático zero pixels, retângulo 3 px envia 4 px, recuperação após DMA.\n");
}
