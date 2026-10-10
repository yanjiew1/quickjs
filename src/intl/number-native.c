/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number-native.h"
#include "number-compact.h"
#include "number-unit.h"
#include "number-range.h"
#include <limits.h>
#include <string.h>

struct QJSIntlNativeNumber {
    QJSIntlAllocator allocator;
    QJSIntlNumberOptions options;
    QJSIntlNumberData data;
    QJSIntlNativePlural *owned_cardinal;
};
typedef struct NumberBuilder {
    const QJSIntlNativeNumber *number;
    QJSIntlFormatted value;
    size_t text_capacity, part_capacity;
    QJSIntlPartSource source;
} NumberBuilder;
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
static QJSIntlStatus copy_string(const QJSIntlAllocator *a, QJSIntlBytes in,
                                 QJSIntlBytes *out)
{
    char *p;
    out->data = NULL; out->length = 0;
    if (!in.length) return QJS_INTL_OK;
    if (!in.data || in.length == SIZE_MAX) return QJS_INTL_OVERFLOW;
    p = a->malloc(a->opaque, in.length + 1);
    if (!p) return QJS_INTL_NO_MEMORY;
    memcpy(p, in.data, in.length); p[in.length] = 0;
    out->data = p; out->length = in.length;
    return QJS_INTL_OK;
}
static void free_string(const QJSIntlAllocator *a, QJSIntlBytes *s)
{
    if (s->data) a->free(a->opaque, (void *)s->data);
    s->data = NULL; s->length = 0;
}
void qjs_intl_native_number_result_clear(const QJSIntlNativeNumber *n,
                                        QJSIntlFormatted *value)
{
    if (!n || !value) return;
    if (value->text) n->allocator.free(n->allocator.opaque, value->text);
    if (value->parts) n->allocator.free(n->allocator.opaque, value->parts);
    memset(value, 0, sizeof(*value));
}
void qjs_intl_native_number_close(QJSIntlNativeNumber *n)
{
    size_t i, j;
    QJSIntlAllocator a;
    if (!n) return;
    a = n->allocator;
    free_string(&a, &n->options.currency); free_string(&a, &n->options.unit);
    for (i = 0; i < QJS_INTL_NUMBER_SYMBOL_COUNT; i++)
        free_string(&a, &n->data.symbols[i]);
    free_string(&a, &n->data.pattern.zero);
    free_string(&a, &n->data.pattern.negative);
    free_string(&a, &n->data.pattern.positive);
    free_string(&a, &n->data.alpha_pattern.zero);
    free_string(&a, &n->data.alpha_pattern.negative);
    free_string(&a, &n->data.alpha_pattern.positive);
    free_string(&a, &n->data.currency_symbol);
    free_string(&a, &n->data.currency_narrow_symbol);
    for (i = 0; i < QJS_INTL_PLURAL_CATEGORY_COUNT; i++) {
        free_string(&a, &n->data.currency_names[i]);
        free_string(&a, &n->data.currency_name_patterns[i]);
        free_string(&a, &n->data.unit_patterns[i]);
    }
    free_string(&a, &n->data.before_currency); free_string(&a, &n->data.after_currency);
    free_string(&a, &n->data.approximately); free_string(&a, &n->data.range);
    if (n->data.classes) a.free(a.opaque, (void *)n->data.classes);
    for (i = 0; i < n->data.compact_count; i++) {
        QJSIntlNumberCompact *row = (QJSIntlNumberCompact *)n->data.compact + i;
        for (j = 0; j < QJS_INTL_PLURAL_CATEGORY_COUNT; j++)
            free_string(&a, &row->patterns[j]);
        free_string(&a, &row->exact_one);
    }
    if (n->data.compact) a.free(a.opaque, (void *)n->data.compact);
    qjs_intl_native_plural_close(n->owned_cardinal);
    a.free(a.opaque, n);
}
void qjs_intl_native_number_take_cardinal(QJSIntlNativeNumber *n,
                                         QJSIntlNativePlural *cardinal)
{
    if (n) { n->owned_cardinal = cardinal; n->data.cardinal = cardinal; }
}
static int pattern_valid(const QJSIntlNumberPattern *p, int optional)
{
    if (!p->zero.length && optional)
        return !p->negative.length && !p->positive.length;
    return qjs_intl_number_template_validate(p->zero, 0) &&
        qjs_intl_number_template_validate(p->negative, 0) &&
        qjs_intl_number_template_validate(p->positive, 0) &&
        p->primary_group <= 9 && p->secondary_group <= 9 &&
        (!!p->primary_group == !!p->secondary_group);
}
QJSIntlStatus qjs_intl_native_number_open(const QJSIntlAllocator *a,
    const QJSIntlNumberOptions *o, const QJSIntlNumberData *d, QJSIntlNativeNumber **out)
{
    QJSIntlNativeNumber *n;
    QJSIntlStatus status;
    size_t i, j;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !a->malloc || !a->free || !o || !d ||
        (unsigned int)o->style > QJS_INTL_NUMBER_UNIT ||
        (unsigned int)o->notation > QJS_INTL_NUMBER_COMPACT ||
        (unsigned int)o->grouping > QJS_INTL_NUMBER_GROUP_MIN2 ||
        (unsigned int)o->sign_display > QJS_INTL_NUMBER_SIGN_NEGATIVE ||
        (unsigned int)o->currency_display > QJS_INTL_CURRENCY_NAME ||
        (unsigned int)o->unit_display > QJS_INTL_UNIT_NARROW ||
        (unsigned int)o->compact_display > QJS_INTL_COMPACT_LONG ||
        o->currency_accounting > 1 || !o->maximum_output_length ||
        qjs_intl_decimal_options_validate(&o->digits) != QJS_INTL_OK)
        return QJS_INTL_INVALID_ARGUMENT;
    if (o->notation == QJS_INTL_NUMBER_COMPACT && (!d->compact_count || !d->cardinal))
        return QJS_INTL_UNSUPPORTED;
    if (o->style == QJS_INTL_NUMBER_CURRENCY) {
        if (o->currency.length != 3 || !o->currency.data) return QJS_INTL_INVALID_ARGUMENT;
        for (i = 0; i < 3; i++)
            if (o->currency.data[i] < 'A' || o->currency.data[i] > 'Z')
                return QJS_INTL_INVALID_ARGUMENT;
    }
    if (o->style == QJS_INTL_NUMBER_UNIT) {
        if (!qjs_intl_number_unit_validate(o->unit)) return QJS_INTL_INVALID_ARGUMENT;
    }
    if (d->minimum_grouping_digits < 1 || d->minimum_grouping_digits > 9 ||
        !pattern_valid(&d->pattern, 0) || !pattern_valid(&d->alpha_pattern, 1))
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < 10; i++) {
        if (d->digits[i] > 0x10ffff || !d->digits[i] ||
            (d->digits[i] >= 0xd800 && d->digits[i] <= 0xdfff)) return QJS_INTL_DATA_ERROR;
        for (j = 0; j < i; j++) if (d->digits[i] == d->digits[j]) return QJS_INTL_DATA_ERROR;
    }
    for (i = 0; i < QJS_INTL_NUMBER_SYMBOL_COUNT; i++)
        if (!utf8_valid(d->symbols[i], 0)) return QJS_INTL_DATA_ERROR;
    if (!utf8_valid(d->currency_symbol, 1) || !utf8_valid(d->currency_narrow_symbol, 1) ||
        !utf8_valid(d->before_currency, 1) || !utf8_valid(d->after_currency, 1) ||
        (d->class_count && !d->classes)) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < d->class_count; i++) {
        const QJSIntlNumberScalarRange *r = &d->classes[i];
        if (r->first > r->last || r->last > 0x10ffff || r->flags > 7 || r->flags == 2 ||
            (r->first <= 0xdfff && r->last >= 0xd800) ||
            (i && d->classes[i - 1].last >= r->first)) return QJS_INTL_DATA_ERROR;
    }
    if (d->compact_count && !d->compact) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < d->compact_count; i++) {
        const QJSIntlNumberCompact *row = &d->compact[i];
        if (row->magnitude < 0 || row->magnitude > QJS_INTL_DECIMAL_EXPONENT_LIMIT ||
            row->exponent < 0 || row->exponent > row->magnitude ||
            (i && d->compact[i - 1].magnitude >= row->magnitude)) return QJS_INTL_DATA_ERROR;
        for (j = 0; j < QJS_INTL_PLURAL_CATEGORY_COUNT; j++)
            if (!qjs_intl_number_compact_template_validate(row->patterns[j], row->exponent))
                return QJS_INTL_DATA_ERROR;
        if (row->exact_one.length &&
            !qjs_intl_number_compact_template_validate(row->exact_one, row->exponent))
            return QJS_INTL_DATA_ERROR;
    }
    if (!!d->approximately.length != !!d->range.length) return QJS_INTL_DATA_ERROR;
    if (d->range.length) {
        QJSIntlNumberRangeSlices pieces;
        if (!utf8_valid(d->approximately, 0) || !utf8_valid(d->range, 0) ||
            !qjs_intl_number_range_slices(d->approximately, 0, &pieces) ||
            !qjs_intl_number_range_slices(d->range, 1, &pieces)) return QJS_INTL_DATA_ERROR;
    }
    if (o->style == QJS_INTL_NUMBER_UNIT ||
        (o->style == QJS_INTL_NUMBER_CURRENCY && o->currency_display == QJS_INTL_CURRENCY_NAME)) {
        if (!d->cardinal) return QJS_INTL_UNSUPPORTED;
        for (i = 0; i < QJS_INTL_PLURAL_CATEGORY_COUNT; i++) {
            if (o->style == QJS_INTL_NUMBER_UNIT) {
                if (!qjs_intl_number_template_validate(d->unit_patterns[i], 3))
                    return QJS_INTL_DATA_ERROR;
            } else if (!utf8_valid(d->currency_names[i], 0) ||
                       !qjs_intl_number_template_validate(d->currency_name_patterns[i], 2))
                return QJS_INTL_DATA_ERROR;
        }
    }
    n = a->malloc(a->opaque, sizeof(*n));
    if (!n) return QJS_INTL_NO_MEMORY;
    memset(n, 0, sizeof(*n)); n->allocator = *a; n->options = *o;
    n->options.currency.data = NULL; n->options.currency.length = 0;
    n->options.unit.data = NULL; n->options.unit.length = 0;
    memcpy(n->data.digits, d->digits, sizeof(d->digits));
    n->data.minimum_grouping_digits = d->minimum_grouping_digits;
    n->data.pattern.primary_group = d->pattern.primary_group;
    n->data.pattern.secondary_group = d->pattern.secondary_group;
    n->data.alpha_pattern.primary_group = d->alpha_pattern.primary_group;
    n->data.alpha_pattern.secondary_group = d->alpha_pattern.secondary_group;
    n->data.cardinal = d->cardinal;
#define COPY_STRING(input, output) do { status = copy_string(a, (input), &(output)); \
    if (status != QJS_INTL_OK) goto fail; } while (0)
    COPY_STRING(o->currency, n->options.currency); COPY_STRING(o->unit, n->options.unit);
    for (i = 0; i < QJS_INTL_NUMBER_SYMBOL_COUNT; i++)
        COPY_STRING(d->symbols[i], n->data.symbols[i]);
    COPY_STRING(d->pattern.zero, n->data.pattern.zero);
    COPY_STRING(d->pattern.negative, n->data.pattern.negative);
    COPY_STRING(d->pattern.positive, n->data.pattern.positive);
    COPY_STRING(d->alpha_pattern.zero, n->data.alpha_pattern.zero);
    COPY_STRING(d->alpha_pattern.negative, n->data.alpha_pattern.negative);
    COPY_STRING(d->alpha_pattern.positive, n->data.alpha_pattern.positive);
    COPY_STRING(d->currency_symbol, n->data.currency_symbol);
    COPY_STRING(d->currency_narrow_symbol, n->data.currency_narrow_symbol);
    for (i = 0; i < QJS_INTL_PLURAL_CATEGORY_COUNT; i++) {
        COPY_STRING(d->currency_names[i], n->data.currency_names[i]);
        COPY_STRING(d->currency_name_patterns[i], n->data.currency_name_patterns[i]);
        COPY_STRING(d->unit_patterns[i], n->data.unit_patterns[i]);
    }
    COPY_STRING(d->before_currency, n->data.before_currency);
    COPY_STRING(d->after_currency, n->data.after_currency);
    COPY_STRING(d->approximately, n->data.approximately); COPY_STRING(d->range, n->data.range);
    if (d->compact_count) {
        QJSIntlNumberCompact *rows;
        if (d->compact_count > SIZE_MAX / sizeof(*rows)) { status = QJS_INTL_OVERFLOW; goto fail; }
        rows = a->malloc(a->opaque, d->compact_count * sizeof(*rows));
        if (!rows) { status = QJS_INTL_NO_MEMORY; goto fail; }
        memset(rows, 0, d->compact_count * sizeof(*rows));
        n->data.compact = rows; n->data.compact_count = d->compact_count;
        for (i = 0; i < d->compact_count; i++) {
            rows[i].magnitude = d->compact[i].magnitude;
            rows[i].exponent = d->compact[i].exponent;
            for (j = 0; j < QJS_INTL_PLURAL_CATEGORY_COUNT; j++)
                COPY_STRING(d->compact[i].patterns[j], rows[i].patterns[j]);
            COPY_STRING(d->compact[i].exact_one, rows[i].exact_one);
        }
    }
#undef COPY_STRING
    if (d->class_count) {
        QJSIntlNumberScalarRange *ranges;
        if (d->class_count > SIZE_MAX / sizeof(*ranges)) { status = QJS_INTL_OVERFLOW; goto fail; }
        ranges = a->malloc(a->opaque, d->class_count * sizeof(*ranges));
        if (!ranges) { status = QJS_INTL_NO_MEMORY; goto fail; }
        memcpy(ranges, d->classes, d->class_count * sizeof(*ranges));
        n->data.classes = ranges; n->data.class_count = d->class_count;
    }
    *out = n;
    return QJS_INTL_OK;
fail:
    qjs_intl_native_number_close(n);
    return status;
}
QJSIntlStatus qjs_intl_native_number_resolved_options(const QJSIntlNativeNumber *n,
                                                     QJSIntlNumberOptions *out)
{
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!n) return QJS_INTL_INVALID_ARGUMENT;
    *out = n->options; /* currency/unit slices borrowed until close */
    return QJS_INTL_OK;
}
static QJSIntlStatus reserve_text(NumberBuilder *b, size_t count)
{
    size_t needed, capacity;
    uint16_t *p;
    const QJSIntlAllocator *a = &b->number->allocator;
    if (count > b->number->options.maximum_output_length - b->value.length)
        return QJS_INTL_OVERFLOW;
    needed = b->value.length + count;
    if (needed <= b->text_capacity) return QJS_INTL_OK;
    capacity = b->text_capacity ? b->text_capacity : 32;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) { capacity = needed; break; }
        capacity *= 2;
    }
    if (capacity > b->number->options.maximum_output_length)
        capacity = b->number->options.maximum_output_length;
    if (capacity > SIZE_MAX / sizeof(*p)) return QJS_INTL_OVERFLOW;
    p = a->malloc(a->opaque, capacity * sizeof(*p));
    if (!p) return QJS_INTL_NO_MEMORY;
    if (b->value.length) memcpy(p, b->value.text, b->value.length * sizeof(*p));
    if (b->value.text) a->free(a->opaque, b->value.text);
    b->value.text = p; b->text_capacity = capacity;
    return QJS_INTL_OK;
}
static QJSIntlStatus part(NumberBuilder *b, size_t start, QJSIntlPartType type)
{
    QJSIntlPart *p;
    size_t capacity;
    const QJSIntlAllocator *a = &b->number->allocator;
    if (start == b->value.length) return QJS_INTL_OK;
    if (b->value.part_count) {
        p = &b->value.parts[b->value.part_count - 1];
        if (p->end == start && p->type == type && p->source == b->source) {
            p->end = b->value.length; return QJS_INTL_OK;
        }
    }
    if (b->value.part_count == b->part_capacity) {
        if (b->part_capacity > SIZE_MAX / 2) return QJS_INTL_OVERFLOW;
        capacity = b->part_capacity ? b->part_capacity * 2 : 16;
        if (capacity > SIZE_MAX / sizeof(*p)) return QJS_INTL_OVERFLOW;
        p = a->malloc(a->opaque, capacity * sizeof(*p));
        if (!p) return QJS_INTL_NO_MEMORY;
        if (b->value.part_count)
            memcpy(p, b->value.parts, b->value.part_count * sizeof(*p));
        if (b->value.parts) a->free(a->opaque, b->value.parts);
        b->value.parts = p; b->part_capacity = capacity;
    }
    p = &b->value.parts[b->value.part_count++];
    memset(p, 0, sizeof(*p)); p->start = start; p->end = b->value.length;
    p->type = type; p->source = b->source;
    return QJS_INTL_OK;
}
static QJSIntlStatus scalar(NumberBuilder *b, uint32_t c, QJSIntlPartType type)
{
    size_t start = b->value.length;
    QJSIntlStatus s = reserve_text(b, c > 0xffff ? 2 : 1);
    if (s != QJS_INTL_OK) return s;
    if (c > 0xffff) {
        c -= 0x10000; b->value.text[b->value.length++] = (uint16_t)(0xd800 + (c >> 10));
        b->value.text[b->value.length++] = (uint16_t)(0xdc00 + (c & 1023));
    } else b->value.text[b->value.length++] = (uint16_t)c;
    return part(b, start, type);
}
static QJSIntlStatus bytes(NumberBuilder *b, QJSIntlBytes text, QJSIntlPartType type)
{
    size_t i = 0;
    uint32_t c;
    QJSIntlStatus s;
    while (i < text.length) {
        if (!utf8_scalar(text, &i, &c)) return QJS_INTL_DATA_ERROR;
        s = scalar(b, c, type); if (s != QJS_INTL_OK) return s;
    }
    return QJS_INTL_OK;
}
static QJSIntlStatus append_result(NumberBuilder *b, const QJSIntlFormatted *text)
{
    size_t i, j, start;
    QJSIntlStatus s;
    for (i = 0; i < text->part_count; i++) {
        const QJSIntlPart *p = &text->parts[i];
        start = b->value.length;
        if (p->end < p->start || p->end > text->length) return QJS_INTL_DATA_ERROR;
        s = reserve_text(b, p->end - p->start); if (s != QJS_INTL_OK) return s;
        for (j = p->start; j < p->end; j++) b->value.text[b->value.length++] = text->text[j];
        s = part(b, start, p->type); if (s != QJS_INTL_OK) return s;
    }
    return QJS_INTL_OK;
}
static int zero(const QJSIntlDecimal *d)
{
    return d->digits && d->length == 1 && d->digits[0] == '0';
}
static int raw_valid(const QJSIntlDecimalResult *r)
{
    size_t i;
    unsigned int decimal_count = 0;
    if (!r || !r->text || !r->length || !r->rounded.digits || !r->rounded.length ||
        r->rounded.negative > 1) return 0;
    for (i = 0; i < r->length; i++) {
        char c = r->text[i];
        if (c == '.' && i && i + 1 < r->length && !decimal_count) decimal_count++;
        else if (c < '0' || c > '9') return 0;
    }
    return 1;
}
static unsigned int sign_pattern(const QJSIntlNativeNumber *n, int negative, int is_zero)
{
    switch (n->options.sign_display) {
    case QJS_INTL_NUMBER_SIGN_NEVER: return 0;
    case QJS_INTL_NUMBER_SIGN_AUTO: return negative ? 1 : 0;
    case QJS_INTL_NUMBER_SIGN_ALWAYS: return negative ? 1 : 2;
    case QJS_INTL_NUMBER_SIGN_EXCEPT_ZERO: return is_zero ? 0 : negative ? 1 : 2;
    case QJS_INTL_NUMBER_SIGN_NEGATIVE: return negative && !is_zero ? 1 : 0;
    }
    return 0;
}
static QJSIntlBytes selected_pattern(const QJSIntlNumberPattern *p, unsigned int sign)
{
    return sign == 1 ? p->negative : sign == 2 ? p->positive : p->zero;
}
static uint32_t class_flags(const QJSIntlNativeNumber *n, uint32_t c)
{
    size_t lo = 0, hi = n->data.class_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const QJSIntlNumberScalarRange *r = &n->data.classes[mid];
        if (c < r->first) hi = mid;
        else if (c > r->last) lo = mid + 1;
        else return r->flags;
    }
    return 2;
}
static uint32_t bytes_edge(QJSIntlBytes s, int last)
{
    uint32_t c = 0, current;
    size_t i = 0;
    while (i < s.length) {
        if (!utf8_scalar(s, &i, &current)) return 0;
        c = current;
        if (!last) break;
    }
    return c;
}
static uint32_t text_edge(const QJSIntlFormatted *f, int last)
{
    size_t i;
    uint32_t c;
    if (!f->length) return 0;
    i = last ? f->length - 1 : 0; c = f->text[i];
    if (!last && c >= 0xd800 && c <= 0xdbff && i + 1 < f->length)
        c = 0x10000 + ((c - 0xd800) << 10) + (f->text[i + 1] - 0xdc00);
    else if (last && c >= 0xdc00 && c <= 0xdfff && i)
        c = 0x10000 + ((f->text[i - 1] - 0xd800) << 10) + (c - 0xdc00);
    return c;
}
static int touches(QJSIntlBytes pattern, int currency_before)
{
    const char *needle = currency_before ? "{currency}{number}" : "{number}{currency}";
    size_t i, length = strlen(needle);
    for (i = 0; i + length <= pattern.length; i++)
        if (!memcmp(pattern.data + i, needle, length)) return 1;
    return 0;
}
static int whitespace(uint32_t c)
{
    return (c >= 9 && c <= 13) || c == 0x20 || c == 0x85 || c == 0xa0 ||
        c == 0x1680 || (c >= 0x2000 && c <= 0x200a) || c == 0x2028 ||
        c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000 ||
        c == 0x061c || c == 0x200e || c == 0x200f ||
        (c >= 0x2066 && c <= 0x2069);
}
static QJSIntlStatus label_literal(NumberBuilder *b, QJSIntlBytes text, QJSIntlPartType type)
{
    size_t i = 0, start = 0, first = text.length, last = 0;
    uint32_t c;
    QJSIntlStatus s;
    while (i < text.length) {
        start = i;
        if (!utf8_scalar(text, &i, &c)) return QJS_INTL_DATA_ERROR;
        if (!whitespace(c)) { if (first == text.length) first = start; last = i; }
    }
    if (first == text.length) return bytes(b, text, QJS_INTL_PART_LITERAL);
    { QJSIntlBytes slice = { text.data, first };
      s = bytes(b, slice, QJS_INTL_PART_LITERAL); if (s != QJS_INTL_OK) return s;
      slice.data = text.data + first; slice.length = last - first;
      s = bytes(b, slice, type); if (s != QJS_INTL_OK) return s;
      slice.data = text.data + last; slice.length = text.length - last;
      return bytes(b, slice, QJS_INTL_PART_LITERAL); }
}
static QJSIntlStatus apply_template(NumberBuilder *b, QJSIntlBytes pattern,
    const QJSIntlFormatted *numeric, QJSIntlBytes currency, int unit_mode, int spacing)
{
    size_t i = 0, start, end;
    unsigned int t;
    QJSIntlStatus s;
    while (i < pattern.length) {
        start = i;
        if (pattern.data[i] != '{') {
            while (i < pattern.length && pattern.data[i] != '{') i++;
            { QJSIntlBytes literal = { pattern.data + start, i - start };
              s = unit_mode ? label_literal(b, literal, unit_mode == 2 ? QJS_INTL_PART_COMPACT : QJS_INTL_PART_UNIT) :
                  bytes(b, literal, QJS_INTL_PART_LITERAL); }
        } else {
            t = token(pattern, i, &end);
            if (!t) return QJS_INTL_DATA_ERROR;
            if (t == TOKEN_NUMBER) {
                s = append_result(b, numeric);
                if (s == QJS_INTL_OK && spacing && end < pattern.length &&
                    token(pattern, end, &start) == TOKEN_CURRENCY &&
                    (class_flags(b->number, text_edge(numeric, 1)) & 4) &&
                    (class_flags(b->number, bytes_edge(currency, 0)) & 2))
                    s = bytes(b, b->number->data.before_currency, QJS_INTL_PART_LITERAL);
            } else if (t == TOKEN_CURRENCY) {
                s = bytes(b, currency, QJS_INTL_PART_CURRENCY);
                if (s == QJS_INTL_OK && spacing && end < pattern.length &&
                    token(pattern, end, &start) == TOKEN_NUMBER &&
                    (class_flags(b->number, bytes_edge(currency, 1)) & 2) &&
                    (class_flags(b->number, text_edge(numeric, 0)) & 4))
                    s = bytes(b, b->number->data.after_currency, QJS_INTL_PART_LITERAL);
            } else {
                unsigned int symbol_index = t == TOKEN_PERCENT ? QJS_INTL_NUMBER_SYMBOL_PERCENT :
                    t == TOKEN_MINUS ? QJS_INTL_NUMBER_SYMBOL_MINUS : QJS_INTL_NUMBER_SYMBOL_PLUS;
                QJSIntlPartType type = t == TOKEN_PERCENT ? QJS_INTL_PART_PERCENT_SIGN :
                    t == TOKEN_MINUS ? QJS_INTL_PART_MINUS_SIGN : QJS_INTL_PART_PLUS_SIGN;
                s = bytes(b, b->number->data.symbols[symbol_index], type);
            }
            i = end;
        }
        if (s != QJS_INTL_OK) return s;
    }
    return QJS_INTL_OK;
}
static QJSIntlStatus numeric(const QJSIntlNativeNumber *n, const QJSIntlDecimalResult *raw,
    QJSIntlMathematicalKind kind, int32_t exponent, QJSIntlFormatted *out)
{
    NumberBuilder b;
    size_t length, point, i, remaining;
    unsigned int primary = n->data.pattern.primary_group;
    unsigned int secondary = n->data.pattern.secondary_group, minimum;
    int grouped;
    QJSIntlStatus s = QJS_INTL_OK;
    memset(&b, 0, sizeof(b)); b.number = n;
    if (kind == QJS_INTL_NAN || kind == QJS_INTL_POSITIVE_INFINITY || kind == QJS_INTL_NEGATIVE_INFINITY) {
        s = bytes(&b, n->data.symbols[kind == QJS_INTL_NAN ? QJS_INTL_NUMBER_SYMBOL_NAN :
            QJS_INTL_NUMBER_SYMBOL_INFINITY], kind == QJS_INTL_NAN ? QJS_INTL_PART_NAN : QJS_INTL_PART_INFINITY);
        goto done;
    }
    if (!raw_valid(raw)) { s = QJS_INTL_INVALID_ARGUMENT; goto done; }
    length = raw->length; point = length;
    for (i = 0; i < length; i++) if (raw->text[i] == '.') { point = i; break; }
    minimum = n->options.grouping == QJS_INTL_NUMBER_GROUP_ALWAYS ? 1 :
        n->options.grouping == QJS_INTL_NUMBER_GROUP_MIN2 ? 2 : n->data.minimum_grouping_digits;
    grouped = n->options.grouping != QJS_INTL_NUMBER_GROUP_OFF && primary &&
        point >= (size_t)primary + minimum;
    for (i = 0; i < point; i++) {
        remaining = point - i;
        if (grouped && i && (remaining == primary ||
            (remaining > primary && (remaining - primary) % secondary == 0))) {
            unsigned int index = n->options.style == QJS_INTL_NUMBER_CURRENCY ?
                QJS_INTL_NUMBER_SYMBOL_CURRENCY_GROUP : QJS_INTL_NUMBER_SYMBOL_GROUP;
            s = bytes(&b, n->data.symbols[index], QJS_INTL_PART_GROUP); if (s != QJS_INTL_OK) goto done;
        }
        s = scalar(&b, n->data.digits[(unsigned int)(raw->text[i] - '0')], QJS_INTL_PART_INTEGER);
        if (s != QJS_INTL_OK) goto done;
    }
    if (point < length) {
        unsigned int index = n->options.style == QJS_INTL_NUMBER_CURRENCY ?
            QJS_INTL_NUMBER_SYMBOL_CURRENCY_DECIMAL : QJS_INTL_NUMBER_SYMBOL_DECIMAL;
        s = bytes(&b, n->data.symbols[index], QJS_INTL_PART_DECIMAL); if (s != QJS_INTL_OK) goto done;
        for (i = point + 1; i < length; i++) {
            s = scalar(&b, n->data.digits[(unsigned int)(raw->text[i] - '0')], QJS_INTL_PART_FRACTION);
            if (s != QJS_INTL_OK) goto done;
        }
    }
    if (n->options.notation == QJS_INTL_NUMBER_SCIENTIFIC ||
        n->options.notation == QJS_INTL_NUMBER_ENGINEERING) {
        uint32_t absolute = exponent < 0 ? (uint32_t)(-(int64_t)exponent) : (uint32_t)exponent;
        char digits[12]; size_t used = 0;
        s = bytes(&b, n->data.symbols[QJS_INTL_NUMBER_SYMBOL_EXPONENTIAL], QJS_INTL_PART_EXPONENT_SEPARATOR);
        if (s != QJS_INTL_OK) goto done;
        if (exponent < 0) {
            s = bytes(&b, n->data.symbols[QJS_INTL_NUMBER_SYMBOL_MINUS], QJS_INTL_PART_EXPONENT_MINUS_SIGN);
            if (s != QJS_INTL_OK) goto done;
        }
        do { digits[used++] = (char)('0' + absolute % 10); absolute /= 10; } while (absolute);
        while (used) {
            s = scalar(&b, n->data.digits[(unsigned int)(digits[--used] - '0')], QJS_INTL_PART_EXPONENT_INTEGER);
            if (s != QJS_INTL_OK) goto done;
        }
    }
done:
    if (s == QJS_INTL_OK) *out = b.value;
    else qjs_intl_native_number_result_clear(n, &b.value);
    return s;
}
static QJSIntlStatus render(const QJSIntlNativeNumber *n, const QJSIntlDecimalResult *raw,
    QJSIntlMathematicalKind kind, int32_t exponent, const QJSIntlNumberCompact *compact,
    QJSIntlFormatted *out)
{
    QJSIntlFormatted digits = { 0 }, notation_number = { 0 }, signed_number = { 0 };
    NumberBuilder b;
    QJSIntlBytes pattern, currency = { NULL, 0 };
    QJSIntlPluralCategory category = QJS_INTL_PLURAL_OTHER;
    QJSIntlStatus s;
    unsigned int sign;
    int finite = kind == QJS_INTL_FINITE || kind == QJS_INTL_NEGATIVE_ZERO;
    int negative = finite ? raw->rounded.negative : kind == QJS_INTL_NEGATIVE_INFINITY;
    int is_zero = finite ? zero(&raw->rounded) : kind == QJS_INTL_NAN;
    int label_style = n->options.style == QJS_INTL_NUMBER_UNIT ||
        (n->options.style == QJS_INTL_NUMBER_CURRENCY && n->options.currency_display == QJS_INTL_CURRENCY_NAME);
    memset(&b, 0, sizeof(b)); b.number = n;
    if ((label_style || n->options.notation == QJS_INTL_NUMBER_COMPACT) && finite) {
        JSIntlPluralOperands operands = { raw->text, raw->length, exponent };
        s = qjs_intl_native_plural_select_operands(n->data.cardinal, &operands, &category);
        if (s != QJS_INTL_OK) goto done;
    }
    s = numeric(n, raw, kind, exponent, &digits); if (s != QJS_INTL_OK) goto done;
    if (finite && exponent && compact) {
        QJSIntlBytes notation = compact->exact_one.length &&
            qjs_intl_number_compact_is_one(&raw->rounded) ? compact->exact_one : compact->patterns[category];
        s = apply_template(&b, notation, &digits, currency, 2, 0);
        if (s != QJS_INTL_OK) goto done;
        notation_number = b.value; memset(&b.value, 0, sizeof(b.value));
        b.text_capacity = 0; b.part_capacity = 0;
    }
    sign = sign_pattern(n, negative, is_zero); pattern = selected_pattern(&n->data.pattern, sign);
    if (n->options.style == QJS_INTL_NUMBER_CURRENCY && !label_style) {
        currency = n->options.currency_display == QJS_INTL_CURRENCY_CODE ? n->options.currency :
            n->options.currency_display == QJS_INTL_CURRENCY_NARROW_SYMBOL ? n->data.currency_narrow_symbol :
            n->data.currency_symbol;
        if (!currency.length) currency = n->options.currency; /* explicit ECMA fallback code */
        if (n->data.alpha_pattern.zero.length &&
            ((touches(pattern, 1) && (class_flags(n, bytes_edge(currency, 1)) & 1)) ||
             (touches(pattern, 0) && (class_flags(n, bytes_edge(currency, 0)) & 1))))
            pattern = selected_pattern(&n->data.alpha_pattern, sign);
    }
    s = apply_template(&b, pattern, notation_number.length ? &notation_number : &digits,
                       currency, 0, !label_style);
    if (s != QJS_INTL_OK) goto done;
    if (label_style) {
        signed_number = b.value; memset(&b.value, 0, sizeof(b.value));
        b.text_capacity = 0; b.part_capacity = 0;
        if (n->options.style == QJS_INTL_NUMBER_UNIT) {
            s = apply_template(&b, n->data.unit_patterns[category], &signed_number, currency, 1, 0);
            if (s == QJS_INTL_OK &&
                !qjs_intl_number_template_validate(n->data.unit_patterns[category], 1)) {
                /* A CLDR singular/dual template can omit the numeric field.
                 * Keep its unit text while applying signDisplay to the final
                 * label, so negative/always signs are never discarded. */
                qjs_intl_native_number_result_clear(n, &signed_number);
                signed_number = b.value; memset(&b.value, 0, sizeof(b.value));
                b.text_capacity = 0; b.part_capacity = 0;
                s = apply_template(&b, pattern, &signed_number, currency, 0, 0);
            }
        } else {
            currency = n->data.currency_names[category];
            s = apply_template(&b, n->data.currency_name_patterns[category], &signed_number, currency, 0, 0);
        }
    }
done:
    qjs_intl_native_number_result_clear(n, &digits);
    qjs_intl_native_number_result_clear(n, &notation_number);
    qjs_intl_native_number_result_clear(n, &signed_number);
    if (s == QJS_INTL_OK) *out = b.value;
    else qjs_intl_native_number_result_clear(n, &b.value);
    return s;
}
QJSIntlStatus qjs_intl_native_number_format_rounded(const QJSIntlNativeNumber *n,
    const QJSIntlDecimalResult *raw, QJSIntlFormatted *out)
{
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!n || !raw_valid(raw)) return QJS_INTL_INVALID_ARGUMENT;
    if (n->options.style != QJS_INTL_NUMBER_DECIMAL || n->options.notation != QJS_INTL_NUMBER_STANDARD)
        return QJS_INTL_UNSUPPORTED;
    return render(n, raw, QJS_INTL_FINITE, 0, NULL, out);
}
static QJSIntlStatus scale_exponent(QJSIntlDecimal *d, int64_t shift)
{
    int64_t result = (int64_t)d->exponent + shift;
    if (result < -QJS_INTL_DECIMAL_EXPONENT_LIMIT || result > QJS_INTL_DECIMAL_EXPONENT_LIMIT)
        return QJS_INTL_OVERFLOW;
    d->exponent = (int32_t)result;
    return QJS_INTL_OK;
}
static int64_t exponent_for_magnitude(const QJSIntlNativeNumber *n, int64_t magnitude)
{
    if (n->options.notation == QJS_INTL_NUMBER_SCIENTIFIC) return magnitude;
    if (n->options.notation == QJS_INTL_NUMBER_ENGINEERING)
        return magnitude >= 0 ? (magnitude / 3) * 3 : -(((-magnitude + 2) / 3) * 3);
    if (n->options.notation == QJS_INTL_NUMBER_COMPACT) {
        const QJSIntlNumberCompact *row = qjs_intl_number_compact_for_magnitude(
            n->data.compact, n->data.compact_count, magnitude);
        return row ? row->exponent : 0;
    }
    return 0;
}
QJSIntlStatus qjs_intl_native_number_format(const QJSIntlNativeNumber *n,
    const QJSIntlMathematicalValue *value, QJSIntlFormatted *out)
{
    QJSIntlDecimal exact = { 0 }, scaled;
    QJSIntlDecimalResult raw = { 0 };
    const QJSIntlNumberCompact *compact = NULL;
    QJSIntlStatus s;
    int64_t magnitude = 0, exponent = 0, rounded_magnitude;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!n || !value || (unsigned int)value->kind > QJS_INTL_NEGATIVE_ZERO)
        return QJS_INTL_INVALID_ARGUMENT;
    if (value->kind != QJS_INTL_FINITE && value->kind != QJS_INTL_NEGATIVE_ZERO)
        return render(n, NULL, value->kind, 0, NULL, out);
    s = qjs_intl_decimal_from_value(&n->allocator, value, &exact);
    if (s != QJS_INTL_OK) return s;
    if (n->options.style == QJS_INTL_NUMBER_PERCENT) {
        s = scale_exponent(&exact, 2); if (s != QJS_INTL_OK) goto done;
    }
    if (!zero(&exact)) {
        if (exact.length > INT32_MAX) { s = QJS_INTL_OVERFLOW; goto done; }
        magnitude = (int64_t)exact.length - 1 + exact.exponent;
        exponent = exponent_for_magnitude(n, magnitude);
    }
    if (exponent < INT32_MIN || exponent > INT32_MAX) { s = QJS_INTL_OVERFLOW; goto done; }
    scaled = exact;
    s = scale_exponent(&scaled, -exponent); if (s != QJS_INTL_OK) goto done;
    /* ComputeExponent rounds abs(x), even under ceil/floor. The final
     * FormatNumericToString call below restores the original signed mode. */
    if (n->options.notation != QJS_INTL_NUMBER_STANDARD) scaled.negative = 0;
    s = qjs_intl_decimal_format_numeric(&n->allocator, &scaled, &n->options.digits, &raw);
    if (s != QJS_INTL_OK) goto done;
    if (n->options.notation != QJS_INTL_NUMBER_STANDARD && !zero(&raw.rounded)) {
        rounded_magnitude = (int64_t)raw.rounded.length - 1 + raw.rounded.exponent;
        if (rounded_magnitude != magnitude - exponent) {
            magnitude++;
            exponent = exponent_for_magnitude(n, magnitude);
            if (exponent < INT32_MIN || exponent > INT32_MAX) { s = QJS_INTL_OVERFLOW; goto done; }
        }
    }
    if (n->options.notation != QJS_INTL_NUMBER_STANDARD) {
        qjs_intl_decimal_result_clear(&n->allocator, &raw);
        scaled = exact; s = scale_exponent(&scaled, -exponent); if (s != QJS_INTL_OK) goto done;
        s = qjs_intl_decimal_format_numeric(&n->allocator, &scaled, &n->options.digits, &raw);
        if (s != QJS_INTL_OK) goto done;
    }
    if (n->options.notation == QJS_INTL_NUMBER_COMPACT && exponent)
        compact = qjs_intl_number_compact_for_magnitude(n->data.compact, n->data.compact_count, magnitude);
    s = render(n, &raw, QJS_INTL_FINITE, (int32_t)exponent, compact, out);
done:
    qjs_intl_decimal_result_clear(&n->allocator, &raw);
    qjs_intl_decimal_clear(&n->allocator, &exact);
    return s;
}
QJSIntlStatus qjs_intl_native_number_format_range(const QJSIntlNativeNumber *n,
    const QJSIntlMathematicalValue *start, const QJSIntlMathematicalValue *end,
    QJSIntlFormatted *out)
{
    QJSIntlFormatted x = { 0 }, y = { 0 };
    QJSIntlNumberRangeSlices pieces;
    NumberBuilder b;
    QJSIntlStatus s;
    int equal;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!n || !start || !end || (unsigned int)start->kind > QJS_INTL_NEGATIVE_ZERO ||
        (unsigned int)end->kind > QJS_INTL_NEGATIVE_ZERO ||
        start->kind == QJS_INTL_NAN || end->kind == QJS_INTL_NAN)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!n->data.range.length) return QJS_INTL_UNSUPPORTED;
    memset(&b, 0, sizeof(b)); b.number = n;
    s = qjs_intl_native_number_format(n, start, &x); if (s != QJS_INTL_OK) goto done;
    s = qjs_intl_native_number_format(n, end, &y); if (s != QJS_INTL_OK) goto done;
    equal = qjs_intl_number_range_equal(&x, &y);
    if (!qjs_intl_number_range_slices(equal ? n->data.approximately : n->data.range,
                                     !equal, &pieces)) { s = QJS_INTL_DATA_ERROR; goto done; }
    b.source = QJS_INTL_SOURCE_SHARED;
    s = equal ? label_literal(&b, pieces.prefix, QJS_INTL_PART_APPROXIMATELY_SIGN) :
        bytes(&b, pieces.prefix, QJS_INTL_PART_LITERAL);
    if (s != QJS_INTL_OK) goto done;
    b.source = equal ? QJS_INTL_SOURCE_SHARED : pieces.first_endpoint ?
        QJS_INTL_SOURCE_END_RANGE : QJS_INTL_SOURCE_START_RANGE;
    s = append_result(&b, !equal && pieces.first_endpoint ? &y : &x); if (s != QJS_INTL_OK) goto done;
    if (!equal) {
        b.source = QJS_INTL_SOURCE_SHARED;
        s = bytes(&b, pieces.separator, QJS_INTL_PART_LITERAL); if (s != QJS_INTL_OK) goto done;
        b.source = pieces.first_endpoint ? QJS_INTL_SOURCE_START_RANGE : QJS_INTL_SOURCE_END_RANGE;
        s = append_result(&b, pieces.first_endpoint ? &x : &y); if (s != QJS_INTL_OK) goto done;
    }
    b.source = QJS_INTL_SOURCE_SHARED;
    s = equal ? label_literal(&b, pieces.suffix, QJS_INTL_PART_APPROXIMATELY_SIGN) :
        bytes(&b, pieces.suffix, QJS_INTL_PART_LITERAL);
done:
    qjs_intl_native_number_result_clear(n, &x); qjs_intl_native_number_result_clear(n, &y);
    if (s == QJS_INTL_OK) *out = b.value;
    else qjs_intl_native_number_result_clear(n, &b.value);
    return s;
}

/* PluralRules reads only c/e from notation metadata after raw rounding.
 * CLDR n/i/v/w/f/t still describe the unscaled unsigned raw string. */
QJSIntlStatus qjs_intl_native_number_notation_exponent(const QJSIntlNativeNumber *n,
    const QJSIntlDecimalResult *raw,int32_t *out)
{
    int64_t magnitude,exponent;
    if(!out)return QJS_INTL_INVALID_ARGUMENT;
    *out=0;
    if(!n || !raw_valid(raw))return QJS_INTL_INVALID_ARGUMENT;
    if(n->options.style!=QJS_INTL_NUMBER_DECIMAL)return QJS_INTL_UNSUPPORTED;
    if(zero(&raw->rounded))return QJS_INTL_OK;
    if(raw->rounded.length>INT32_MAX)return QJS_INTL_OVERFLOW;
    magnitude=(int64_t)raw->rounded.length-1+raw->rounded.exponent;
    exponent=exponent_for_magnitude(n,magnitude);
    if(exponent<INT32_MIN || exponent>INT32_MAX)return QJS_INTL_OVERFLOW;
    *out=(int32_t)exponent;return QJS_INTL_OK;
}
