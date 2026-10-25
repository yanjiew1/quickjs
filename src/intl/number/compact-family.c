/* CLDR raw-zero, compact alpha adjacency and coherent family divisors.
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
#include "compact-family.h"
#include <string.h>

static int currency(QJSIntlNumberTokenKind kind)
{
    return kind >= QJS_INTL_NUMBER_TOKEN_CURRENCY && kind <= QJS_INTL_NUMBER_TOKEN_CURRENCY_NARROW;
}

static int alpha_boundary(const QJSIntlCompactData *data, const QJSIntlCompactPattern *pattern,
                          const QJSIntlCurrencyBoundary *boundary)
{
    const QJSIntlNumberAffix *form = &pattern->forms[QJS_INTL_NUMBER_PATTERN_POSITIVE];
    QJSIntlNumberPatternToken token;
    QJSIntlNumberTokenKind previous = QJS_INTL_NUMBER_TOKEN_COUNT;
    uint32_t i;
    for (i = 0; i < form->count; i++) {
        qjs_intl_compact_token_at(data, form, i, &token);
        if ((currency(previous) && token.kind == QJS_INTL_NUMBER_TOKEN_NUMBER &&
             boundary->compact_before_number) ||
            (previous == QJS_INTL_NUMBER_TOKEN_NUMBER && currency(token.kind) && boundary->after_number))
            return 1;
        previous = token.kind;
    }
    return 0;
}

static int usable(const QJSIntlCompactPattern *pattern)
{
    return pattern->state == QJS_INTL_NUMBER_PATTERN_VALUE && pattern->mode != QJS_INTL_COMPACT_FALLBACK;
}

QJSIntlStatus qjs_intl_compact_family_at(const QJSIntlCompactData *data, const QJSIntlCompactMap *map,
                                        uint32_t magnitude, const QJSIntlCurrencyBoundary *boundary,
                                        QJSIntlCompactFamily *out)
{
    QJSIntlCompactFamily result = {0};
    QJSIntlCompactPattern alpha;
    QJSIntlStatus status;
    unsigned int count;
    int alpha_other;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_compact_pattern_find(data, map, magnitude, QJS_INTL_COMPACT_OTHER,
                                           QJS_INTL_COMPACT_STANDARD, &result.baseline);
    if (status)
        return status;
    if (result.baseline.state != QJS_INTL_NUMBER_PATTERN_VALUE)
        return QJS_INTL_UNSUPPORTED;
    result.requested = QJS_INTL_COMPACT_STANDARD;
    if (boundary && result.baseline.mode != QJS_INTL_COMPACT_FALLBACK) {
        status = qjs_intl_compact_pattern_find(data, map, magnitude, QJS_INTL_COMPACT_OTHER,
                                               QJS_INTL_COMPACT_ALPHA, &alpha);
        if (status != QJS_INTL_OK && status != QJS_INTL_UNSUPPORTED)
            return status;
        alpha_other = !status && usable(&alpha);
        result.alpha_available = alpha_other;
        if (alpha_boundary(data, &result.baseline, boundary)) {
            result.requested = QJS_INTL_COMPACT_ALPHA;
            if (alpha_other)
                result.baseline = alpha;
        }
        /* Family capability replaces legacy spacing independently of the
           divisor: sparse alpha count leaves need not provide an OTHER. */
        for (count = 0; !result.alpha_available && count < QJS_INTL_COMPACT_OTHER; count++) {
            status = qjs_intl_compact_pattern_find(data, map, magnitude, count,
                                                   QJS_INTL_COMPACT_ALPHA, &alpha);
            if (status != QJS_INTL_OK && status != QJS_INTL_UNSUPPORTED)
                return status;
            result.alpha_available = !status && usable(&alpha);
        }
    }
    *out = result;
    return QJS_INTL_OK;
}

static QJSIntlStatus candidate(const QJSIntlCompactData *data, const QJSIntlCompactMap *map,
                              const QJSIntlCompactFamily *family, QJSIntlCompactCount count,
                              QJSIntlCompactVariant variant, QJSIntlCompactPattern *out)
{
    QJSIntlCompactPattern pattern;
    QJSIntlStatus status = qjs_intl_compact_pattern_find(data, map, family->baseline.magnitude,
                                                        count, variant, &pattern);
    if (status)
        return status;
    if (!usable(&pattern) || pattern.exponent != family->baseline.exponent)
        return QJS_INTL_UNSUPPORTED;
    *out = pattern;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_compact_family_pattern(const QJSIntlCompactData *data,
                                             const QJSIntlCompactMap *map,
                                             const QJSIntlCompactFamily *family,
                                             QJSIntlCompactCount count, QJSIntlCompactPattern *out)
{
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !family)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!usable(&family->baseline))
        return QJS_INTL_UNSUPPORTED;
    status = candidate(data, map, family, count, family->requested, out);
    if (status == QJS_INTL_UNSUPPORTED && family->requested == QJS_INTL_COMPACT_ALPHA &&
        family->baseline.variant == QJS_INTL_COMPACT_STANDARD)
        status = candidate(data, map, family, count, QJS_INTL_COMPACT_STANDARD, out);
    return status;
}
