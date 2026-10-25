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
#include "collation-text.h"
#include "../../src/cutils/cutils.h"
#include "../../src/unicode/libunicode.h"
#include <limits.h>

typedef struct NormalizationAllocator {
    const QJSIntlAllocator *allocator;
    QJSIntlStatus failure;
} NormalizationAllocator;

static void *normalize_realloc(void *opaque, void *ptr, size_t size)
{
    NormalizationAllocator *state = opaque;
    const QJSIntlAllocator *allocator = state->allocator;
    void *result;
    if (!size) {
        if (ptr)
            allocator->free(allocator->opaque, ptr);
        return NULL;
    }
#if SIZE_MAX > UINT32_MAX
    /* Bound capacity before the Unicode library converts its output count to
       int. Even an int-sized input can expand beyond that count under NFD. */
    if (size > (size_t)INT_MAX * sizeof(uint32_t)) {
        state->failure = QJS_INTL_OVERFLOW;
        return NULL;
    }
#endif
    result = ptr ? allocator->realloc(allocator->opaque, ptr, size) :
        allocator->malloc(allocator->opaque, size);
    if (!result)
        state->failure = QJS_INTL_NO_MEMORY;
    return result;
}

void qjs_intl_compiler_text_clear(const QJSIntlAllocator *allocator, QJSIntlCompilerText *text)
{
    if (text->codes)
        allocator->free(allocator->opaque, text->codes);
    *text = (QJSIntlCompilerText){0};
}

QJSIntlStatus qjs_intl_compiler_text_nfd(const QJSIntlAllocator *allocator,
                                         const uint32_t *codes, uint32_t count,
                                         QJSIntlCompilerText *out)
{
    uint32_t i;
    int length;
    NormalizationAllocator adapter = {allocator, QJS_INTL_OK};
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = (QJSIntlCompilerText){0};
    if (!allocator || !allocator->malloc || !allocator->realloc || !allocator->free || (!codes && count))
        return QJS_INTL_INVALID_ARGUMENT;
    if (count > INT_MAX)
        return QJS_INTL_OVERFLOW;
    for (i = 0; i < count; i++) {
        if (codes[i] > 0x10ffff || (codes[i] >= 0xd800 && codes[i] <= 0xdfff))
            return QJS_INTL_INVALID_ARGUMENT;
    }
    if (!count)
        return QJS_INTL_OK;
    length = unicode_normalize(&out->codes, codes, count, UNICODE_NFD, &adapter, normalize_realloc);
    if (length < 0) {
        qjs_intl_compiler_text_clear(allocator, out);
        return adapter.failure ? adapter.failure : QJS_INTL_OVERFLOW;
    }
    out->count = length;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_compiler_text_inert(const QJSIntlAllocator *allocator, uint32_t code, int *out)
{
    QJSIntlCompilerText text = {0};
    QJSIntlStatus status;
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = 0;
    if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
        return QJS_INTL_INVALID_ARGUMENT;
    if (unicode_get_cc(code))
        return QJS_INTL_OK;
    status = qjs_intl_compiler_text_nfd(allocator, &code, 1, &text);
    if (!status)
        *out = text.count == 1 && text.codes[0] == code;
    qjs_intl_compiler_text_clear(allocator, &text);
    return status;
}
