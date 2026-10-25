/* Shared visible append-item lookup and two-slot interpretation.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT.
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
#include "append-layout.h"
#include <string.h>

QJSIntlStatus qjs_intl_date_append_layout(const QJSIntlDateAvailableSources *sources,
                                         QJSIntlBytes key, QJSIntlDateAppendLayout *out)
{
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !key.data || !key.length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!sources)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < sources->count; i++) {
        QJSIntlDateAppendLayout result = {0};
        QJSIntlDateAvailableGroup group;
        QJSIntlStatus status = qjs_intl_date_available_source_at(sources, i,
                                                                &result.entry, &result.origin);
        if (!status)
            status = qjs_intl_date_available_group_at(&sources->data, result.origin.group, &group);
        if (status)
            return status;
        if (group.family != QJS_INTL_DATE_AVAILABLE_APPEND)
            return QJS_INTL_INVALID_ARGUMENT;
        if (result.entry.plural != QJS_INTL_DATE_NO_PLURAL || result.entry.alt.length ||
            result.entry.code.length != key.length || memcmp(result.entry.code.data, key.data, key.length))
            continue;
        if (result.entry.blocked || !result.entry.text.length)
            return QJS_INTL_UNSUPPORTED;
        status = qjs_intl_pair_pattern_parse(result.entry.text, &result.pattern);
        if (!status)
            *out = result;
        return status;
    }
    return QJS_INTL_UNSUPPORTED;
}
