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
#include "../../src/intl/locale/case.h"

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSIntlCaseResult result;
    static const uint16_t input[] = {'I', 0, 0xd800, 0x130};
    static const uint16_t expected[] = {0x131, 0, 0xd800, 'i'};
    static const uint16_t expansion[] = {0x130};
    size_t i, calls;

    assert(!qjs_intl_case_map(&allocator, bytes("tr-u-nu-arab"), (QJSIntlUTF16){input, 4}, 1, 1024,
                              &result));
    assert(result.length == 4 && !memcmp(result.text, expected, sizeof(expected)));
    calls = state.calls;
    qjs_intl_case_result_clear(&allocator, &result);
    assert(!state.live);
    // C owns result storage; fail each allocation without changing ICU globals.
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        assert(qjs_intl_case_map(&allocator, bytes("tr"), (QJSIntlUTF16){input, 4}, 1, 1024,
                                 &result) == QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.length && !state.live);
    }
    state.fail_at = 0;
    assert(qjs_intl_case_map(&allocator, bytes("en"), (QJSIntlUTF16){expansion, 1}, 1, 1,
                             &result) == QJS_INTL_OVERFLOW);
    assert(!result.text && !result.length && !state.live);
    assert(!qjs_intl_case_map(&allocator, bytes("en"), (QJSIntlUTF16){0}, 0, 10, &result));
    assert(!result.text && !result.length && !state.live);
    assert(qjs_intl_case_map(&allocator, bytes("1"), (QJSIntlUTF16){input, 4}, 1, 1024, &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!result.text && !result.length && !state.live);
    qjs_intl_case_result_clear(&allocator, NULL);
    return 0;
}
