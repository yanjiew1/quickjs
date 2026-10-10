/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Exact allocation-free owner grammar extraction. Link this OR
 * number-native.c, never both. */
#include "../number-native.h"
#include <string.h>

enum { TOKEN_NUMBER = 1, TOKEN_CURRENCY, TOKEN_PERCENT, TOKEN_MINUS, TOKEN_PLUS };

static int utf8_scalar(QJSIntlBytes s, size_t *position, uint32_t *scalar)
{
    const unsigned char *p = (const unsigned char *)s.data;
    size_t i = *position;
    uint32_t c;
    unsigned int remaining, j;
    if (!s.data || i >= s.length) return 0;
    c = p[i++];
    if (c < 0x80) remaining = 0;
    else if (c >= 0xc2 && c <= 0xdf) { c &= 0x1f; remaining = 1; }
    else if (c >= 0xe0 && c <= 0xef) { c &= 0x0f; remaining = 2; }
    else if (c >= 0xf0 && c <= 0xf4) { c &= 7; remaining = 3; }
    else return 0;
    if (remaining > s.length - i) return 0;
    for (j = 0; j < remaining; j++) {
        if ((p[i] & 0xc0) != 0x80) return 0;
        c = (c << 6) | (p[i++] & 0x3f);
    }
    if ((remaining == 1 && c < 0x80) || (remaining == 2 && c < 0x800) ||
        (remaining == 3 && c < 0x10000) || c > 0x10ffff ||
        (c >= 0xd800 && c <= 0xdfff) || c == 0)
        return 0;
    *position = i; *scalar = c;
    return 1;
}
static int utf8_valid(QJSIntlBytes s, int empty)
{
    size_t i = 0;
    uint32_t c;
    if (!s.length) return empty;
    while (i < s.length) if (!utf8_scalar(s, &i, &c)) return 0;
    return 1;
}
static unsigned int token(QJSIntlBytes s, size_t position, size_t *end)
{
    static const char *const names[] = {
        "{number}", "{currency}", "{percentSign}", "{minusSign}", "{plusSign}"
    };
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        size_t n = strlen(names[i]);
        if (n <= s.length - position && !memcmp(s.data + position, names[i], n)) {
            *end = position + n;
            return (unsigned int)i + 1;
        }
    }
    return 0;
}
int qjs_intl_number_template_validate(QJSIntlBytes s, unsigned int unit_mode)
{
    size_t i = 0, end;
    unsigned int counts[6] = { 0 }, t;
    uint32_t c;
    if (unit_mode > 3 || !utf8_valid(s, 0)) return 0;
    while (i < s.length) {
        if (s.data[i] == '{') {
            t = token(s, i, &end);
            if (!t || ++counts[t] > 1 || ((unit_mode == 1 || unit_mode == 3) && t != TOKEN_NUMBER) ||
                (unit_mode == 2 && t != TOKEN_NUMBER && t != TOKEN_CURRENCY)) return 0;
            i = end;
        } else {
            if (s.data[i] == '}' || !utf8_scalar(s, &i, &c)) return 0;
        }
    }
    return (counts[TOKEN_NUMBER] == 1 || unit_mode == 3) &&
        (unit_mode != 2 || counts[TOKEN_CURRENCY] == 1);
}
int qjs_intl_number_compact_template_validate(QJSIntlBytes s, int32_t exponent)
{
    size_t i = 0, end;
    unsigned int numbers = 0;
    uint32_t c;
    int affix = 0;
    if (!utf8_valid(s, 0)) return 0;
    while (i < s.length) {
        if (s.data[i] == '{') {
            if (token(s, i, &end) != TOKEN_NUMBER || ++numbers > 1) return 0;
            i = end;
        } else {
            if (s.data[i] == '}' || !utf8_scalar(s, &i, &c)) return 0;
            affix = 1;
        }
    }
    return exponent ? affix : numbers == 1 && !affix;
}
