/* ICU4X digital-duration metadata for shared Number/List composition.
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
#include "provider.h"
#include "digital.h"
#include "../icu4x/common.h"
#include "../../icu4x/duration.h"
#include <string.h>

/* Quote an already decoded LDML literal. An all-apostrophe literal must use
   doubled apostrophes alone: adding delimiters would invent another quote. */
static size_t separator(char *out, const uint8_t *text, size_t length)
{
    size_t i, at = 0;
    int delimit = 0;
    for (i = 0; i < length; i++)
        if (text[i] != '\'')
            delimit = 1;
    if (delimit)
        out[at++] = '\'';
    for (i = 0; i < length; i++) {
        out[at++] = (char)text[i];
        if (text[i] == '\'')
            out[at++] = '\'';
    }
    if (delimit)
        out[at++] = '\'';
    return at;
}

QJSIntlStatus qjs_intl_duration_digital_pattern(const QJSIntlAllocator *allocator,
                                                QJSIntlBytes locale, char **out)
{
    QJSICU4XDuration *handle = NULL;
    QJSICU4XDurationDigital data;
    QJSIntlStatus status;
    char *pattern = NULL;
    size_t at = 0, i;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free ||
        (!locale.data && locale.length))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_icu4x_status(
        qjs_icu4x_duration_open((const uint8_t *)locale.data, locale.length, &handle));
    if (!status)
        status = qjs_icu4x_status(qjs_icu4x_duration_digital(handle, &data));
    if (!status && (data.hour_width < 1 || data.hour_width > 2))
        status = QJS_INTL_DATA_ERROR;
    if (!status && data.length > (SIZE_MAX - 12) / 4)
        status = QJS_INTL_OVERFLOW;
    if (!status) {
        /* Two quoted separators can each double every apostrophe. Six field
           bytes, four delimiters and the terminator fit the remaining bound. */
        pattern = allocator->malloc(allocator->opaque, data.length * 4 + 12);
        if (!pattern)
            status = QJS_INTL_NO_MEMORY;
    }
    if (!status) {
        for (i = 0; i < data.hour_width; i++)
            pattern[at++] = 'h';
        at += separator(pattern + at, data.separator, data.length);
        pattern[at++] = 'm';
        pattern[at++] = 'm';
        at += separator(pattern + at, data.separator, data.length);
        pattern[at++] = 's';
        pattern[at++] = 's';
        pattern[at] = 0;
        status = qjs_intl_duration_digital_validate((QJSIntlBytes){pattern, at});
    }
    qjs_icu4x_duration_close(handle);
    if (status) {
        if (pattern)
            allocator->free(allocator->opaque, pattern);
        return status;
    }
    *out = pattern;
    return QJS_INTL_OK;
}
