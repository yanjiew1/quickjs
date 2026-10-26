/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../icu4c/helpers.h"
#include "../../src/intl/number/unit-icu4x.h"
#include "../../src/intl/unit/identifier.h"

static void check(const QJSIntlICU4XNumberUnit *unit, const QJSIntlAllocator *allocator,
                  const char *decimal, uint32_t shift, int exact, const char *expected)
{
    uint16_t digit = '2';
    QJSIntlPart part = {0, 1, QJS_INTL_PART_INTEGER};
    QJSIntlFormatted body = {&digit, 1, &part, 1}, result;
    QJSIntlPluralOperands operands = {decimal, strlen(decimal), (int32_t)shift};
    size_t i;
    assert(!qjs_icu4x_number_unit_format(unit, &operands, shift, exact, &body, 1000, &result));
    assert(result.length > 1 && result.part_count > 1);
    if (expected) {
        assert(result.length == strlen(expected));
        for (i = 0; i < result.length; i++)
            assert(result.text[i] == (unsigned char)expected[i]);
    }
    qjs_intl_formatted_clear(allocator, &result);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSIntlICU4XUnitData data;
    QJSIntlICU4XNumberUnit *unit;
    QJSIntlFormatted result;
    QJSIntlPluralOperands number = {"2", 1, 0};
    uint16_t digit = '2';
    QJSIntlPart part = {0, 1, QJS_INTL_PART_INTEGER};
    QJSIntlFormatted body = {&digit, 1, &part, 1};
    size_t i, calls, live;
    unsigned width;

    assert(!qjs_icu4x_number_unit_data_open(&allocator, &data));
    assert(!state.live);
    assert(qjs_icu4x_number_unit_open(&allocator, &data, (QJSIntlBytes){NULL, 1},
                                      QJS_INTL_UNIT_LONG, bytes("meter"),
                                      &unit) == QJS_INTL_INVALID_ARGUMENT);
    assert(!unit && !state.live);
    for (i = 0; i < qjs_intl_unit_definition_count(); i++) {
        const QJSIntlUnitDefinition *definition = qjs_intl_unit_definition_at((uint32_t)i);
        for (width = 0; width < QJS_INTL_UNIT_WIDTH_COUNT; width++) {
            assert(!qjs_icu4x_number_unit_open(&allocator, &data, bytes("en"), width,
                                               bytes(definition->identifier), &unit));
            check(unit, &allocator, "2", 0, -1, NULL);
            qjs_icu4x_number_unit_close(unit);
        }
    }
    assert(!qjs_icu4x_number_unit_open(&allocator, &data, bytes("en"), QJS_INTL_UNIT_LONG,
                                       bytes("meter"), &unit));
    check(unit, &allocator, "2", 0, -1, "2 meters");
    check(unit, &allocator, "1", 0, 1, "2 meter");
    /* Visible fraction zeros are plural operands; an exact numeric selector is
       separate from the grammatical one category. The displayed body is fixed. */
    check(unit, &allocator, "1.0", 0, 1, "2 meters");
    live = state.live;
    calls = state.calls;
    check(unit, &allocator, "2", 0, -1, "2 meters");
    calls = state.calls - calls;
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        assert(qjs_icu4x_number_unit_format(unit, &number, 0, -1, &body, 1000, &result) ==
               QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && state.live == live);
    }
    state.fail_at = 0;
    assert(qjs_icu4x_number_unit_format(unit, &number, 0, -1, &body, 1, &result) ==
           QJS_INTL_OVERFLOW);
    assert(!result.text && !result.parts && state.live == live);
    qjs_icu4x_number_unit_close(unit);
    assert(!state.live);

    /* Constructor errors must close every already retained foreign payload. */
    calls = state.calls;
    assert(!qjs_icu4x_number_unit_open(&allocator, &data, bytes("de"), QJS_INTL_UNIT_LONG,
                                       bytes("kilometer-per-hour"), &unit));
    calls = state.calls - calls;
    check(unit, &allocator, "2", 0, -1, NULL);
    qjs_icu4x_number_unit_close(unit);
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        assert(qjs_icu4x_number_unit_open(&allocator, &data, bytes("de"), QJS_INTL_UNIT_LONG,
                                          bytes("kilometer-per-hour"),
                                          &unit) == QJS_INTL_NO_MEMORY);
        assert(!unit && !state.live);
    }
    state.fail_at = 0;
    assert(!qjs_icu4x_number_unit_open(&allocator, &data, bytes("ru"), QJS_INTL_UNIT_LONG,
                                       bytes("meter-per-second"), &unit));
    check(unit, &allocator, "1.2", 3, -1, NULL);
    qjs_icu4x_number_unit_close(unit);
    assert(!state.live);
    return 0;
}
