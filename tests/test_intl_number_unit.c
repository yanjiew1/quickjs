/* Authored compound adapter: derived features, source rules and error paths.
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
#include "../src/intl/number/unit.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t qjs_intl_unit_composition_bytes[];
extern const size_t qjs_intl_unit_composition_length;
extern const uint8_t qjs_intl_unit_composition_projected_bytes[];
extern const size_t qjs_intl_unit_composition_projected_length;

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        state->live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static uint32_t locale_id(const QJSIntlData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes tag;
        assert(!qjs_intl_data_locale(data, i, &tag, &parent));
        if (tag.length == strlen(name) && !memcmp(tag.data, name, tag.length))
            return i;
    }
    abort();
}

static void formatted(const QJSIntlNumberUnit *unit, const QJSIntlAllocator *allocator,
                      unsigned int value, int exact, size_t limit, const char *expected,
                      QJSIntlStatus status)
{
    uint16_t text = '0' + value;
    char decimal = text;
    QJSIntlPart part = {0, 1, QJS_INTL_PART_INTEGER};
    QJSIntlFormatted body = {&text, 1, &part, 1}, result, zero = {0};
    QJSIntlPluralOperands number = {&decimal, 1, 0};
    size_t i, at = 0;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_number_unit_format(unit, allocator, &number, 0, exact, &body, limit, &result) == status);
    if (status) {
        assert(!memcmp(&result, &zero, sizeof(result)));
        return;
    }
    assert(result.length == strlen(expected));
    for (i = 0; i < result.length; i++)
        assert(result.text[i] == (unsigned char)expected[i]);
    for (i = 0; i < result.part_count; i++) {
        assert(result.parts[i].start == at && result.parts[i].end >= at);
        at = result.parts[i].end;
    }
    assert(at == result.length);
    qjs_intl_unit_format_clear(allocator, &result);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlUnitData units;
    QJSIntlPluralData plurals;
    QJSIntlUnitGrammarData grammar;
    QJSIntlNumberUnit unit, zero = {0};
    uint32_t en, british, de, root;
    size_t calls, i;
    assert(!qjs_intl_data_open(qjs_intl_unit_composition_bytes, qjs_intl_unit_composition_length,
                                &allocator, &data));
    assert(!qjs_intl_unit_data_open(&data, &allocator, &units));
    assert(!qjs_intl_plural_data_open(&data, &allocator, &plurals));
    assert(!qjs_intl_unit_grammar_data_open(&data, &allocator, &grammar));
    assert(!state.live);
    en = locale_id(&data, "en");
    british = locale_id(&data, "en-GB");
    de = locale_id(&data, "de");
    root = locale_id(&data, "root");
    calls = state.calls;
    assert(!qjs_intl_number_unit_open(&units, &plurals, &grammar, british, root, QJS_INTL_UNIT_LONG,
                                       bytes("liter-per-fluid-ounce"), &unit));
    assert(state.calls == calls);
    formatted(&unit, &allocator, 2, -1, 100, "ACC_TWO CHILD_DIV 2 EN_ONE", QJS_INTL_OK);
    calls = state.calls;
    formatted(&unit, &allocator, 2, -1, 100, "ACC_TWO CHILD_DIV 2 EN_ONE", QJS_INTL_OK);
    assert(state.calls == calls + 2); /* No intermediate component strings. */
    for (i = 1; i <= 2; i++) {
        state.fail_at = state.calls + i;
        formatted(&unit, &allocator, 2, -1, 100, NULL, QJS_INTL_NO_MEMORY);
        assert(!state.live);
    }
    state.fail_at = 0;
    formatted(&unit, &allocator, 2, -1, 5, NULL, QJS_INTL_OVERFLOW);
    assert(!qjs_intl_number_unit_open(&units, &plurals, &grammar, en, root, QJS_INTL_UNIT_LONG,
                                       bytes("liter-per-fluid-ounce"), &unit));
    formatted(&unit, &allocator, 1, 1, 100, "ACC_ONE CHILD_DIV 1 EN_ONE", QJS_INTL_OK);
    assert(!qjs_intl_number_unit_open(&units, &plurals, &grammar, de, root, QJS_INTL_UNIT_LONG,
                                       bytes("liter-per-fluid-ounce"), &unit));
    formatted(&unit, &allocator, 2, -1, 100, "2 DE_OTHER DE_DIV DE_ACC", QJS_INTL_OK);
    assert(!qjs_intl_number_unit_open(&units, &plurals, &grammar, british, root, QJS_INTL_UNIT_LONG,
                                       bytes("liter-per-hour"), &unit));
    formatted(&unit, &allocator, 2, -1, 100, "EN_PER_H 2 EN_ONE", QJS_INTL_OK);
    assert(!qjs_intl_number_unit_open(&units, &plurals, &grammar, en, root, QJS_INTL_UNIT_LONG,
                                       bytes("meter-per-second"), &unit));
    formatted(&unit, &allocator, 2, -1, 100, "2 EN_FULL", QJS_INTL_OK);
    assert(!qjs_intl_number_unit_open(&units, &plurals, &grammar, british, root, QJS_INTL_UNIT_SHORT,
                                       bytes("liter-per-fluid-ounce"), &unit));
    formatted(&unit, &allocator, 2, -1, 100, NULL, QJS_INTL_DATA_ERROR);
    assert(!qjs_intl_number_unit_open(&units, &plurals, &grammar, british, root, QJS_INTL_UNIT_NARROW,
                                       bytes("liter-per-fluid-ounce"), &unit));
    formatted(&unit, &allocator, 2, -1, 100, "2 ROOT_L ROOT_DIV ROOT_F", QJS_INTL_OK);
    assert(qjs_intl_number_unit_open(&units, &plurals, &grammar, british, en, QJS_INTL_UNIT_LONG,
                                      bytes("liter-per-hour"), &unit) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&unit, &zero, sizeof(unit)));
    assert(qjs_intl_number_unit_open(&units, &plurals, &grammar, data.locales.count, root,
                                      QJS_INTL_UNIT_LONG, bytes("liter-per-hour"), &unit) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_unit_open(&units, &plurals, &grammar, en, root, QJS_INTL_UNIT_WIDTH_COUNT,
                                      bytes("liter-per-hour"), &unit) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_unit_open(&units, &plurals, NULL, en, root, QJS_INTL_UNIT_LONG,
                                      bytes("liter-per-hour"), &unit) == QJS_INTL_INVALID_ARGUMENT);
    {
        QJSIntlData missing_data;
        QJSIntlUnitData missing_units;
        QJSIntlPluralData missing_plurals;
        QJSIntlUnitGrammarData missing_grammar;
        assert(!qjs_intl_data_open(qjs_intl_unit_composition_projected_bytes,
                                    qjs_intl_unit_composition_projected_length, &allocator, &missing_data));
        assert(!qjs_intl_unit_data_open(&missing_data, &allocator, &missing_units));
        assert(!qjs_intl_plural_data_open(&missing_data, &allocator, &missing_plurals));
        assert(!qjs_intl_unit_grammar_data_open(&missing_data, &allocator, &missing_grammar));
        assert(!qjs_intl_number_unit_open(&missing_units, &missing_plurals, &missing_grammar,
                                           locale_id(&missing_data, "en-GB"), locale_id(&missing_data, "root"),
                                           QJS_INTL_UNIT_LONG, bytes("liter-per-fluid-ounce"), &unit));
        /* The en-origin noun needs cardinal rules that this projected pack
           deliberately omits. A usable root expression must not hide that. */
        calls = state.calls;
        formatted(&unit, &allocator, 2, -1, 100, NULL, QJS_INTL_UNSUPPORTED);
        assert(state.calls == calls);
    }
    assert(!qjs_intl_number_unit_open(&units, &plurals, &grammar, british, root, QJS_INTL_UNIT_LONG,
                                       bytes("liter-per-fluid-ounce"), &unit));
    memset(&data, 0, sizeof(data));
    memset(&units, 0, sizeof(units));
    memset(&plurals, 0, sizeof(plurals));
    memset(&grammar, 0, sizeof(grammar));
    formatted(&unit, &allocator, 2, -1, 100, "ACC_TWO CHILD_DIV 2 EN_ONE", QJS_INTL_OK);
    formatted(&unit, &allocator, 2, 2, 100, NULL, QJS_INTL_INVALID_ARGUMENT);
    assert(!state.live);
    return 0;
}
