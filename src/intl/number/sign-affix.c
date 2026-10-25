/* Separate sign differences from unsigned currency pattern decoration.
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
#include "sign-affix.h"
#include "../../cutils/cutils.h"
#include <limits.h>

typedef struct Span {
    const QJSIntlNumberPatternToken *tokens;
    size_t first, end, front, back;
} Span;

typedef struct Atom {
    QJSIntlNumberTokenKind kind;
    int scalar;
    size_t width;
} Atom;

static int atom(const Span *span, int last, Atom *out)
{
    const QJSIntlNumberPatternToken *token = span->tokens + (last ? span->end - 1 : span->first);
    size_t begin = last && span->end - span->first > 1 ? 0 : span->front;
    size_t end = token->text.length - (last || span->end - span->first == 1 ? span->back : 0);
    const uint8_t *next;
    int c;
    *out = (Atom){token->kind, 0, 0};
    if (token->kind != QJS_INTL_NUMBER_TOKEN_LITERAL)
        return 0;
    if (!token->text.data || end <= begin || end - begin > INT_MAX)
        return -1;
    if (last) {
        size_t at = end - 1;
        while (at > begin && ((unsigned char)token->text.data[at] & 0xc0) == 0x80)
            at--;
        begin = at;
    }
    c = unicode_from_utf8((const uint8_t *)token->text.data + begin, (int)(end - begin), &next);
    if (c <= 0 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff) ||
        (last && next != (const uint8_t *)token->text.data + end))
        return -1;
    out->scalar = c;
    out->width = next - ((const uint8_t *)token->text.data + begin);
    return 0;
}

static void consume(Span *span, int last, const Atom *value)
{
    const QJSIntlNumberPatternToken *token = span->tokens + (last ? span->end - 1 : span->first);
    int done;
    if (last) {
        span->back += value->width;
        done = value->kind != QJS_INTL_NUMBER_TOKEN_LITERAL ||
               span->back + (span->end - span->first == 1 ? span->front : 0) == token->text.length;
        if (done) {
            span->end--;
            span->back = 0;
        }
    } else {
        span->front += value->width;
        done = value->kind != QJS_INTL_NUMBER_TOKEN_LITERAL ||
               span->front + (span->end - span->first == 1 ? span->back : 0) == token->text.length;
        if (done) {
            span->first++;
            span->front = 0;
        }
    }
}

static QJSIntlStatus subtract(Span *positive, Span *selected)
{
    unsigned int last;
    for (last = 0; last < 2; last++) {
        while (positive->first < positive->end && selected->first < selected->end) {
            Atom a, b;
            if (atom(positive, last, &a) || atom(selected, last, &b))
                return QJS_INTL_DATA_ERROR;
            if (a.kind != b.kind || a.scalar != b.scalar)
                break;
            consume(positive, last, &a);
            consume(selected, last, &b);
        }
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus number_index(const QJSIntlNumberPatternToken *tokens, size_t count,
                                  size_t *out)
{
    size_t i, index = count;
    if (!tokens || !count || count > SIZE_MAX / sizeof(*tokens))
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < count; i++) {
        if (tokens[i].kind == QJS_INTL_NUMBER_TOKEN_NUMBER) {
            if (index != count)
                return QJS_INTL_DATA_ERROR;
            index = i;
        }
    }
    if (index == count)
        return QJS_INTL_DATA_ERROR;
    *out = index;
    return QJS_INTL_OK;
}

static QJSIntlStatus copy_span(const Span *span, QJSIntlNumberPatternToken *out, size_t *count)
{
    size_t i;
    for (i = span->first; i < span->end; i++) {
        QJSIntlNumberPatternToken token = span->tokens[i];
        if (token.kind == QJS_INTL_NUMBER_TOKEN_LITERAL) {
            size_t start = i == span->first ? span->front : 0;
            size_t end = token.text.length - (i + 1 == span->end ? span->back : 0);
            token.text.data += start;
            token.text.length = end - start;
        } else if (token.kind != QJS_INTL_NUMBER_TOKEN_MINUS && token.kind != QJS_INTL_NUMBER_TOKEN_PLUS)
            return QJS_INTL_UNSUPPORTED;
        out[(*count)++] = token;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_sign_affix(const QJSIntlNumberPatternToken *positive,
                                        size_t positive_count,
                                        QJSIntlNumberPatternToken *selected, size_t selected_count,
                                        size_t *count)
{
    size_t p, s, used = 0;
    Span pp, ps, sp, ss;
    QJSIntlStatus status;
    if (count)
        *count = 0;
    if (!count)
        return QJS_INTL_INVALID_ARGUMENT;
    status = number_index(positive, positive_count, &p);
    if (!status)
        status = number_index(selected, selected_count, &s);
    if (status)
        return status;
    pp = (Span){positive, 0, p, 0, 0};
    ps = (Span){positive, p + 1, positive_count, 0, 0};
    sp = (Span){selected, 0, s, 0, 0};
    ss = (Span){selected, s + 1, selected_count, 0, 0};
    status = subtract(&pp, &sp);
    if (!status)
        status = subtract(&ps, &ss);
    if (!status)
        status = copy_span(&sp, selected, &used);
    if (!status) {
        selected[used++] = (QJSIntlNumberPatternToken){QJS_INTL_NUMBER_TOKEN_NUMBER, {0}};
        status = copy_span(&ss, selected, &used);
    }
    if (!status)
        *count = used;
    return status;
}
