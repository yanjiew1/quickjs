/* Prepared number data through exact rounding, body and outer pattern steps.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/number/affix.h"
#include "../src/intl/number/body.h"
#include "../src/intl/number/symbol-data.h"
#include "../src/cutils/cutils.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Data {
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlNumberPatternData patterns;
    QJSIntlNumberSymbolData symbols;
    QJSIntlAllocator allocator;
    size_t live;
} Data;

static void *allocate(void *opaque, size_t size)
{
    Data *data = opaque;
    void *ptr;
    assert(size);
    ptr = malloc(size);
    if (ptr)
        data->live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    Data *data = opaque;
    assert(ptr && data->live);
    data->live--;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static uint32_t locale_id(const Data *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->data.locales.count; i++) {
        QJSIntlBytes tag;
        assert(!qjs_intl_data_locale(&data->data, i, &tag, &parent));
        if (tag.length == strlen(name) && !memcmp(tag.data, name, tag.length))
            return i;
    }
    abort();
}

static uint32_t next_scalar(QJSIntlBytes text, size_t *at)
{
    const uint8_t *p = (const uint8_t *)text.data + *at, *next;
    int c = unicode_from_utf8(p, (int)(text.length - *at), &next);
    assert(c >= 0);
    *at = (const char *)next - text.data;
    return c;
}

static QJSIntlUTF16 decode(QJSIntlBytes text)
{
    QJSIntlUTF16 result;
    uint16_t *out;
    size_t at = 0, length = 0;
    assert(text.length && text.length <= SIZE_MAX / 2);
    out = malloc(text.length * sizeof(*out));
    assert(out);
    while (at < text.length) {
        uint32_t c = next_scalar(text, &at);
        if (c > 0xffff) {
            c -= 0x10000;
            out[length++] = 0xd800 + (c >> 10);
            out[length++] = 0xdc00 + (c & 1023);
        } else
            out[length++] = c;
    }
    result = (QJSIntlUTF16){out, length};
    return result;
}

static void check(Data *data, const char *tag, const char *system, QJSIntlNumberPatternField field,
                  const QJSIntlNumberRounding *rounding, QJSIntlNumberSign sign_display,
                  const char *currency, const char *input, const char *expected)
{
    QJSIntlNumberingSystem definition;
    QJSIntlNumberingPreferences preferences;
    QJSIntlNumberPattern pattern;
    QJSIntlNumberSymbols symbols;
    QJSIntlNumberPatternToken *tokens;
    QJSIntlNumberAffixSymbols affixes;
    QJSIntlNumberBody layout = {0};
    QJSIntlRoundedNumber rounded;
    QJSIntlMathematicalValue value = {QJS_INTL_FINITE, bytes(input)};
    QJSIntlFormatted body, result;
    QJSIntlUTF16 wanted = decode(bytes(expected));
    uint32_t source = locale_id(data, tag), number_system, i;
    unsigned int form;
    size_t at = 0, part_end = 0;
    if (system)
        assert(!qjs_intl_numbering_system_find(&data->numbering, bytes(system), &number_system));
    else {
        assert(!qjs_intl_numbering_preferences(&data->numbering, bytes(tag), &preferences));
        number_system = preferences.standard;
    }
    assert(!qjs_intl_numbering_system_at(&data->numbering, number_system, &definition));
    assert(definition.kind == QJS_INTL_NUMBERING_NUMERIC);
    for (i = 0; i < 10; i++)
        layout.digits[i] = next_scalar(definition.value, &at);
    assert(at == definition.value.length);
    assert(!qjs_intl_number_pattern_at(&data->patterns, source, number_system, field, &pattern));
    assert(pattern.state == QJS_INTL_NUMBER_PATTERN_VALUE);
    assert(!qjs_intl_number_symbols_at(&data->symbols, source, number_system, &symbols));
    layout.primary_grouping = pattern.primary_grouping;
    layout.secondary_grouping = pattern.secondary_grouping;
    layout.minimum_grouping_digits = pattern.minimum_grouping_digits;
    layout.grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    layout.maximum_output_length = 1000;
    layout.decimal = decode(symbols.fields[QJS_INTL_SYMBOL_DECIMAL].text);
    layout.group = decode(symbols.fields[QJS_INTL_SYMBOL_GROUP].text);
    layout.nan = decode(symbols.fields[QJS_INTL_SYMBOL_NAN].text);
    layout.infinity = decode(symbols.fields[QJS_INTL_SYMBOL_INFINITY].text);
    affixes = (QJSIntlNumberAffixSymbols){
        symbols.fields[QJS_INTL_SYMBOL_MINUS].text,
        symbols.fields[QJS_INTL_SYMBOL_PLUS].text,
        symbols.fields[QJS_INTL_SYMBOL_PERCENT].text,
        symbols.fields[QJS_INTL_SYMBOL_PER_MILLE].text,
        bytes(currency),
    };
    assert(!qjs_intl_number_round(&data->allocator, rounding, &value, &rounded));
    assert(!rounded.exponent);
    assert(!qjs_intl_number_sign_pattern(sign_display, &rounded, &form));
    assert(!qjs_intl_number_body_format(&data->allocator, &layout, rounded.kind,
                                        (QJSIntlBytes){rounded.number.text, rounded.number.length},
                                        &body));
    tokens = malloc(pattern.forms[form].count * sizeof(*tokens));
    assert(tokens);
    for (i = 0; i < pattern.forms[form].count; i++)
        assert(!qjs_intl_number_pattern_token_at(&data->patterns, &pattern.forms[form], i,
                                                 tokens + i));
    assert(!qjs_intl_number_affix_format(&data->allocator, tokens, pattern.forms[form].count,
                                         &affixes, &body, form == QJS_INTL_NUMBER_PATTERN_PLUS,
                                         1000, &result));
    assert(result.length == wanted.length);
    assert(!memcmp(result.text, wanted.data, wanted.length * sizeof(*wanted.data)));
    for (i = 0; i < result.part_count; i++) {
        assert(result.parts[i].start == part_end);
        part_end = result.parts[i].end;
        assert(part_end <= result.length);
    }
    assert(part_end == result.length);
    qjs_intl_number_body_clear(&data->allocator, &result);
    qjs_intl_number_body_clear(&data->allocator, &body);
    qjs_intl_number_round_clear(&data->allocator, &rounded);
    free(tokens);
    free((void *)layout.decimal.data);
    free((void *)layout.group.data);
    free((void *)layout.nan.data);
    free((void *)layout.infinity.data);
    free((void *)wanted.data);
    assert(!data->live);
}

int main(void)
{
    Data data = {0};
    QJSIntlNumberRounding rounding = {0};
    data.allocator = (QJSIntlAllocator){&data, allocate, NULL, release};
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &data.allocator,
                               &data.data));
    assert(!qjs_intl_numbering_data_open(&data.data, &data.allocator, &data.numbering));
    assert(!qjs_intl_number_pattern_data_open(&data.numbering, &data.allocator, &data.patterns));
    assert(!qjs_intl_number_symbol_data_open(&data.numbering, &data.allocator, &data.symbols));
    rounding.digits = (QJSIntlDecimalOptions){1,
                                              0,
                                              2,
                                              1,
                                              21,
                                              1,
                                              QJS_INTL_ROUND_HALF_EXPAND,
                                              QJS_INTL_ROUND_FRACTION,
                                              QJS_INTL_TRAILING_ZERO_AUTO,
                                              1000};
    check(&data, "en", NULL, QJS_INTL_PATTERN_DECIMAL, &rounding, QJS_INTL_NUMBER_SIGN_AUTO, "",
          "12345.678", "12,345.68");
    check(&data, "en", NULL, QJS_INTL_PATTERN_DECIMAL, &rounding, QJS_INTL_NUMBER_SIGN_NEGATIVE, "",
          "-0.001", "0");
    check(&data, "en", NULL, QJS_INTL_PATTERN_DECIMAL, &rounding, QJS_INTL_NUMBER_SIGN_AUTO, "",
          "-0.001", "-0");
    check(&data, "en", "arab", QJS_INTL_PATTERN_DECIMAL, &rounding, QJS_INTL_NUMBER_SIGN_AUTO, "",
          "12345.678", "١٢٬٣٤٥٫٦٨");
    rounding.digits.minimum_fraction_digits = 2;
    check(&data, "en", NULL, QJS_INTL_PATTERN_CURRENCY_ACCOUNTING, &rounding,
          QJS_INTL_NUMBER_SIGN_AUTO, "$", "-1234.5", "($1,234.50)");
    check(&data, "en", NULL, QJS_INTL_PATTERN_CURRENCY_ACCOUNTING, &rounding,
          QJS_INTL_NUMBER_SIGN_ALWAYS, "$", "1234.5", "+$1,234.50");
    rounding.digits.minimum_fraction_digits = 0;
    if (TEST_INTL_PROFILE) {
        check(&data, "fr", NULL, QJS_INTL_PATTERN_DECIMAL, &rounding, QJS_INTL_NUMBER_SIGN_AUTO, "",
              "12345.678", "12 345,68");
        check(&data, "hi", NULL, QJS_INTL_PATTERN_DECIMAL, &rounding, QJS_INTL_NUMBER_SIGN_AUTO, "",
              "1234567.89", "12,34,567.89");
    }
    rounding.percent = 1;
    rounding.digits.maximum_fraction_digits = 0;
    check(&data, "en", NULL, QJS_INTL_PATTERN_PERCENT, &rounding, QJS_INTL_NUMBER_SIGN_AUTO, "",
          "12.345", "1,235%");
    assert(!data.live);
    return 0;
}
