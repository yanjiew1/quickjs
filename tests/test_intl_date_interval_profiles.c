/* Prepared interval source data and its exact, allocation-free query interface.
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
#include "../src/intl/datetime/interval-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        state->live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    QJSIntlBytes result = {text, strlen(text)};
    return result;
}

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static void check_english(const QJSIntlDateIntervalData *data)
{
    uint32_t calendar, locale, parent, group, head;
    QJSIntlDateIntervalGroup key = {0};
    for (calendar = 0; calendar < data->calendar_count; calendar++) {
        QJSIntlBytes name;
        assert(!qjs_intl_date_interval_calendar_at(data, calendar, &name));
        if (equal(name, "gregorian"))
            break;
    }
    assert(calendar < data->calendar_count);
    for (locale = 0; locale < data->section.count; locale++) {
        QJSIntlBytes name;
        assert(!qjs_intl_data_locale(&data->data, locale, &name, &parent));
        if (equal(name, "en"))
            break;
    }
    assert(locale < data->section.count);
    key.calendar = calendar;
    key.family = QJS_INTL_DATE_INTERVAL_ITEM;
    key.skeleton = bytes("yMd");
    assert(!qjs_intl_date_interval_find_group(data, &key, &group));
    assert(!qjs_intl_date_interval_head(data, locale, group, &head));
    while (head) {
        QJSIntlDateIntervalNode node;
        QJSIntlDateIntervalEntry entry;
        QJSIntlStatus status = qjs_intl_date_interval_lookup(data, head, bytes("d"), bytes(""), &entry);
        assert(!qjs_intl_date_interval_node(data, head, &node));
        if (status == QJS_INTL_OK) {
            assert(!entry.blocked && equal(entry.text, "M/d/y – M/d/y"));
            assert(equal(node.origin_locale, "en"));
            break;
        }
        assert(status == QJS_INTL_UNSUPPORTED);
        head = node.parent;
    }
    assert(head);
}

static void check_entries(const QJSIntlDateIntervalData *data)
{
    uint32_t i, j;
    for (i = 0; i < data->group_count; i++) {
        QJSIntlDateIntervalGroup group;
        uint32_t found;
        assert(!qjs_intl_date_interval_group_at(data, i, &group));
        assert(!qjs_intl_date_interval_find_group(data, &group, &found) && found == i);
    }
    /* Check the locale/calendar-to-shared-block mapping without expanding
       the implicit cube. Source layer selectors are checked below. */
    for (i = 0; i < data->section.count; i++) {
        for (j = 0; j < data->calendar_count; j++) {
            uint32_t suffix = (i + j) % data->suffix_count;
            uint32_t head, row = qjs_intl_read_u32(data->locale_rows +
                                                 ((size_t)i * data->calendar_count + j) * 4);
            assert(!qjs_intl_date_interval_head(data, i, j * data->suffix_count + suffix, &head));
            assert(head == qjs_intl_read_u32(data->rows + ((size_t)row * data->suffix_count + suffix) * 4));
        }
    }
    /* Exercise every raw selector in the prepared source layers, including
       difference/range codes and container/item/leaf alternatives. */
    for (i = 1; i <= data->node_count; i++) {
        QJSIntlDateIntervalNode node;
        uint32_t start, count;
        assert(!qjs_intl_date_interval_node(data, i, &node));
        start = qjs_intl_read_u32(data->maps + (size_t)node.map * 8);
        count = qjs_intl_read_u32(data->maps + (size_t)node.map * 8 + 4);
        for (j = 0; j < count; j++) {
            const uint8_t *field = data->entries + ((size_t)start + j) * 12;
            QJSIntlBytes code, alt, text;
            QJSIntlDateIntervalEntry entry;
            uint32_t value = qjs_intl_read_u32(field + 8);
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field), &code));
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field + 4), &alt));
            assert(!qjs_intl_date_interval_lookup(data, i, code, alt, &entry));
            assert(entry.code.length == code.length && entry.alt.length == alt.length);
            assert(!code.length || !memcmp(entry.code.data, code.data, code.length));
            assert(!alt.length || !memcmp(entry.alt.data, alt.data, alt.length));
            if (value == UINT32_C(0xfffffffe)) {
                assert(entry.blocked);
            } else {
                assert(!qjs_intl_data_string(&data->data, value, &text));
                assert(!entry.blocked && entry.text.length == text.length);
                assert(!text.length || !memcmp(entry.text.data, text.data, text.length));
            }
        }
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlDateIntervalData owner, result, zero = {0};
    size_t calls, successful, i;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_date_interval_data_open(&data, &allocator, &owner) && !state.live);
    successful = state.calls;
    assert(successful);
    calls = state.calls;
    check_english(&owner);
    check_entries(&owner);
    assert(state.calls == calls && !state.live);
    for (i = 1; i <= successful; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_interval_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.calls = state.fail_at = 0;
        assert(!qjs_intl_date_interval_data_open(&data, &allocator, &result) && !state.live);
    }
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_date_interval_data_open(&data, &allocator, &result) && !state.live);
    check_english(&result);
    free(unaligned);
    return 0;
}
