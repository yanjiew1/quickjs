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
#include "case.h"
#include "../icu4x/common.h"
#include "../common/locale-grammar.h"
#include "../../icu4x/casing.h"
#include <limits.h>
#include <string.h>

typedef struct CaseOutput {
    uint16_t *text;
    size_t length, limit;
    QJSIntlStatus error;
} CaseOutput;

static int write_case(void *opaque, const uint16_t *text, size_t length)
{
    CaseOutput *state = opaque;

    if (length > state->limit - state->length) {
        state->error = QJS_INTL_OVERFLOW;
        return QJS_ICU4X_RANGE;
    }
    if (state->text && length)
        memcpy(state->text + state->length, text, length * sizeof(*text));
    state->length += length;
    return QJS_ICU4X_OK;
}

void qjs_intl_case_result_clear(const QJSIntlAllocator *allocator, QJSIntlCaseResult *out)
{
    if (!out)
        return;
    if (out->text)
        allocator->free(allocator->opaque, out->text);
    memset(out, 0, sizeof(*out));
}

QJSIntlStatus qjs_intl_case_map(const QJSIntlAllocator *allocator, QJSIntlBytes tag,
                                QJSIntlUTF16 input, int lower, size_t limit, QJSIntlCaseResult *out)
{
    QJSIntlCaseResult result = {0};
    CaseOutput state = {0};
    QJSICU4XTextSink sink = {&state, write_case};
    QJSIntlStatus error;
    size_t language = 0, expected;
    int status;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !tag.data ||
        (input.length && !input.data) || (unsigned)lower > 1 || !limit)
        return QJS_INTL_INVALID_ARGUMENT;
    if (input.length > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    // Casing depends on the canonical language, not variants or extensions.
    while (language < tag.length && tag.data[language] != '-')
        language++;
    if (language > 8 ||
        !qjs_intl_unicode_subtag_well_formed(QJS_INTL_LANGUAGE_SUBTAG, tag.data, language))
        return QJS_INTL_INVALID_ARGUMENT;
    state.limit = limit < INT32_MAX ? limit : INT32_MAX;
    status = qjs_icu4x_case_map((const uint8_t *)tag.data, language, input.data, input.length,
                                lower, sink);
    error = state.error ? state.error : qjs_icu4x_status(status);
    if (error)
        return error;
    expected = state.length;
    if (expected > SIZE_MAX / sizeof(*result.text))
        return QJS_INTL_OVERFLOW;
    if (expected) {
        result.text = allocator->malloc(allocator->opaque, expected * sizeof(*result.text));
        if (!result.text)
            return QJS_INTL_NO_MEMORY;
    }
    state.text = result.text;
    state.length = 0;
    state.limit = expected;
    status = qjs_icu4x_case_map((const uint8_t *)tag.data, language, input.data, input.length,
                                lower, sink);
    error = state.error ? state.error : qjs_icu4x_status(status);
    if (!error && state.length != expected)
        error = QJS_INTL_DATA_ERROR;
    if (error) {
        qjs_intl_case_result_clear(allocator, &result);
        return error;
    }
    result.length = expected;
    *out = result;
    return QJS_INTL_OK;
}
