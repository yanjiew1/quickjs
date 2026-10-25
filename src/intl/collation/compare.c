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
#include "compare.h"
#include "elements.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include <string.h>

typedef struct ElementStream {
    QJSIntlCollationElements iterator;
    QJSIntlCollationElement *elements;
    size_t count, capacity;
    uint32_t numeric_weight;
    int shifted, variable_run;
    QJSIntlCollationElement small[8];
} ElementStream;

static void stream_clear(ElementStream *stream)
{
    if (stream->elements && stream->elements != stream->small)
        stream->iterator.match.allocator.free(stream->iterator.match.allocator.opaque, stream->elements);
    qjs_intl_collation_elements_clear(&stream->iterator);
}

typedef struct PrimaryWeight {
    uint32_t weight;
    QJSIntlCollationNumber number;
} PrimaryWeight;

/* Keep lower-level weights only while primaries tie. This avoids building two
   complete sort keys just to decide strings with an early primary difference. */
static QJSIntlStatus stream_primary(ElementStream *stream, PrimaryWeight *out)
{
    QJSIntlStatus status;
    QJSIntlCollationElement element;
    int present;
    memset(out, 0, sizeof(*out));
    for (;;) {
        if (stream->numeric_weight)
            status = qjs_intl_collation_elements_next_numeric(&stream->iterator, &element,
                                                              &out->number, &present);
        else
            status = qjs_intl_collation_elements_next(&stream->iterator, &element, &present);
        if (status || !present)
            return status;
        if (stream->shifted) {
            /* Shift variables and following primary-ignorables out of the
               levels exposed by Intl sensitivity. A new nonvariable primary
               (including a numeric token) ends the variable sequence. */
            if (out->number.length || (element.primary && !element.variable))
                stream->variable_run = 0;
            else if (element.variable)
                stream->variable_run = 1;
            if (element.variable || stream->variable_run)
                continue;
        }
        if (out->number.length) {
            /* One logical numeric CE: script and leading-zero count must not
               leak into lower levels once two numeric values are equal. */
            element.secondary = 0x20;
            element.tertiary = 2;
        }
        if (!element.primary && !element.secondary && !element.tertiary)
            continue;
        if (stream->count == stream->capacity) {
            QJSIntlAllocator *a = &stream->iterator.match.allocator;
            QJSIntlCollationElement *next;
            size_t capacity;
            if (stream->capacity > SIZE_MAX / sizeof(*next) / 2)
                return QJS_INTL_OVERFLOW;
            capacity = stream->capacity * 2;
            if (stream->elements == stream->small) {
                next = a->malloc(a->opaque, capacity * sizeof(*next));
                if (next)
                    memcpy(next, stream->small, stream->count * sizeof(*next));
            } else {
                next = a->realloc(a->opaque, stream->elements, capacity * sizeof(*next));
            }
            if (!next)
                return QJS_INTL_NO_MEMORY;
            stream->elements = next;
            stream->capacity = capacity;
        }
        stream->elements[stream->count++] = element;
        if (element.primary || out->number.length) {
            /* Doubling stored primaries leaves an insertion band immediately
               before ordinary zero without changing the serialized CE format. */
            out->weight = out->number.length ? stream->numeric_weight : 2U * element.primary;
            return QJS_INTL_OK;
        }
    }
}

static int compare_primary(const PrimaryWeight *left, const PrimaryWeight *right)
{
    uint32_t i;
    if (left->weight != right->weight)
        return left->weight < right->weight ? -1 : 1;
    if (!left->number.length)
        return 0;
    if (left->number.length != right->number.length)
        return left->number.length < right->number.length ? -1 : 1;
    for (i = 0; i < left->number.length; i++) {
        int x = lre_decimal_value(left->number.digits[i]);
        int y = lre_decimal_value(right->number.digits[i]);
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

/* Root case belongs to each CE, including Kana size variants. Unicode letter
   case cannot reconstruct it from a code point or an expansion as a whole. */
static int root_upper(unsigned int tertiary)
{
    return (tertiary >= 0x08 && tertiary <= 0x0c) || tertiary == 0x0e ||
        tertiary == 0x11 || tertiary == 0x12 || tertiary == 0x1d;
}

enum { LEVEL_SECONDARY, LEVEL_TERTIARY, LEVEL_CASE };

static unsigned int next_level(const ElementStream *stream, size_t *at, int level,
                                QJSIntlCollationCaseFirst case_first)
{
    while (*at < stream->count) {
        const QJSIntlCollationElement *element = &stream->elements[(*at)++];
        unsigned int weight, c;
        if (level == LEVEL_SECONDARY) {
            weight = element->secondary;
        } else if (level == LEVEL_TERTIARY && case_first == QJS_INTL_COLLATION_CASE_OFF) {
            weight = element->tertiary;
        } else {
            c = root_upper(element->tertiary) ? 3 : 1;
            if (case_first == QJS_INTL_COLLATION_UPPER_FIRST)
                c = 4 - c;
            if (level == LEVEL_CASE) {
                /* Primary-ignorable accents must not distinguish a from ä
                   when sensitivity ignores accents but retains case. */
                weight = element->primary ? c : 0;
            } else if (!element->tertiary) {
                weight = 0;
            } else {
                /* Tertiary-only CEs keep the highest artificial case rank
                   even in upper-first order (TR35's WF2 requirement). */
                if (!element->primary && !element->secondary)
                    c = 3;
                weight = (c << 16) | element->tertiary;
            }
        }
        if (weight)
            return weight;
    }
    return 0;
}

QJSIntlStatus qjs_intl_collation_compare_root(const QJSIntlCollationContexts *data,
                                             const uint16_t *left, size_t left_length,
                                             const uint16_t *right, size_t right_length,
                                             const QJSIntlAllocator *allocator,
                                             const QJSIntlRootCollationOptions *options, int *order)
{
    ElementStream streams[2] = {0};
    QJSIntlStatus status;
    unsigned int x, y;
    uint32_t numeric_weight = 0;
    QJSIntlCollationSensitivity sensitivity = options ? options->sensitivity : QJS_INTL_COLLATION_VARIANT;
    QJSIntlCollationCaseFirst case_first = options ? options->case_first : QJS_INTL_COLLATION_CASE_OFF;
    int level, levels[2], level_count;
    if (!order)
        return QJS_INTL_INVALID_ARGUMENT;
    *order = 0;
    if (!data || !allocator || !allocator->malloc || !allocator->realloc ||
        !allocator->free || (!left && left_length) || (!right && right_length))
        return QJS_INTL_INVALID_ARGUMENT;
    if ((unsigned int)sensitivity > QJS_INTL_COLLATION_CASE ||
        (unsigned int)case_first > QJS_INTL_COLLATION_LOWER_FIRST)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!data->section.bytes)
        return QJS_INTL_UNSUPPORTED;
    if (left_length > SIZE_MAX / sizeof(*left) || right_length > SIZE_MAX / sizeof(*right))
        return QJS_INTL_OVERFLOW;
    if (options && options->numeric) {
        uint32_t zero = '0';
        QJSIntlCollationEntry entry;
        QJSIntlCollationElement element;
        status = qjs_intl_collation_root_find(&data->root, &zero, 1, &entry);
        if (status || !entry.element_count)
            return QJS_INTL_DATA_ERROR;
        qjs_intl_collation_element_at(&entry, 0, &element);
        if (!element.primary)
            return QJS_INTL_DATA_ERROR;
        numeric_weight = 2U * element.primary - 1;
    }
    /* Identity is safe even across contraction boundaries; a common-prefix
       shortcut would not be, since context can change following elements. */
    if (left_length == right_length &&
        (!left_length || !memcmp(left, right, left_length * sizeof(*left))))
        return QJS_INTL_OK;
    status = qjs_intl_collation_elements_init(&streams[0].iterator, data,
                                              left, left_length, allocator);
    if (status)
        goto done;
    status = qjs_intl_collation_elements_init(&streams[1].iterator, data,
                                              right, right_length, allocator);
    if (status)
        goto done;
    for (level = 0; level < 2; level++) {
        streams[level].numeric_weight = numeric_weight;
        streams[level].shifted = options && options->ignore_punctuation;
        streams[level].elements = streams[level].small;
        streams[level].capacity = sizeof(streams[level].small) / sizeof(*streams[level].small);
    }
    for (;;) {
        PrimaryWeight left_weight, right_weight;
        status = stream_primary(&streams[0], &left_weight);
        if (status)
            goto done;
        status = stream_primary(&streams[1], &right_weight);
        if (status)
            goto done;
        *order = compare_primary(&left_weight, &right_weight);
        if (*order)
            goto done;
        if (!left_weight.weight)
            break;
    }
    /* Each level compares complete nonzero streams. Comparing CEs one by one
       would let an early accent/case difference override a later primary. */
    if (sensitivity == QJS_INTL_COLLATION_BASE)
        goto done;
    levels[0] = sensitivity == QJS_INTL_COLLATION_CASE ? LEVEL_CASE : LEVEL_SECONDARY;
    levels[1] = LEVEL_TERTIARY;
    level_count = sensitivity == QJS_INTL_COLLATION_VARIANT ? 2 : 1;
    for (level = 0; level < level_count; level++) {
        size_t a = 0, b = 0;
        do {
            x = next_level(&streams[0], &a, levels[level], case_first);
            y = next_level(&streams[1], &b, levels[level], case_first);
            if (x != y) {
                *order = x < y ? -1 : 1;
                goto done;
            }
        } while (x);
    }
done:
    stream_clear(&streams[0]);
    stream_clear(&streams[1]);
    return status;
}
