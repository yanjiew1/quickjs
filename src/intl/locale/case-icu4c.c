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
#include "../icu4c/common.h"
#include "../common/locale-grammar.h"
#include <limits.h>
#include <string.h>
#include <unicode/ustring.h>

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
    static const UChar empty = 0;
    const UChar *source = input.data ? input.data : &empty;
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlStatus error;
    QJSIntlCaseResult result = {0};
    char locale[9];
    size_t language_length = 0;
    int32_t length, actual;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free ||
        (input.length && !input.data) || (unsigned int)lower > 1 || !limit)
        return QJS_INTL_INVALID_ARGUMENT;
    if (input.length > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    /* Unicode special casing is language-sensitive, not variant/extension
       sensitive. Pass that canonical language directly, including for tags
       longer than ICU's legacy Locale representation can store. */
    if (!tag.data)
        return QJS_INTL_INVALID_ARGUMENT;
    while (language_length < tag.length && tag.data[language_length] != '-')
        language_length++;
    if (language_length >= sizeof(locale) ||
        !qjs_intl_unicode_subtag_well_formed(QJS_INTL_LANGUAGE_SUBTAG, tag.data, language_length))
        return QJS_INTL_INVALID_ARGUMENT;
    memcpy(locale, tag.data, language_length);
    locale[language_length] = 0;
    length = lower ? u_strToLower(NULL, 0, source, (int32_t)input.length, locale, &status)
                   : u_strToUpper(NULL, 0, source, (int32_t)input.length, locale, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR)
        status = U_ZERO_ERROR;
    if (U_FAILURE(status)) {
        error = qjs_icu4c_status(status);
        goto done;
    }
    if (length < 0 || length == INT32_MAX || (size_t)length > limit ||
        (size_t)length + 1 > SIZE_MAX / sizeof(*result.text)) {
        error = QJS_INTL_OVERFLOW;
        goto done;
    }
    if (length) {
        result.text =
            allocator->malloc(allocator->opaque, ((size_t)length + 1) * sizeof(*result.text));
        if (!result.text) {
            error = QJS_INTL_NO_MEMORY;
            goto done;
        }
        actual = lower ? u_strToLower(result.text, length + 1, source, (int32_t)input.length,
                                      locale, &status)
                       : u_strToUpper(result.text, length + 1, source, (int32_t)input.length,
                                      locale, &status);
        if (U_FAILURE(status) || actual != length) {
            error = U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_DATA_ERROR;
            goto done;
        }
    }
    result.length = (size_t)length;
    *out = result;
    memset(&result, 0, sizeof(result));
    error = QJS_INTL_OK;
done:
    qjs_intl_case_result_clear(allocator, &result);
    return error;
}
