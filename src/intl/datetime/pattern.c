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
#include "pattern.h"
#include <string.h>

typedef struct Scan {
    QJSIntlDatePatternToken *tokens;
    char *text;
    size_t count, bytes;
    int literal;
} Scan;

static QJSIntlStatus scalar(QJSIntlBytes source, size_t *at, uint32_t *out)
{
    const unsigned char *bytes = (const unsigned char *)source.data;
    uint32_t c = bytes[(*at)++], minimum;
    unsigned int n, i;
    if (c < 0x80) {
        *out = c;
        return c ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
    }
    if (c >= 0xc2 && c <= 0xdf) {
        n = 1; minimum = 0x80; c &= 31;
    } else if (c >= 0xe0 && c <= 0xef) {
        n = 2; minimum = 0x800; c &= 15;
    } else if (c >= 0xf0 && c <= 0xf4) {
        n = 3; minimum = 0x10000; c &= 7;
    } else {
        return QJS_INTL_DATA_ERROR;
    }
    if (n > source.length - *at)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < n; i++) {
        unsigned int next = bytes[(*at)++];
        if ((next & 0xc0) != 0x80)
            return QJS_INTL_DATA_ERROR;
        c = (c << 6) | (next & 63);
    }
    if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        return QJS_INTL_DATA_ERROR;
    *out = c;
    return QJS_INTL_OK;
}

static QJSIntlStatus literal(Scan *state, const char *text, size_t length)
{
    if (length > SIZE_MAX - state->bytes)
        return QJS_INTL_OVERFLOW;
    if (!state->literal) {
        if (state->count == SIZE_MAX)
            return QJS_INTL_OVERFLOW;
        if (state->tokens)
            state->tokens[state->count] = (QJSIntlDatePatternToken){{state->text + state->bytes, 0}, 0, 0};
        state->count++;
        state->literal = 1;
    }
    if (state->tokens) {
        memcpy(state->text + state->bytes, text, length);
        state->tokens[state->count - 1].literal.length += length;
    }
    state->bytes += length;
    return QJS_INTL_OK;
}

static QJSIntlStatus field(Scan *state, unsigned int symbol, size_t width)
{
    if (state->count == SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    if (state->tokens)
        state->tokens[state->count] = (QJSIntlDatePatternToken){{0}, width, symbol};
    state->count++;
    state->literal = 0;
    return QJS_INTL_OK;
}

static int letter(uint32_t c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static QJSIntlStatus scan(QJSIntlBytes source, int combination, Scan *state)
{
    size_t at = 0;
    unsigned int seen = 0;
    int quoted = 0;
    while (at < source.length) {
        size_t start = at;
        uint32_t c;
        QJSIntlStatus status = scalar(source, &at, &c);
        if (status)
            return status;
        if (c == '\'') {
            if (at < source.length && source.data[at] == '\'') {
                at++;
                status = literal(state, source.data + start, 1);
            } else {
                quoted = !quoted;
            }
        } else if (combination && !quoted && c == '{' && source.length - start >= 3 &&
                   (source.data[start + 1] == '0' || source.data[start + 1] == '1') && source.data[start + 2] == '}') {
            unsigned int symbol = (unsigned char)source.data[start + 1];
            unsigned int bit = 1U << (symbol - '0');
            if (seen & bit)
                return QJS_INTL_DATA_ERROR;
            seen |= bit;
            at = start + 3;
            status = field(state, symbol, 0);
        } else if (!quoted && letter(c)) {
            /* This is the stored-pattern alphabet, not an ECMA candidate
               subset. Input-skeleton-only j/J/C must never enter this layer. */
            if (!strchr("GyYuUrQqMLlwWdDFgEecabBhHKkmsSAzZOvVXx", (int)c))
                return QJS_INTL_DATA_ERROR;
            while (at < source.length && (unsigned char)source.data[at] == c)
                at++;
            status = field(state, c, at - start);
        } else {
            status = literal(state, source.data + start, at - start);
        }
        if (status)
            return status;
    }
    return quoted || (combination && seen != 3) ? QJS_INTL_DATA_ERROR : QJS_INTL_OK;
}

static QJSIntlStatus compile(const QJSIntlAllocator *allocator, QJSIntlBytes source,
                             int combination, int interval, QJSIntlDatePattern *out)
{
    QJSIntlDatePattern result = {0};
    Scan measure = {0}, write = {0};
    QJSIntlStatus status;
    size_t token_bytes;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !source.data || !source.length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (interval) {
        static const char *const prefixes[] = {"earliestFirst:", "latestFirst:"};
        size_t i;
        for (i = 0; i < sizeof(prefixes) / sizeof(*prefixes); i++) {
            size_t length = strlen(prefixes[i]);
            if (source.length >= length && !memcmp(source.data, prefixes[i], length)) {
                source.data += length;
                source.length -= length;
                result.order = i ? QJS_INTL_DATE_INTERVAL_LATEST_FIRST : QJS_INTL_DATE_INTERVAL_EARLIEST_FIRST;
                break;
            }
        }
    }
    if (!source.length)
        return QJS_INTL_DATA_ERROR;
    status = scan(source, combination, &measure);
    if (status)
        return status;
    if (measure.count > SIZE_MAX / sizeof(*result.tokens))
        return QJS_INTL_OVERFLOW;
    token_bytes = measure.count * sizeof(*result.tokens);
    if (measure.bytes > SIZE_MAX - token_bytes)
        return QJS_INTL_OVERFLOW;
    result.storage = allocator->malloc(allocator->opaque, token_bytes + measure.bytes);
    if (!result.storage)
        return QJS_INTL_NO_MEMORY;
    result.tokens = result.storage;
    result.count = measure.count;
    write.tokens = result.tokens;
    write.text = (char *)result.storage + token_bytes;
    status = scan(source, combination, &write);
    if (status) {
        allocator->free(allocator->opaque, result.storage);
        return status;
    }
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_pattern_compile(const QJSIntlAllocator *allocator,
                                          QJSIntlBytes source, QJSIntlDatePattern *out)
{
    return compile(allocator, source, 0, 0, out);
}

QJSIntlStatus qjs_intl_date_combination_compile(const QJSIntlAllocator *allocator,
                                              QJSIntlBytes source, QJSIntlDatePattern *out)
{
    return compile(allocator, source, 1, 0, out);
}

QJSIntlStatus qjs_intl_date_interval_compile(const QJSIntlAllocator *allocator,
                                           QJSIntlBytes source, QJSIntlDatePattern *out)
{
    return compile(allocator, source, 0, 1, out);
}

static QJSIntlStatus append_tokens(Scan *state, const QJSIntlDatePattern *pattern, int ordinary)
{
    size_t i;
    for (i = 0; i < pattern->count; i++) {
        const QJSIntlDatePatternToken *token = &pattern->tokens[i];
        QJSIntlStatus status;
        if (ordinary && (token->symbol == '0' || token->symbol == '1'))
            return QJS_INTL_INVALID_ARGUMENT;
        status = token->symbol ? field(state, token->symbol, token->width) :
                                 literal(state, token->literal.data, token->literal.length);
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus compose_tokens(Scan *state, const QJSIntlDatePattern *connector,
                                    const QJSIntlDatePattern *time, const QJSIntlDatePattern *date)
{
    unsigned int seen = 0;
    size_t i;
    for (i = 0; i < connector->count; i++) {
        const QJSIntlDatePatternToken *token = &connector->tokens[i];
        QJSIntlStatus status;
        if (token->symbol == '0' || token->symbol == '1') {
            unsigned int bit = 1U << (token->symbol - '0');
            if (token->width || (seen & bit))
                return QJS_INTL_INVALID_ARGUMENT;
            seen |= bit;
            status = append_tokens(state, token->symbol == '0' ? time : date, 1);
        } else {
            status = token->symbol ? field(state, token->symbol, token->width) :
                                     literal(state, token->literal.data, token->literal.length);
        }
        if (status)
            return status;
    }
    return seen == 3 ? QJS_INTL_OK : QJS_INTL_INVALID_ARGUMENT;
}

static QJSIntlStatus replace_tokens(Scan *state, const QJSIntlDatePattern *pattern,
                                    unsigned int symbol, const QJSIntlDatePattern *replacement)
{
    size_t i, seen = 0;
    for (i = 0; i < pattern->count; i++) {
        const QJSIntlDatePatternToken *token = &pattern->tokens[i];
        QJSIntlStatus status;
        if (token->symbol == symbol) {
            if (++seen > 1)
                return QJS_INTL_INVALID_ARGUMENT;
            status = append_tokens(state, replacement, 1);
        } else {
            status = token->symbol ? field(state, token->symbol, token->width) :
                                     literal(state, token->literal.data, token->literal.length);
        }
        if (status)
            return status;
    }
    return seen == 1 ? QJS_INTL_OK : QJS_INTL_INVALID_ARGUMENT;
}

static int compiled(const QJSIntlDatePattern *pattern)
{
    return pattern && pattern->tokens && pattern->count;
}

static QJSIntlStatus transform(const QJSIntlAllocator *allocator, const QJSIntlDatePattern *pattern,
                               const QJSIntlDatePattern *time, const QJSIntlDatePattern *date,
                               unsigned int replace_symbol, QJSIntlDatePattern *out)
{
    QJSIntlDatePattern result = {0};
    Scan measure = {0}, write = {0};
    QJSIntlStatus status;
    size_t token_bytes;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !compiled(pattern))
        return QJS_INTL_INVALID_ARGUMENT;
    if (replace_symbol) {
        if (!compiled(time) || date || pattern->order != QJS_INTL_DATE_INTERVAL_DEFAULT ||
            time->order != QJS_INTL_DATE_INTERVAL_DEFAULT)
            return QJS_INTL_INVALID_ARGUMENT;
        status = replace_tokens(&measure, pattern, replace_symbol, time);
    } else if (time || date) {
        if (!compiled(time) || !compiled(date) || pattern->order != QJS_INTL_DATE_INTERVAL_DEFAULT ||
            time->order != QJS_INTL_DATE_INTERVAL_DEFAULT || date->order != QJS_INTL_DATE_INTERVAL_DEFAULT)
            return QJS_INTL_INVALID_ARGUMENT;
        status = compose_tokens(&measure, pattern, time, date);
    } else {
        status = append_tokens(&measure, pattern, 0);
    }
    if (status)
        return status;
    if (measure.count > SIZE_MAX / sizeof(*result.tokens))
        return QJS_INTL_OVERFLOW;
    token_bytes = measure.count * sizeof(*result.tokens);
    if (measure.bytes > SIZE_MAX - token_bytes)
        return QJS_INTL_OVERFLOW;
    result.storage = allocator->malloc(allocator->opaque, token_bytes + measure.bytes);
    if (!result.storage)
        return QJS_INTL_NO_MEMORY;
    result.tokens = result.storage;
    result.count = measure.count;
    result.order = pattern->order;
    write.tokens = result.tokens;
    write.text = (char *)result.storage + token_bytes;
    status = replace_symbol ? replace_tokens(&write, pattern, replace_symbol, time) :
             time ? compose_tokens(&write, pattern, time, date) : append_tokens(&write, pattern, 0);
    if (status) {
        allocator->free(allocator->opaque, result.storage);
        return status;
    }
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_pattern_copy(const QJSIntlAllocator *allocator,
                                       const QJSIntlDatePattern *pattern, QJSIntlDatePattern *out)
{
    return transform(allocator, pattern, NULL, NULL, 0, out);
}

QJSIntlStatus qjs_intl_date_pattern_combine(const QJSIntlAllocator *allocator,
                                          const QJSIntlDatePattern *connector,
                                          const QJSIntlDatePattern *time,
                                          const QJSIntlDatePattern *date, QJSIntlDatePattern *out)
{
    if (!time || !date) {
        if (out)
            memset(out, 0, sizeof(*out));
        return QJS_INTL_INVALID_ARGUMENT;
    }
    return transform(allocator, connector, time, date, 0, out);
}

QJSIntlStatus qjs_intl_date_pattern_join(const QJSIntlAllocator *allocator,
                                       const QJSIntlPairPattern *layout,
                                       const QJSIntlDatePattern *first,
                                       const QJSIntlDatePattern *second, QJSIntlDatePattern *out)
{
    QJSIntlDatePatternToken tokens[5];
    QJSIntlDatePattern connector = {0};
    QJSIntlBytes fragments[3];
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !layout || layout->first > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    fragments[0] = layout->prefix;
    fragments[1] = layout->separator;
    fragments[2] = layout->suffix;
    connector.tokens = tokens;
    for (i = 0; i < 3; i++) {
        if ((!fragments[i].data && fragments[i].length) || fragments[i].length > PTRDIFF_MAX)
            return QJS_INTL_INVALID_ARGUMENT;
        if (fragments[i].length)
            tokens[connector.count++] = (QJSIntlDatePatternToken){fragments[i], 0, 0};
        if (i < 2)
            tokens[connector.count++] = (QJSIntlDatePatternToken){{0}, 0,
                                              '0' + (i ? 1 - layout->first : layout->first)};
    }
    return qjs_intl_date_pattern_combine(allocator, &connector, first, second, out);
}

QJSIntlStatus qjs_intl_date_pattern_replace(const QJSIntlAllocator *allocator,
                                          const QJSIntlDatePattern *pattern, unsigned int symbol,
                                          const QJSIntlDatePattern *replacement, QJSIntlDatePattern *out)
{
    if (!symbol || symbol == '0' || symbol == '1') {
        if (out)
            memset(out, 0, sizeof(*out));
        return QJS_INTL_INVALID_ARGUMENT;
    }
    return transform(allocator, pattern, replacement, NULL, symbol, out);
}

void qjs_intl_date_pattern_clear(const QJSIntlAllocator *allocator, QJSIntlDatePattern *pattern)
{
    if (!pattern)
        return;
    if (pattern->storage)
        allocator->free(allocator->opaque, pattern->storage);
    memset(pattern, 0, sizeof(*pattern));
}
