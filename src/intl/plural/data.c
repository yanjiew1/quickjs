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
#include "data.h"
#include "../native/wire.h"
#include <string.h>

#define CATEGORY_COUNT QJS_INTL_PLURAL_CATEGORY_COUNT
#define RANGE_WIDTH (CATEGORY_COUNT * CATEGORY_COUNT)

static uint32_t word(const uint8_t *bytes, size_t index)
{
    return qjs_intl_read_u32(bytes + index * 4);
}

static int ordered_rules(const uint8_t *records, uint32_t count)
{
    uint32_t i;
    for (i = 1; i < count; i++) {
        unsigned int j;
        for (j = 0; j < CATEGORY_COUNT; j++) {
            uint32_t a = word(records, (size_t)(i - 1) * CATEGORY_COUNT + j);
            uint32_t b = word(records, (size_t)i * CATEGORY_COUNT + j);
            if (a != b) {
                if (a > b)
                    return 0;
                break;
            }
        }
        if (j == CATEGORY_COUNT)
            return 0;
    }
    return 1;
}

static int canonical_condition(QJSIntlBytes text)
{
    size_t i;
    QJSIntlPluralOperands zero = {"0", 1, 0};
    int matches;
    for (i = 0; i < text.length; i++) {
        unsigned char c = (unsigned char)text.data[i];
        if (c < 0x20 || c > 0x7e ||
            (c == ' ' && (!i || i + 1 == text.length || text.data[i - 1] == ' ')))
            return 0;
    }
    return qjs_intl_plural_rule_evaluate(text, &zero, &matches) == QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_plural_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                        QJSIntlPluralData *out)
{
    QJSIntlPluralData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    size_t length, at, i, scratch_size;
    uint8_t *used, *checked;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_PLURALS, &result.section) ||
        !result.section.count || result.section.count != data->locales.count ||
        result.section.length < 8 || result.section.length % 4)
        return status;
    length = result.section.length / 4;
    result.rule_count = word(result.section.bytes, 0);
    result.range_count = word(result.section.bytes, 1);
    if (!result.rule_count || !result.range_count ||
        (uint64_t)result.rule_count > (uint64_t)result.section.count * 2 ||
        (uint64_t)result.range_count > (uint64_t)result.section.count * 2)
        return status;
    at = 2;
    if (result.section.count > (length - at) / 4)
        return status;
    result.maps = result.section.bytes + at * 4;
    at += (size_t)result.section.count * 4;
    if (result.rule_count > (length - at) / CATEGORY_COUNT)
        return status;
    result.rules = result.section.bytes + at * 4;
    at += (size_t)result.rule_count * CATEGORY_COUNT;
    if ((length - at) % (RANGE_WIDTH / 4) ||
        result.range_count != (length - at) / (RANGE_WIDTH / 4))
        return status;
    result.ranges = result.section.bytes + at * 4;
    if (!ordered_rules(result.rules, result.rule_count))
        return status;
    for (i = 1; i < result.range_count; i++) {
        if (memcmp(result.ranges + (i - 1) * RANGE_WIDTH, result.ranges + i * RANGE_WIDTH,
                   RANGE_WIDTH) >= 0)
            return status;
    }
    /* Counts are bounded by disjoint sections within the size-capped pack:
       offset words for strings, six words per rule and 36 bytes per range. */
    scratch_size = (size_t)result.rule_count + result.range_count + data->strings.count;
    used = allocator->malloc(allocator->opaque, scratch_size);
    if (!used)
        return QJS_INTL_NO_MEMORY;
    memset(used, 0, scratch_size);
    checked = used + result.rule_count + result.range_count;
    for (i = 0; i < (size_t)result.section.count * 4; i += 2) {
        uint32_t rule = word(result.maps, i), range = word(result.maps, i + 1);
        if (rule >= result.rule_count || range >= result.range_count)
            goto done;
        used[rule] = 1;
        used[result.rule_count + (size_t)range] = 1;
    }
    for (i = 0; i < (size_t)result.rule_count + result.range_count; i++) {
        if (!used[i])
            goto done;
    }
    for (i = 0; i < (size_t)result.rule_count * CATEGORY_COUNT; i++) {
        uint32_t index = word(result.rules, i);
        unsigned int category = i % CATEGORY_COUNT;
        QJSIntlBytes text;
        if (index == UINT32_MAX && category != QJS_INTL_PLURAL_OTHER)
            continue;
        if (qjs_intl_data_string(data, index, &text) ||
            !!text.length != (category != QJS_INTL_PLURAL_OTHER))
            goto done;
        if (!checked[index]) {
            if (!canonical_condition(text))
                goto done;
            checked[index] = 1;
        }
    }
    for (i = 0; i < (size_t)result.range_count * RANGE_WIDTH; i++) {
        if (result.ranges[i] >= CATEGORY_COUNT && result.ranges[i] != QJS_INTL_PLURAL_RANGE_ABSENT)
            goto done;
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, used);
    return status;
}

QJSIntlStatus qjs_intl_plural_rules_at(const QJSIntlPluralData *data, uint32_t locale,
                                       QJSIntlPluralType type, QJSIntlPluralRuleSet *out)
{
    uint32_t rule, range;
    unsigned int i;
    size_t map;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        (unsigned int)type > QJS_INTL_PLURAL_ORDINAL)
        return QJS_INTL_INVALID_ARGUMENT;
    map = (size_t)locale * 4 + (unsigned int)type * 2;
    rule = word(data->maps, map);
    range = word(data->maps, map + 1);
    for (i = 0; i < CATEGORY_COUNT; i++) {
        uint32_t index = word(data->rules, (size_t)rule * CATEGORY_COUNT + i);
        if (index != UINT32_MAX) {
            out->present |= 1u << i;
            qjs_intl_data_string(&data->data, index, &out->conditions[i]);
        }
    }
    out->ranges = data->ranges + (size_t)range * RANGE_WIDTH;
    return QJS_INTL_OK;
}
