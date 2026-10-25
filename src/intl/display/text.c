/* Present display-name text owns one exact UTF-16 allocation, possibly empty.
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
#include "text.h"
#include "../common/format-builder.h"
#include <string.h>

void qjs_intl_name_text_clear(const QJSIntlAllocator *allocator, QJSIntlNameText *out)
{
    if (!out)
        return;
    if (allocator && allocator->free && out->text)
        allocator->free(allocator->opaque, out->text);
    memset(out, 0, sizeof(*out));
}

QJSIntlStatus qjs_intl_name_text_copy(const QJSIntlAllocator *allocator, QJSIntlBytes text,
                                      size_t limit, QJSIntlNameText *out)
{
    QJSIntlFormatBuilder builder = {0};
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !limit ||
        limit > SIZE_MAX / sizeof(uint16_t) || text.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    builder.limit = limit;
    status = qjs_intl_format_builder_text(&builder, text, NULL, NULL);
    if (status)
        return status;
    if (builder.length) {
        builder.result.text = allocator->malloc(allocator->opaque, builder.length * sizeof(uint16_t));
        if (!builder.result.text)
            return QJS_INTL_NO_MEMORY;
        builder.length = 0;
        status = qjs_intl_format_builder_text(&builder, text, NULL, NULL);
        if (status) {
            allocator->free(allocator->opaque, builder.result.text);
            return status;
        }
    }
    *out = (QJSIntlNameText){builder.result.text, builder.length, 1};
    return QJS_INTL_OK;
}
