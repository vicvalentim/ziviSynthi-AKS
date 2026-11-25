#include "open_font.h"

#define RGB(r, g, b) ((uint16_t)(((r) >> 3) << 11 | ((g) >> 2) << 5 | ((b) >> 3)))

/* Keep both frames in local palette indices. The SDK may byte-swap its
 * borrowed RGB565 staging buffer while DMA is in progress. */
enum {
    BLACK, IVORY, INK, SILVER, GRAPHITE, BLUE, RED, GREEN, AMBER, WHITE,
    MATRIX_TILE_COLOR, DARK_BLUE, DISPLAY_COLORS
};
#define ZIVI_PANEL_PALETTE
static const uint16_t display_palette[DISPLAY_COLORS] = {
    [BLACK] = RGB(17, 19, 21), [IVORY] = RGB(233, 231, 221), [INK] = RGB(29, 35, 38),
    [SILVER] = RGB(199, 201, 197), [GRAPHITE] = RGB(68, 77, 81),
    [BLUE] = RGB(40, 121, 184), [RED] = RGB(190, 52, 64), [GREEN] = RGB(57, 150, 91),
    [AMBER] = RGB(242, 193, 91), [WHITE] = RGB(246, 244, 235),
    [MATRIX_TILE_COLOR] = RGB(22, 39, 50), [DARK_BLUE] = RGB(22, 67, 109)
};
enum { W = FM1_SCREEN_WIDTH, H = FM1_SCREEN_HEIGHT, BAND = 16, BANDS = H / BAND };
_Static_assert(H % BAND == 0 && W % 2 == 0 && W * BAND <= FM1_SCREEN_DMA_PIXELS, "LCD dimensions");
_Static_assert(DISPLAY_COLORS <= 16 && BANDS < 32, "LCD palette and band mask");
static uint8_t canvas[H][W / 2], shown[H][W / 2];
static uint16_t stripe[BAND * W] __attribute__((aligned(32)));
static struct { unsigned band, left, top, right, height; } dma;
static unsigned band_cursor, screen_faults;
/* Initialization clears the hardware to RGB black, outside our palette. */
static uint32_t invalid_bands = (1u << BANDS) - 1;
static bool screen_inflight, screen_dirty;

typedef struct { unsigned scale, advance; } OpenFont;
static const OpenFont FONT_T = {1, 6}, FONT_XS = {1, 6}, FONT_S = {1, 7}, FONT_B = {2, 12};

static void cv_pset(int x, int y, uint16_t color)
{
    if ((unsigned)x < W && (unsigned)y < H) {
        uint8_t *pixel = &canvas[y][x / 2];
        unsigned shift = ((unsigned)x & 1u) * 4;
        *pixel = (uint8_t)((*pixel & ~(15u << shift)) | (color << shift));
    }
}

static void cv_rect(int x, int y, int width, int height, uint16_t color)
{
    int right = x + width < W ? x + width : W;
    int bottom = y + height < H ? y + height : H;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right <= x || bottom <= y) return;
    unsigned pair = color | (color << 4);
    for (; y < bottom; ++y) {
        int first = x, last = right;
        if (first & 1) cv_pset(first++, y, color);
        if (last & 1) cv_pset(--last, y, color);
        if (first < last) memset(&canvas[y][first / 2], pair, (size_t)(last - first) / 2);
    }
}

static void cv_line(int x, int y, int end_x, int end_y, uint16_t color)
{
    int dx = end_x >= x ? end_x - x : x - end_x;
    int dy = end_y >= y ? y - end_y : end_y - y;
    int sx = x < end_x ? 1 : -1;
    int sy = y < end_y ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        cv_pset(x, y, color);
        if (x == end_x && y == end_y) return;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x += sx; }
        if (twice <= dx) { error += dx; y += sy; }
    }
}

static uint8_t glyph_column(unsigned ch, unsigned x)
{
    if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
    if (ch >= 'A' && ch <= 'Z') return font[10 + ch - 'A'][x];
    if (ch >= '0' && ch <= '9') return font[ch - '0'][x];
    switch (ch) {
    case '*': return x == 0 || x == 4 ? 0x14 : x == 2 ? 0x3e : 8;
    case '_': return 0x40;
    case '-': return 8;
    case '=': return 20;
    case '+': return x == 2 ? 0x3e : 8;
    case '.': return x == 2 ? 0x60 : 0;
    case ':': return x == 2 ? 0x36 : 0;
    case '/': return 1u << (5 - x);
    case '%': return (1u << (5 - x)) | (x == 0 ? 2 : x == 4 ? 32 : 0);
    case '>': x = 4 - x; /* fall through */
    case '<': return x < 4 ? (1u << (3 - x)) | (1u << (3 + x)) : 0;
    default: return 0;
    }
}

static int text_w(const OpenFont *face, const char *text)
{
    return (int)(strlen(text) * face->advance);
}

static void cv_text(int x, int y, const OpenFont *face, const char *text, uint16_t color)
{
    int scale = (int)face->scale;
    for (; *text; ++text, x += (int)face->advance) {
        if (x >= W) break;
        for (int col = 0; col < 5; ++col) {
            uint8_t bits = glyph_column((uint8_t)*text, (unsigned)col);
            for (int row = 0; row < 7; ++row) {
                if (bits & (1u << row)) cv_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void cv_begin(unsigned width, unsigned height, uint16_t color)
{
    if (width == W && height == H) memset(canvas, color | (color << 4), sizeof canvas);
    else cv_rect(0, 0, (int)width, (int)height, color);
}

static void cv_blit(unsigned x, unsigned y)
{
    if (!x && !y) screen_dirty = true;
}

/* Commit only after the SDK releases its DMA buffer. If a transfer fails,
 * the LCD may contain a partial band, so send that band again in full. */
static bool screen_idle(void)
{
    int state = fm1_screen_pending();
    if (state > 0) return false;
    if (screen_inflight) {
        if (state == 0) {
            for (unsigned y = dma.top; y < dma.top + dma.height; ++y)
                memcpy(&shown[y][dma.left], &canvas[y][dma.left], dma.right - dma.left);
            invalid_bands &= ~(1u << dma.band);
        } else {
            invalid_bands |= 1u << dma.band;
            ++screen_faults;
        }
        screen_inflight = false;
    }
    return true;
}

static bool screen_damage(unsigned band)
{
    unsigned y0 = band * BAND;
    unsigned left = W / 2, right = 0, top = y0 + BAND, bottom = y0;
    if (invalid_bands & (1u << band)) {
        left = 0; right = W / 2; top = y0; bottom = y0 + BAND;
    } else if (memcmp(canvas[y0], shown[y0], BAND * W / 2) != 0) {
        for (unsigned y = y0; y < y0 + BAND; ++y) {
            if (!memcmp(canvas[y], shown[y], W / 2)) continue;
            unsigned lo = 0, hi = W / 2;
            while (canvas[y][lo] == shown[y][lo]) ++lo;
            while (canvas[y][hi - 1] == shown[y][hi - 1]) --hi;
            if (lo < left) left = lo;
            if (hi > right) right = hi;
            if (y < top) top = y;
            bottom = y + 1;
        }
    }
    if (left >= right) return false;
    dma.band = band;
    dma.left = left;
    dma.right = right;
    dma.top = top;
    dma.height = bottom - top;
    return true;
}

/* At most one nonblocking transfer per visit to the control task. */
static void screen_send_stripe(void)
{
    if (!screen_dirty && !invalid_bands) return;
    for (unsigned checked = 0; checked < BANDS; ++checked) {
        unsigned band = band_cursor;
        band_cursor = (band_cursor + 1 == BANDS) ? 0 : band_cursor + 1;
        if (!screen_damage(band)) continue;
        uint16_t *pixels = stripe;
        for (unsigned y = dma.top; y < dma.top + dma.height; ++y) {
            for (unsigned x = dma.left; x < dma.right; ++x) {
                uint8_t pair = canvas[y][x];
                *pixels++ = display_palette[pair & 15u];
                *pixels++ = display_palette[pair >> 4];
            }
        }
        unsigned width = 2 * (dma.right - dma.left);
        int state = fm1_screen_submit(2 * dma.left, dma.top, width, dma.height, stripe, width * dma.height);
        if (state == 0) {
            screen_inflight = true;
        } else if (state < 0) {
            invalid_bands |= 1u << band;
            ++screen_faults;
        }
        return;
    }
    screen_dirty = false;
}
