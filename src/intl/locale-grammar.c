/* Backend-independent ECMA402 Unicode locale grammar.
 * ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd, 2026-10-08.
 * Anchor: sec-iswellformedlanguagetag. */
#include "locale-grammar.h"

typedef struct IntlSlice { const char *text; size_t length; } IntlSlice;
typedef struct IntlCursor { const char *text; size_t length, offset; } IntlCursor;
static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int alpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static int digit(int c) { return c >= '0' && c <= '9'; }
static int alnum(int c) { return alpha(c) || digit(c); }
static int slice_all(IntlSlice s, int (*test)(int))
{
    size_t i;
    for (i = 0; i < s.length; i++) {
        if (!test((unsigned char)s.text[i]))
            return 0;
    }
    return 1;
}
static int slice_equal(IntlSlice a, IntlSlice b)
{
    size_t i;
    if (a.length != b.length)
        return 0;
    for (i = 0; i < a.length; i++) {
        if (lower((unsigned char)a.text[i]) != lower((unsigned char)b.text[i]))
            return 0;
    }
    return 1;
}
static int cursor_next(IntlCursor *cursor, IntlSlice *token)
{
    size_t start = cursor->offset, end = start;
    if (start == cursor->length) return 0;
    while (end < cursor->length && cursor->text[end] != '-') end++;
    token->text = cursor->text + start; token->length = end - start;
    if (!token->length || token->length > 8) return -1;
    cursor->offset = end < cursor->length ? end + 1 : end;
    /* A final separator represents an empty final subtag. */
    if (end < cursor->length && cursor->offset == cursor->length) return -1;
    return 1;
}
static int cursor_peek(IntlCursor cursor, IntlSlice *token) { return cursor_next(&cursor, token); }
static int is_language(IntlSlice s)
{
    return (s.length == 2 || s.length == 3 || (s.length >= 5 && s.length <= 8)) && slice_all(s, alpha);
}
static int is_script(IntlSlice s) { return s.length == 4 && slice_all(s, alpha); }
static int is_region(IntlSlice s)
{
    return (s.length == 2 && slice_all(s, alpha)) || (s.length == 3 && slice_all(s, digit));
}
static int is_variant(IntlSlice s)
{
    return ((s.length >= 5 && s.length <= 8) || (s.length == 4 && digit(s.text[0]))) && slice_all(s, alnum);
}
static int duplicate_variant(const char *start, const char *end, IntlSlice candidate)
{
    while (start < end) {
        const char *p = start;
        while (p < end && *p != '-') p++;
        if (slice_equal((IntlSlice){ start, p - start }, candidate)) return 1;
        start = p + 1;
    }
    return 0;
}
static int parse_language(IntlCursor *cursor)
{
    IntlSlice token; const char *variants = NULL; int r;
    r = cursor_next(cursor, &token); if (r != 1 || !is_language(token)) return 0;
    r = cursor_peek(*cursor, &token);
    if (r < 0) return 0;
    if (r && is_script(token)) { cursor_next(cursor, &token); r = cursor_peek(*cursor, &token); }
    if (r < 0) return 0;
    if (r && is_region(token)) { cursor_next(cursor, &token); r = cursor_peek(*cursor, &token); }
    if (r < 0) return 0;
    while (r && is_variant(token)) {
        if (!variants) variants = token.text;
        else if (duplicate_variant(variants, token.text, token)) return 0;
        cursor_next(cursor, &token); r = cursor_peek(*cursor, &token);
        if (r < 0) return 0;
    }
    return 1;
}
int intl_unicode_type_well_formed(const char *type, size_t length)
{
    IntlCursor cursor = { type, length, 0 }; IntlSlice token; size_t i; int r;
    if (!length) return 0;
    for (i = 0; i < length; i++) if (type[i] != '-' && !alnum((unsigned char)type[i])) return 0;
    while ((r = cursor_next(&cursor, &token)) == 1) if (token.length < 3) return 0;
    return r == 0;
}
int intl_unicode_locale_well_formed(const char *tag, size_t length)
{
    IntlCursor cursor = { tag, length, 0 }; IntlSlice token; unsigned char seen[128] = { 0 }; size_t i; int r;
    if (!length) return 0;
    for (i = 0; i < length; i++) if (tag[i] != '-' && !alnum((unsigned char)tag[i])) return 0;
    if (!parse_language(&cursor)) return 0;
    while ((r = cursor_next(&cursor, &token)) == 1) {
        int singleton, payload = 0;
        if (token.length != 1) return 0;
        singleton = lower((unsigned char)token.text[0]);
        if (singleton == 'x') {
            while ((r = cursor_next(&cursor, &token)) == 1) payload = 1;
            return r == 0 && payload != 0;
        }
        if (seen[singleton]) return 0;
        seen[singleton] = 1;
        if (singleton == 't') {
            r = cursor_peek(cursor, &token); if (r != 1) return 0;
            if (is_language(token)) { if (!parse_language(&cursor)) return 0; payload = 1; }
            while ((r = cursor_peek(cursor, &token)) == 1 && token.length != 1) {
                if (token.length != 2 || !alpha(token.text[0]) || !digit(token.text[1])) return 0;
                cursor_next(&cursor, &token);
                r = cursor_peek(cursor, &token); if (r != 1 || token.length < 3) return 0;
                do { cursor_next(&cursor, &token); r = cursor_peek(cursor, &token); } while (r == 1 && token.length >= 3);
                if (r < 0) return 0;
                payload = 1;
            }
        } else if (singleton == 'u') {
            while ((r = cursor_peek(cursor, &token)) == 1 && token.length >= 3) { cursor_next(&cursor, &token); payload = 1; }
            if (r < 0) return 0;
            while ((r = cursor_peek(cursor, &token)) == 1 && token.length != 1) {
                if (token.length != 2 || !alpha(token.text[1])) return 0;
                cursor_next(&cursor, &token); payload = 1;
                while ((r = cursor_peek(cursor, &token)) == 1 && token.length >= 3) cursor_next(&cursor, &token);
                if (r < 0) return 0;
            }
        } else {
            while ((r = cursor_peek(cursor, &token)) == 1 && token.length != 1) { if (token.length < 2) return 0; cursor_next(&cursor, &token); payload = 1; }
        }
        if (r < 0 || !payload) return 0;
    }
    return r == 0;
}
