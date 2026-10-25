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
#include "period-spacing.h"
#include <string.h>

enum { GAP_NONE, GAP_ASCII, GAP_SPECIAL, GAP_OTHER };

static unsigned int clock_field(unsigned int symbol)
{
    return symbol == 'h' || symbol == 'H' || symbol == 'K' || symbol == 'k' ? 1 :
           symbol == 'm' ? 2 : symbol == 's' ? 4 : 0;
}

static int special(QJSIntlBytes text)
{
    return text.length == 3 && (unsigned char)text.data[0] == 0xe2 &&
           (unsigned char)text.data[1] == 0x80 &&
           ((unsigned char)text.data[2] == 0xaf || (unsigned char)text.data[2] == 0x89);
}

typedef struct Evidence {
    unsigned int fields, occurrences, periods;
} Evidence;

static Evidence scan(QJSIntlBytes source, unsigned int proof, int wanted)
{
    static const char *const directives[] = {"earliestFirst:", "latestFirst:"};
    Evidence result = {0};
    unsigned int previous = 0;
    size_t i = 0, j;
    int quoted = 0, gap = GAP_NONE;

    if (!source.data || !source.length)
        return result;
    for (j = 0; j < sizeof(directives) / sizeof(*directives); j++) {
        size_t length = strlen(directives[j]);
        if (source.length >= length && !memcmp(source.data, directives[j], length)) {
            i = length;
            break;
        }
    }
    while (i < source.length) {
        unsigned int c = (unsigned char)source.data[i++];
        if (c == '\'') {
            gap = GAP_OTHER;
            if (i < source.length && source.data[i] == '\'')
                i++;
            else
                quoted = !quoted;
            continue;
        }
        if (quoted) {
            gap = GAP_OTHER;
            continue;
        }
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            while (i < source.length && (unsigned char)source.data[i] == c)
                i++;
            if (c == 'a') {
                if (result.periods == 2)
                    return (Evidence){0};
                if ((previous & proof) && gap == wanted) {
                    result.fields |= previous;
                    result.occurrences |= 1U << result.periods;
                }
                result.periods++;
            }
            previous = clock_field(c);
            gap = GAP_NONE;
            continue;
        }
        if (gap != GAP_NONE) {
            gap = GAP_OTHER;
        } else if (c == ' ') {
            gap = GAP_ASCII;
        } else if (source.length - (i - 1) >= 3 &&
                   special((QJSIntlBytes){source.data + i - 1, 3})) {
            gap = GAP_SPECIAL;
            i += 2;
        } else {
            gap = GAP_OTHER;
        }
    }
    return quoted ? (Evidence){0} : result;
}

unsigned int qjs_intl_date_period_ascii_source(QJSIntlBytes source, QJSIntlBytes alternate)
{
    Evidence evidence;
    if (!alternate.data || alternate.length != 5 || memcmp(alternate.data, "ascii", 5))
        return 0;
    evidence = scan(source, 7, GAP_ASCII);
    return evidence.periods == 1 ? evidence.fields : 0;
}

unsigned int qjs_intl_date_period_ascii_mask(const QJSIntlDatePattern *pattern, unsigned int proof)
{
    unsigned int result = 0;
    size_t i;
    if (!pattern || !pattern->tokens || (proof & ~7U))
        return 0;
    for (i = 2; i < pattern->count; i++) {
        const QJSIntlDatePatternToken *gap = &pattern->tokens[i - 1];
        if (pattern->tokens[i].symbol == 'a' && !gap->symbol &&
            gap->literal.length == 1 && gap->literal.data[0] == ' ')
            result |= clock_field(pattern->tokens[i - 2].symbol) & proof;
    }
    return result;
}

void qjs_intl_date_period_ascii_interval(QJSIntlDatePattern *owned,
                                         QJSIntlBytes source, unsigned int proof)
{
    Evidence evidence;
    unsigned int period = 0;
    size_t i;
    if (!owned || !owned->storage || !owned->tokens || !proof || (proof & ~7U))
        return;
    evidence = scan(source, proof, GAP_SPECIAL);
    for (i = 0; i < owned->count; i++) {
        QJSIntlDatePatternToken *gap;
        if (owned->tokens[i].symbol != 'a')
            continue;
        if (period == 2)
            return;
        if ((evidence.occurrences & (1U << period++)) && i >= 2 &&
            (clock_field(owned->tokens[i - 2].symbol) & proof)) {
            gap = &owned->tokens[i - 1];
            if (!gap->symbol && special(gap->literal)) {
                /* interval_compile owns this disjoint literal storage. Shorten
                   its view in place; never mutate the raw CLDR source bytes. */
                ((char *)gap->literal.data)[0] = ' ';
                gap->literal.length = 1;
            }
        }
    }
}
