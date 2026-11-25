/* Canvas for the shared Synthi panel. Pixels are stored in LCD wire order. */
typedef struct {
    uint8_t h, pad, first, last;
    const uint8_t *adv, *bw;
    const uint16_t *off;
    const uint8_t *data;
} felucca_font_t;

#include "zivi_fonts.h"

#define RGB(r, g, b) ((uint16_t)(((r) >> 3) << 11 | ((g) >> 2) << 5 | ((b) >> 3)))

enum { LCD_WIDTH = 240, LCD_HEIGHT = 240, LCD_STRIPE = 4 };
static uint16_t pixels[LCD_HEIGHT][LCD_WIDTH] __attribute__((section(".pool")));
static unsigned rows_sent = LCD_HEIGHT;

static uint16_t pixel_order(uint16_t value)
{
    return (uint16_t)(value << 8 | value >> 8);
}

static void cv_pset(int x, int y, uint16_t color)
{
    if ((unsigned)x < LCD_WIDTH && (unsigned)y < LCD_HEIGHT)
        pixels[y][x] = pixel_order(color);
}

static void cv_rect(int x, int y, int width, int height, uint16_t color)
{
    int right = x + width < LCD_WIDTH ? x + width : LCD_WIDTH;
    int bottom = y + height < LCD_HEIGHT ? y + height : LCD_HEIGHT;
    uint16_t value = pixel_order(color);
    for (int row = y < 0 ? 0 : y; row < bottom; ++row) {
        for (int col = x < 0 ? 0 : x; col < right; ++col)
            pixels[row][col] = value;
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

static unsigned font_index(const felucca_font_t *font, unsigned ch)
{
    return (ch >= font->first && ch <= font->last ? ch : '?') - font->first;
}

static int text_w(const felucca_font_t *font, const char *text)
{
    int width = 0;
    for (; *text; ++text) width += font->adv[font_index(font, (uint8_t)*text)];
    return width;
}

static uint16_t pixel_blend(uint16_t background, uint16_t color, unsigned alpha)
{
    unsigned inverse = 15 - alpha;
    unsigned r = ((background >> 11) * inverse + (color >> 11) * alpha + 7) / 15;
    unsigned g = (((background >> 5) & 63) * inverse + ((color >> 5) & 63) * alpha + 7) / 15;
    unsigned b = ((background & 31) * inverse + (color & 31) * alpha + 7) / 15;
    return (uint16_t)(r << 11 | g << 5 | b);
}

static void cv_text(int x, int y, const felucca_font_t *font, const char *text, uint16_t color)
{
    for (; *text; ++text) {
        unsigned glyph = font_index(font, (uint8_t)*text);
        unsigned width = font->bw[glyph];
        unsigned stride = (width + 1) / 2;
        const uint8_t *shape = font->data + font->off[glyph];
        for (unsigned gy = 0; gy < font->h; ++gy) {
            for (unsigned gx = 0; gx < width; ++gx) {
                unsigned px = (unsigned)(x - font->pad) + gx;
                unsigned py = (unsigned)y + gy;
                unsigned packed = shape[gy * stride + gx / 2];
                unsigned alpha = gx & 1u ? packed & 15u : packed >> 4;
                if (alpha && px < LCD_WIDTH && py < LCD_HEIGHT) {
                    uint16_t bg = pixel_order(pixels[py][px]);
                    pixels[py][px] = pixel_order(alpha == 15 ? color : pixel_blend(bg, color, alpha));
                }
            }
        }
        x += font->adv[glyph];
    }
}

static void cv_begin(unsigned width, unsigned height, uint16_t color)
{
    lcd_sync();
    cv_rect(0, 0, (int)width, (int)height, color);
}

static void cv_blit(unsigned x, unsigned y)
{
    if (x == 0 && y == 0) rows_sent = 0;
}

static void screen_send_stripe(void)
{
    if (rows_sent == LCD_HEIGHT) return;
    lcd_blit(0, rows_sent, LCD_WIDTH, LCD_STRIPE, pixels[rows_sent]);
    rows_sent += LCD_STRIPE;
}
