/* Bounded formatter for the LCD's integer and fixed-decimal labels. */
#include <stdarg.h>

typedef struct {
    char *out;
    size_t left;
} LcdText;

static void text_char(LcdText *dst, char ch)
{
    if (dst->left > 1) {
        *dst->out++ = ch;
        --dst->left;
    }
}

static void text_uint(LcdText *dst, uint32_t value, unsigned digits)
{
    char reversed[10];
    unsigned count = 0;
    do {
        reversed[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    for (unsigned i = count; i < digits; ++i) text_char(dst, '0');
    while (count) text_char(dst, reversed[--count]);
}

static void lcd_format(char *out, size_t size, const char *format, ...)
{
    if (!size) return;
    LcdText dst = {out, size};
    va_list args;
    va_start(args, format);
    while (*format) {
        char ch = *format++;
        if (ch != '%') {
            text_char(&dst, ch);
            continue;
        }
        bool sign = *format == '+';
        if (sign) ++format;
        unsigned width = 0, decimals = 0;
        if (format[0] == '0' && format[1] >= '1' && format[1] <= '9') {
            width = (unsigned)(format[1] - '0');
            format += 2;
        }
        if (format[0] == '.' && format[1] >= '0' && format[1] <= '2') {
            decimals = (unsigned)(format[1] - '0');
            format += 2;
        }
        if (!*format) break;
        switch (*format++) {
        case '%': text_char(&dst, '%'); break;
        case 'c': text_char(&dst, (char)va_arg(args, int)); break;
        case 's':
            for (const char *p = va_arg(args, const char *); *p; ++p) text_char(&dst, *p);
            break;
        case 'u': text_uint(&dst, va_arg(args, unsigned), width); break;
        case 'd': {
            int n = va_arg(args, int);
            if (sign || n < 0) text_char(&dst, n < 0 ? '-' : '+');
            text_uint(&dst, n < 0 ? 0u - (unsigned)n : (unsigned)n, width);
            break;
        }
        case 'f': {
#ifdef ZIVI_LCD_FLOAT_BITS
            union { uint32_t bits; float value; } encoded = {va_arg(args, uint32_t)};
            float n = encoded.value;
#else
            double n = va_arg(args, double);
#endif
            static const unsigned scales[] = {1, 10, 100};
            unsigned scale = scales[decimals];
            if (sign || n < 0) text_char(&dst, n < 0 ? '-' : '+');
            unsigned rounded = (unsigned)((n < 0 ? -n : n) * scale + 0.5);
            text_uint(&dst, rounded / scale, width);
            if (decimals) {
                text_char(&dst, '.');
                text_uint(&dst, rounded % scale, decimals);
            }
            break;
        }
        }
    }
    *dst.out = 0;
    va_end(args);
}
