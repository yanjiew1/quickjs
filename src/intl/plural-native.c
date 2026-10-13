/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "plural-native.h"
#include <string.h>

struct QJSIntlNativePlural {
    QJSIntlAllocator allocator;
    QJSIntlPluralOptions options;
    QJSIntlPluralRule rules[QJS_INTL_PLURAL_CATEGORY_COUNT];
    size_t rule_count;
    uint8_t ranges[QJS_INTL_PLURAL_CATEGORY_COUNT * QJS_INTL_PLURAL_CATEGORY_COUNT];
};
const char *qjs_intl_plural_category_name(QJSIntlPluralCategory c)
{
    static const char *const names[] = {"zero", "one", "two", "few", "many", "other"};
    return (unsigned int)c < QJS_INTL_PLURAL_CATEGORY_COUNT ? names[c] : NULL;
}
void qjs_intl_native_plural_close(QJSIntlNativePlural *h)
{
    QJSIntlAllocator a;
    size_t i;
    if (!h) return;
    a = h->allocator;
    for (i = 0; i < h->rule_count; i++)
        if (h->rules[i].relation.data) a.free(a.opaque, (void *)h->rules[i].relation.data);
    a.free(a.opaque, h);
}
QJSIntlStatus qjs_intl_native_plural_open(const QJSIntlAllocator *a,
    const QJSIntlPluralOptions *options, const QJSIntlPluralRulesData *data,
    QJSIntlNativePlural **out)
{
    QJSIntlNativePlural *h;
    JSIntlPluralOperands operand = {"0", 1, 0};
    size_t i;
    int previous = -1, matches;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !a->malloc || !a->free || !options || !data || !data->rules ||
        !data->rule_count || data->rule_count > QJS_INTL_PLURAL_CATEGORY_COUNT ||
        (unsigned int)options->type > QJS_INTL_PLURAL_ORDINAL ||
        qjs_intl_decimal_options_validate(&options->digits) != QJS_INTL_OK ||
        (data->range_count && !data->ranges) || data->range_count > 36)
        return QJS_INTL_INVALID_ARGUMENT;
    h = a->malloc(a->opaque, sizeof(*h));
    if (!h) return QJS_INTL_NO_MEMORY;
    memset(h, 0, sizeof(*h));
    h->allocator = *a; h->options = *options;
    memset(h->ranges, QJS_INTL_PLURAL_OTHER, sizeof(h->ranges));
    for (i = 0; i < data->rule_count; i++) {
        const QJSIntlPluralRule *r = &data->rules[i];
        char *copy;
        if (r->relation.length == SIZE_MAX) { qjs_intl_native_plural_close(h); return QJS_INTL_OVERFLOW; }
        if ((unsigned int)r->category >= QJS_INTL_PLURAL_CATEGORY_COUNT ||
            (int)r->category <= previous || !r->relation.data ||
            (r->relation.length &&
             (r->relation.data[0] < '!' ||
              r->relation.data[r->relation.length - 1] < '!' ||
              memchr(r->relation.data, '@', r->relation.length))) ||
            js_intl_plural_rule_evaluate(r->relation.data, r->relation.length,
                                         &operand, &matches) < 0 ||
            (r->category != QJS_INTL_PLURAL_OTHER && !r->relation.length) ||
            (r->category == QJS_INTL_PLURAL_OTHER && r->relation.length))
            goto invalid;
        copy = a->malloc(a->opaque, r->relation.length + 1);
        if (!copy) { qjs_intl_native_plural_close(h); return QJS_INTL_NO_MEMORY; }
        memcpy(copy, r->relation.data, r->relation.length); copy[r->relation.length] = '\0';
        h->rules[i].category = r->category;
        h->rules[i].relation.data = copy;
        h->rules[i].relation.length = r->relation.length;
        h->rule_count++;
        previous = r->category;
    }
    if (previous != QJS_INTL_PLURAL_OTHER) goto invalid;
    previous = -1;
    for (i = 0; i < data->range_count; i++) {
        const QJSIntlPluralRange *r = &data->ranges[i];
        unsigned int key;
        if ((unsigned int)r->start >= QJS_INTL_PLURAL_CATEGORY_COUNT ||
            (unsigned int)r->end >= QJS_INTL_PLURAL_CATEGORY_COUNT ||
            (unsigned int)r->result >= QJS_INTL_PLURAL_CATEGORY_COUNT) goto invalid;
        key = (unsigned int)r->start * QJS_INTL_PLURAL_CATEGORY_COUNT + r->end;
        if ((int)key <= previous) goto invalid;
        h->ranges[key] = (uint8_t)r->result; previous = (int)key;
    }
    *out = h;
    return QJS_INTL_OK;
 invalid:
    qjs_intl_native_plural_close(h);
    return QJS_INTL_DATA_ERROR;
}
QJSIntlStatus qjs_intl_native_plural_resolved_options(const QJSIntlNativePlural *h,
                                                    QJSIntlPluralOptions *out)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!h || !out) return QJS_INTL_INVALID_ARGUMENT;
    *out = h->options; return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_plural_categories(const QJSIntlNativePlural *h,
    QJSIntlPluralCategory out[QJS_INTL_PLURAL_CATEGORY_COUNT], size_t *count)
{
    size_t i;
    if (count) *count = 0;
    if (out) memset(out, 0, sizeof(*out) * QJS_INTL_PLURAL_CATEGORY_COUNT);
    if (!h || !out || !count) return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < h->rule_count; i++) out[i] = h->rules[i].category;
    *count = h->rule_count; return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_plural_select_operands(const QJSIntlNativePlural *h,
    const JSIntlPluralOperands *operand, QJSIntlPluralCategory *out)
{
    size_t i;
    int matches;
    QJSIntlPluralCategory selected = QJS_INTL_PLURAL_OTHER;
    if (out) *out = QJS_INTL_PLURAL_OTHER;
    if (!h || !operand || !out) return QJS_INTL_INVALID_ARGUMENT;
    /* Do not early return: every call validates the complete decimal input. */
    for (i = 0; i < h->rule_count; i++) {
        if (js_intl_plural_rule_evaluate(h->rules[i].relation.data,
            h->rules[i].relation.length, operand, &matches) < 0) return QJS_INTL_INVALID_ARGUMENT;
        if (matches && selected == QJS_INTL_PLURAL_OTHER &&
            h->rules[i].category != QJS_INTL_PLURAL_OTHER) selected = h->rules[i].category;
    }
    *out = selected; return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_plural_range_categories(const QJSIntlNativePlural *h,
    QJSIntlPluralCategory start, QJSIntlPluralCategory end, QJSIntlPluralCategory *out)
{
    if (out) *out = QJS_INTL_PLURAL_OTHER;
    if (!h || !out || (unsigned int)start >= QJS_INTL_PLURAL_CATEGORY_COUNT ||
        (unsigned int)end >= QJS_INTL_PLURAL_CATEGORY_COUNT) return QJS_INTL_INVALID_ARGUMENT;
    *out = (QJSIntlPluralCategory)h->ranges[(unsigned int)start * QJS_INTL_PLURAL_CATEGORY_COUNT + end];
    return QJS_INTL_OK;
}
static QJSIntlStatus resolve(const QJSIntlNativePlural *h,
    const QJSIntlMathematicalValue *value, QJSIntlPluralCategory *category,
    QJSIntlDecimalResult *formatted)
{
    QJSIntlDecimal number = {0};
    JSIntlPluralOperands operand;
    QJSIntlStatus s;
    *category = QJS_INTL_PLURAL_OTHER;
    memset(formatted, 0, sizeof(*formatted));
    if (!value || (unsigned int)value->kind > QJS_INTL_NEGATIVE_ZERO)
        return QJS_INTL_INVALID_ARGUMENT;
    if (value->kind == QJS_INTL_NAN || value->kind == QJS_INTL_POSITIVE_INFINITY ||
        value->kind == QJS_INTL_NEGATIVE_INFINITY) return QJS_INTL_OK;
    s = qjs_intl_decimal_from_value(&h->allocator, value, &number);
    if (s == QJS_INTL_OK) s = qjs_intl_decimal_format_numeric(&h->allocator, &number,
                                                           &h->options.digits, formatted);
    qjs_intl_decimal_clear(&h->allocator, &number);
    if (s != QJS_INTL_OK) return s;
    operand.decimal = formatted->text; operand.length = formatted->length; operand.exponent = 0;
    return qjs_intl_native_plural_select_operands(h, &operand, category);
}
QJSIntlStatus qjs_intl_native_plural_select(const QJSIntlNativePlural *h,
    const QJSIntlMathematicalValue *value, QJSIntlPluralCategory *out)
{
    QJSIntlDecimalResult result = {0};
    QJSIntlStatus s;
    if (out) *out = QJS_INTL_PLURAL_OTHER;
    if (!h || !out) return QJS_INTL_INVALID_ARGUMENT;
    s = resolve(h, value, out, &result);
    qjs_intl_decimal_result_clear(&h->allocator, &result);
    return s;
}
QJSIntlStatus qjs_intl_native_plural_select_range(const QJSIntlNativePlural *h,
    const QJSIntlMathematicalValue *start, const QJSIntlMathematicalValue *end,
    QJSIntlPluralCategory *out)
{
    QJSIntlDecimalResult first = {0}, last = {0};
    QJSIntlPluralCategory x, y;
    QJSIntlStatus s;
    int equal;
    if (out) *out = QJS_INTL_PLURAL_OTHER;
    if (!h || !out || !start || !end || start->kind == QJS_INTL_NAN || end->kind == QJS_INTL_NAN)
        return QJS_INTL_INVALID_ARGUMENT;
    s = resolve(h, start, &x, &first);
    if (s != QJS_INTL_OK) goto done;
    s = resolve(h, end, &y, &last);
    if (s != QJS_INTL_OK) goto done;
    /* ILD infinity strings are distinguishable by sign. Finite text is
     * unsigned per FormatNumericToString, so +/- equal magnitudes coincide. */
    if (!first.text || !last.text) equal = !first.text && !last.text && start->kind == end->kind;
    else equal = first.length == last.length && !memcmp(first.text, last.text, first.length);
    if (equal) { *out = x; s = QJS_INTL_OK; }
    else s = qjs_intl_native_plural_range_categories(h, x, y, out);
 done:
    qjs_intl_decimal_result_clear(&h->allocator, &first);
    qjs_intl_decimal_result_clear(&h->allocator, &last);
    return s;
}
