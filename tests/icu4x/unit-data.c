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
#include "../../src/intl/icu4x/unit-data.h"
#include "../../src/intl/unit/data.h"
#include "../../src/intl/unit/grammar-data.h"
#include "../../src/intl/unit/identifier.h"

static QJSIntlStatus admit(const QJSIntlAllocator *allocator)
{
    QJSIntlData data;
    QJSIntlUnitData units;
    QJSIntlUnitGrammarData grammar;
    QJSIntlSection missing;
    QJSIntlStatus error;
    uint32_t i, group;

    error = qjs_intl_data_open(qjs_icu4x_unit_bytes, qjs_icu4x_unit_length, allocator, &data);
    if (!error)
        error = qjs_intl_unit_data_open(&data, allocator, &units);
    if (!error)
        error = qjs_intl_unit_grammar_data_open(&data, allocator, &grammar);
    if (error)
        return error;
    assert(qjs_intl_pack_section(&data.pack, QJS_INTL_SECTION_PLURALS, &missing) ==
           QJS_INTL_UNSUPPORTED);
    for (i = 0; i < qjs_intl_unit_definition_count(); i++) {
        const QJSIntlUnitDefinition *definition = qjs_intl_unit_definition_at(i);
        assert(!qjs_intl_unit_group_find(&units, QJS_INTL_UNIT_GROUP_SINGLE,
                                         bytes(definition->cldr_identifier), &group));
    }
    return QJS_INTL_OK;
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    size_t calls, i;

    assert(!strcmp(qjs_icu4x_unit_version, "49.0.0"));
    assert(!admit(&allocator));
    assert(!state.live);
    calls = state.calls;
    /* Validators own only temporary graph/index buffers; no partial view may
       retain any of them after an allocation failure. */
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        assert(admit(&allocator) == QJS_INTL_NO_MEMORY);
        assert(!state.live);
    }
    return 0;
}
