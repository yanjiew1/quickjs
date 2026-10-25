/* Currency selection fallback, provenance and borrowed-storage contracts.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "../src/intl/currency/display.h"
#include "../src/intl/number/currency-pattern.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t qjs_intl_currency_display_bytes[];
extern const size_t qjs_intl_currency_display_length;

static void *allocate(void *opaque, size_t size)
{
    size_t *calls = opaque;
    assert(size);
    ++*calls;
    return malloc(size);
}

static void release(void *opaque, void *ptr)
{
    (void)opaque;
    assert(ptr);
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes value, const char *text)
{
    return value.length == strlen(text) &&
           (!value.length || !memcmp(value.data, text, value.length));
}

static uint32_t locale(const QJSIntlData *data, const char *text)
{
    uint32_t i;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes name;
        uint32_t parent;
        assert(!qjs_intl_data_locale(data, i, &name, &parent));
        if (equal(name, text))
            return i;
    }
    abort();
}

static void symbol(const QJSIntlCurrencyData *data, const char *tag, const char *code,
                    unsigned int narrow, const char *expected)
{
    QJSIntlBytes result;
    assert(!qjs_intl_currency_symbol(data, locale(&data->data, tag), bytes(code), narrow, &result));
    assert(equal(result, expected));
}

static void name(const QJSIntlCurrencyData *data, const char *tag, const char *code,
                  QJSIntlPluralCategory category, int exact, const char *expected)
{
    QJSIntlBytes result;
    assert(!qjs_intl_currency_name(data, locale(&data->data, tag), bytes(code), category, exact,
                                   &result));
    assert(equal(result, expected));
}

int main(void)
{
    size_t calls = 0, opened;
    QJSIntlAllocator allocator = {&calls, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlCurrencyData currency;
    QJSIntlCurrencyUnitLayouts layouts;
    QJSIntlBytes out, unknown = bytes("ZZZ");
    uint32_t en;
    unsigned int i;
    assert(!qjs_intl_data_open(qjs_intl_currency_display_bytes, qjs_intl_currency_display_length,
                               &allocator, &data));
    assert(!qjs_intl_currency_data_open(&data, &allocator, &currency));
    en = locale(&data, "en");
    opened = calls;
    symbol(&currency, "en", "USD", 0, "");
    symbol(&currency, "en", "USD", 1, "$");
    symbol(&currency, "en", "EUR", 1, "€");
    symbol(&currency, "en", "GBP", 1, "GBP");
    symbol(&currency, "en", "JPY", 1, "¥");
    symbol(&currency, "en", "CHF", 0, "CHF");
    symbol(&currency, "de", "USD", 1, "$");
    symbol(&currency, "en", "ZZZ", 1, "ZZZ");
    name(&currency, "en", "EUR", QJS_INTL_PLURAL_ONE, 1, "exact euro");
    name(&currency, "en-GB", "EUR", QJS_INTL_PLURAL_ONE, 1, "euro");
    name(&currency, "en", "EUR", QJS_INTL_PLURAL_ONE, -1, "euro");
    name(&currency, "en", "EUR", QJS_INTL_PLURAL_MANY, -1, "euros");
    name(&currency, "en", "EUR", QJS_INTL_PLURAL_ZERO, 0, "");
    name(&currency, "de", "EUR", QJS_INTL_PLURAL_ZERO, 0, "root euro");
    name(&currency, "de", "EUR", QJS_INTL_PLURAL_ONE, 1, "root euro");
    name(&currency, "en", "GBP", QJS_INTL_PLURAL_ONE, 1, "");
    name(&currency, "en", "USD", QJS_INTL_PLURAL_OTHER, -1, "root dollar");
    name(&currency, "en-GB", "USD", QJS_INTL_PLURAL_ONE, -1, "dollar");
    name(&currency, "en-GB", "USD", QJS_INTL_PLURAL_OTHER, -1, "child dollars");
    name(&currency, "en", "JPY", QJS_INTL_PLURAL_OTHER, -1, "JPY");
    name(&currency, "en", "CHF", QJS_INTL_PLURAL_ONE, 1, "CHF");
    name(&currency, "en", "ZZZ", QJS_INTL_PLURAL_OTHER, -1, "ZZZ");
    assert(!qjs_intl_currency_symbol(&currency, en, unknown, 0, &out));
    assert(out.data == unknown.data); /* Code fallback borrows the caller. */
    memset(&layouts, 0, sizeof(layouts));
    for (i = 0; i < QJS_INTL_CURRENCY_UNIT_LAYOUT_COUNT; i++)
        layouts.fields[i].state = QJS_INTL_LAYOUT_ABSENT;
    layouts.fields[QJS_INTL_CURRENCY_UNIT_ONE] =
        (QJSIntlNumberLayoutText){bytes("{0} one {1}"), QJS_INTL_LAYOUT_VALUE, 0};
    layouts.fields[QJS_INTL_CURRENCY_UNIT_OTHER] =
        (QJSIntlNumberLayoutText){bytes("{0} root {1}"), QJS_INTL_LAYOUT_VALUE, 1};
    /* Exact-one names do not force a template without an exact-one selector. */
    assert(!qjs_intl_currency_unit_pattern(&layouts, QJS_INTL_PLURAL_ONE, 1, &out));
    assert(equal(out, "{0} one {1}"));
    name(&currency, "en", "EUR", QJS_INTL_PLURAL_ONE, 1, "exact euro");
    layouts.fields[QJS_INTL_CURRENCY_UNIT_EXACT_ONE] =
        (QJSIntlNumberLayoutText){bytes("exact {0} {1}"), QJS_INTL_LAYOUT_VALUE, 0};
    assert(!qjs_intl_currency_unit_pattern(&layouts, QJS_INTL_PLURAL_ONE, 1, &out));
    assert(equal(out, "exact {0} {1}"));
    name(&currency, "en", "USD", QJS_INTL_PLURAL_ONE, 1, "dollar");
    layouts.fields[QJS_INTL_CURRENCY_UNIT_EXACT_ONE].from_root = 1;
    assert(!qjs_intl_currency_unit_pattern(&layouts, QJS_INTL_PLURAL_ONE, 1, &out));
    assert(equal(out, "{0} one {1}"));
    layouts.fields[QJS_INTL_CURRENCY_UNIT_ONE].from_root = 1;
    assert(!qjs_intl_currency_unit_pattern(&layouts, QJS_INTL_PLURAL_ONE, 1, &out));
    assert(equal(out, "{0} root {1}"));
    layouts.fields[QJS_INTL_CURRENCY_UNIT_ONE] =
        (QJSIntlNumberLayoutText){{NULL, 0}, QJS_INTL_LAYOUT_VALUE, 0};
    assert(!qjs_intl_currency_unit_pattern(&layouts, QJS_INTL_PLURAL_ONE, -1, &out));
    assert(!out.length);
    layouts.fields[QJS_INTL_CURRENCY_UNIT_ONE].state = QJS_INTL_LAYOUT_BLOCKED;
    layouts.fields[QJS_INTL_CURRENCY_UNIT_OTHER].state = QJS_INTL_LAYOUT_ABSENT;
    assert(qjs_intl_currency_unit_pattern(&layouts, QJS_INTL_PLURAL_ONE, -1, &out) ==
           QJS_INTL_UNSUPPORTED);
    assert(!out.data && !out.length);
    assert(qjs_intl_currency_unit_pattern(NULL, QJS_INTL_PLURAL_ONE, -1, &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_unit_pattern(&layouts, QJS_INTL_PLURAL_ONE, -1, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_unit_pattern(&layouts, QJS_INTL_PLURAL_CATEGORY_COUNT, -1, &out) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_unit_pattern(&layouts, QJS_INTL_PLURAL_ONE, 2, &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_unit_pattern(&layouts, QJS_INTL_PLURAL_ONE, -2, &out) == QJS_INTL_INVALID_ARGUMENT);
    for (i = 0; i < 3; i++) {
        const char *bad[] = {"US", "usd", "US1"};
        assert(qjs_intl_currency_symbol(&currency, en, bytes(bad[i]), 0, &out) ==
               QJS_INTL_INVALID_ARGUMENT);
        assert(!out.data && !out.length);
        assert(qjs_intl_currency_name(&currency, en, bytes(bad[i]), QJS_INTL_PLURAL_ONE, 1,
                                      &out) == QJS_INTL_INVALID_ARGUMENT);
        assert(!out.data && !out.length);
    }
    assert(qjs_intl_currency_symbol(&currency, en, (QJSIntlBytes){NULL, 3}, 0, &out) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_symbol(&currency, en, bytes("USD"), 2, &out) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_symbol(&currency, data.locales.count, bytes("USD"), 0, &out) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_symbol(NULL, en, bytes("USD"), 0, &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_symbol(&currency, en, bytes("USD"), 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_name(&currency, en, bytes("USD"), QJS_INTL_PLURAL_CATEGORY_COUNT, -1,
                                  &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_name(&currency, en, bytes("USD"), (QJSIntlPluralCategory)-1, -1,
                                  &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_name(&currency, en, bytes("USD"), QJS_INTL_PLURAL_ONE, 2, &out) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_name(&currency, en, bytes("USD"), QJS_INTL_PLURAL_ONE, -2, &out) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!out.data && !out.length);
    assert(qjs_intl_currency_name(NULL, en, bytes("USD"), QJS_INTL_PLURAL_ONE, 1, &out) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_name(&currency, en, bytes("USD"), QJS_INTL_PLURAL_ONE, 1, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(calls == opened);
    return 0;
}
