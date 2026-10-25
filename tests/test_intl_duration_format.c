/* Duration ListFormatParts metadata, empty elements, exact UTF-16 and ownership.
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
#include "../src/intl/duration/format.h"
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

/* Keep authored text fixtures borrowed while giving the production composer
   the owned text buffer required by its consuming contract. */
static QJSIntlStatus join_fixture(const QJSIntlAllocator *allocator,
                                  const QJSIntlFormatted *list,
                                  const QJSIntlDurationComponent *components,
                                  size_t count, size_t limit,
                                  QJSIntlDurationFormatted *out)
{
    QJSIntlFormatted owned = *list;
    QJSIntlStatus status;

    if (!allocator)
        return qjs_intl_duration_join_parts(NULL, &owned, components, count, limit, out);
    owned.text = allocator->malloc(allocator->opaque,
                                    (list->length ? list->length : 1) * sizeof(*owned.text));
    if (!owned.text) {
        memset(out, 0, sizeof(*out));
        return QJS_INTL_NO_MEMORY;
    }
    if (list->length)
        memcpy(owned.text, list->text, list->length * sizeof(*owned.text));
    status = qjs_intl_duration_join_parts(allocator, &owned, components, count, limit, out);
    if (owned.text)
        allocator->free(allocator->opaque, owned.text);
    return status;
}

static void check_parts(const QJSIntlDurationFormatted *result)
{
    static const QJSIntlDurationPart expected[] = {
        {0, 1, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_DAY},
        {1, 2, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_DAY},
        {2, 3, QJS_INTL_PART_UNIT, QJS_TEMPORAL_DAY},
        {3, 5, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_UNIT_COUNT},
        {5, 6, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_HOUR},
        {6, 7, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_UNIT_COUNT},
        {7, 9, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_MINUTE},
    };
    size_t i;
    assert(result->length == 9 && result->part_count == 7);
    for (i = 0; i < 7; i++) {
        assert(result->parts[i].start == expected[i].start && result->parts[i].end == expected[i].end);
        assert(result->parts[i].type == expected[i].type && result->parts[i].unit == expected[i].unit);
    }
}

static void test_composition(const QJSIntlAllocator *allocator, Allocation *allocation)
{
    uint16_t text[] = {'1', ' ', 'd', ',', ' ', '2', ':', '0', '3'};
    QJSIntlPart list_parts[] = {{0, 3, QJS_INTL_PART_ELEMENT}, {3, 5, QJS_INTL_PART_LITERAL},
                               {5, 9, QJS_INTL_PART_ELEMENT}};
    QJSIntlFormatted list = {text, 9, list_parts, 3};
    QJSIntlDurationPart day[] = {{0, 1, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_DAY},
                                {1, 2, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_DAY},
                                {2, 3, QJS_INTL_PART_UNIT, QJS_TEMPORAL_DAY}};
    QJSIntlDurationPart time[] = {{0, 1, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_HOUR},
                                 {1, 2, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_UNIT_COUNT},
                                 {2, 4, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_MINUTE}};
    QJSIntlDurationComponent components[] = {{{text, 3}, day, 3}, {{text + 5, 4}, time, 3}};
    QJSIntlDurationFormatted result, zero = {0};
    size_t step;

    assert(!join_fixture(allocator, &list, components, 2, 9, &result));
    assert(!memcmp(result.text, text, sizeof(text)));
    check_parts(&result);
    text[0] = '9';
    day[0].unit = QJS_TEMPORAL_YEAR;
    assert(result.text[0] == '1');
    check_parts(&result);
    qjs_intl_duration_result_clear(allocator, &result);
    qjs_intl_duration_result_clear(allocator, &result);
    text[0] = '1';
    day[0].unit = QJS_TEMPORAL_DAY;
    for (step = 1; step <= 2; step++) {
        allocation->calls = 0;
        allocation->fail_at = step;
        memset(&result, 0xff, sizeof(result));
        assert(join_fixture(allocator, &list, components, 2, 9, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && !allocation->live);
        allocation->fail_at = 0;
        assert(!join_fixture(allocator, &list, components, 2, 9, &result));
        qjs_intl_duration_result_clear(allocator, &result);
    }
    assert(join_fixture(allocator, &list, components, 2, 8, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)) && !allocation->live);
    assert(join_fixture(allocator, &list, components, 1, 9, &result) == QJS_INTL_DATA_ERROR);
    list_parts[1].type = QJS_INTL_PART_INTEGER;
    assert(join_fixture(allocator, &list, components, 2, 9, &result) == QJS_INTL_DATA_ERROR);
    list_parts[1].type = QJS_INTL_PART_LITERAL;
    day[0].unit = QJS_TEMPORAL_UNIT_UNSET;
    assert(join_fixture(allocator, &list, components, 2, 9, &result) == QJS_INTL_DATA_ERROR);
    day[0].unit = QJS_TEMPORAL_DAY;
    day[1].start = 0;
    assert(join_fixture(allocator, &list, components, 2, 9, &result) == QJS_INTL_DATA_ERROR);
    day[1].start = 1;
    day[0].type = QJS_INTL_PART_ELEMENT;
    assert(join_fixture(allocator, &list, components, 2, 9, &result) == QJS_INTL_DATA_ERROR);
    day[0].type = QJS_INTL_PART_INTEGER;
    components[0].part_count = SIZE_MAX;
    assert(join_fixture(allocator, &list, components, 2, 9, &result) == QJS_INTL_OVERFLOW);
    components[0].part_count = 3;
    components[0].text.length = 2;
    assert(join_fixture(allocator, &list, components, 2, 9, &result) == QJS_INTL_DATA_ERROR);
    components[0].text.length = 3;
    {
        uint16_t other[] = {'9', ' ', 'd'};
        components[0].text.data = other;
        assert(join_fixture(allocator, &list, components, 2, 9, &result) == QJS_INTL_DATA_ERROR);
        components[0].text.data = text;
    }
    assert(join_fixture(allocator, &list, components, 2, 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(join_fixture(allocator, &list, components, 11, 9, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(join_fixture(NULL, &list, components, 2, 9, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!allocation->live);
}

static void test_digital_composition(const QJSIntlAllocator *allocator, Allocation *state)
{
    uint16_t text[] = {'1', '0', '2', '0', '3'};
    const uint16_t expected[] = {'1', ':', '0', '2', ':', '0', '3'};
    QJSIntlDurationPart parts[] = {
        {0, 1, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_HOUR},
        {0, 2, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_MINUTE},
        {0, 2, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_SECOND},
    };
    QJSIntlDurationComponent components[] = {
        {{text, 1}, &parts[0], 1}, {{text + 1, 2}, &parts[1], 1}, {{text + 3, 2}, &parts[2], 1},
    };
    QJSIntlBytes separators[] = {{":", 1}, {":", 1}};
    QJSIntlDurationFormatted result, zero = {0};
    size_t calls, i;

    state->calls = 0;
    assert(!qjs_intl_duration_join_digital(allocator, components, 3, separators, 7, &result));
    calls = state->calls;
    assert(result.length == 7 && !memcmp(result.text, expected, sizeof(expected)));
    assert(result.part_count == 5 && result.parts[0].unit == QJS_TEMPORAL_HOUR &&
           result.parts[2].unit == QJS_TEMPORAL_MINUTE && result.parts[4].unit == QJS_TEMPORAL_SECOND);
    assert(result.parts[1].type == QJS_INTL_PART_LITERAL &&
           result.parts[1].unit == QJS_TEMPORAL_UNIT_COUNT && result.parts[1].start == 1 && result.parts[1].end == 2);
    text[0] = '9';
    parts[0].unit = QJS_TEMPORAL_YEAR;
    assert(result.text[0] == '1' && result.parts[0].unit == QJS_TEMPORAL_HOUR);
    qjs_intl_duration_result_clear(allocator, &result);
    text[0] = '1';
    parts[0].unit = QJS_TEMPORAL_HOUR;

    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_duration_join_digital(allocator, components, 3, separators, 7, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && !state->live);
        state->fail_at = 0;
    }
    assert(qjs_intl_duration_join_digital(allocator, components, 3, separators, 6, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state->live);

    /* Empty separator parts still identify the boundary, without a unit field. */
    separators[0] = separators[1] = (QJSIntlBytes){NULL, 0};
    assert(!qjs_intl_duration_join_digital(allocator, components, 3, separators, 5, &result));
    assert(result.length == 5 && !memcmp(result.text, text, sizeof(text)) && result.part_count == 5);
    assert(result.parts[1].start == 1 && result.parts[1].end == 1 &&
           result.parts[3].start == 3 && result.parts[3].end == 3 &&
           result.parts[1].unit == QJS_TEMPORAL_UNIT_COUNT && result.parts[3].unit == QJS_TEMPORAL_UNIT_COUNT);
    qjs_intl_duration_result_clear(allocator, &result);

    components[0] = (QJSIntlDurationComponent){{NULL, 0}, NULL, 0};
    assert(!qjs_intl_duration_join_digital(allocator, components, 1, NULL, 1, &result));
    assert(result.text && !result.length && !result.parts && !result.part_count);
    qjs_intl_duration_result_clear(allocator, &result);
    components[0] = (QJSIntlDurationComponent){{text, 1}, &parts[0], SIZE_MAX};
    assert(qjs_intl_duration_join_digital(allocator, components, 1, NULL, 7, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state->live);
    components[0].part_count = 1;
    separators[0] = (QJSIntlBytes){"\xc0", 1};
    assert(qjs_intl_duration_join_digital(allocator, components, 2, separators, 7, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state->live);
}

static void test_equal_and_empty_elements(const QJSIntlAllocator *allocator)
{
    uint16_t text[] = {'x', '|', 'x'};
    QJSIntlPart parts[] = {{0, 1, QJS_INTL_PART_ELEMENT}, {1, 2, QJS_INTL_PART_LITERAL},
                          {2, 3, QJS_INTL_PART_ELEMENT}};
    QJSIntlFormatted list = {text, 3, parts, 3};
    QJSIntlDurationPart first = {0, 1, QJS_INTL_PART_UNIT, QJS_TEMPORAL_DAY};
    QJSIntlDurationPart last = {0, 1, QJS_INTL_PART_UNIT, QJS_TEMPORAL_HOUR};
    QJSIntlDurationComponent components[] = {{{text, 1}, &first, 1}, {{text + 2, 1}, &last, 1}};
    QJSIntlDurationFormatted result;
    assert(!join_fixture(allocator, &list, components, 2, 3, &result));
    assert(result.part_count == 3 && result.parts[0].unit == QJS_TEMPORAL_DAY &&
           result.parts[2].unit == QJS_TEMPORAL_HOUR);
    qjs_intl_duration_result_clear(allocator, &result);

    text[0] = '|';
    list.length = 1;
    parts[0].end = 0;
    parts[1].start = 0;
    parts[1].end = parts[2].start = parts[2].end = 1;
    components[0] = components[1] = (QJSIntlDurationComponent){{NULL, 0}, NULL, 0};
    assert(!join_fixture(allocator, &list, components, 2, 1, &result));
    assert(result.length == 1 && result.part_count == 1 && result.text[0] == '|');
    assert(result.parts[0].unit == QJS_TEMPORAL_UNIT_COUNT);
    qjs_intl_duration_result_clear(allocator, &result);
    {
        QJSIntlDurationPart empty = {0, 0, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_DAY};
        components[0].parts = &empty;
        components[0].part_count = 1;
        assert(!join_fixture(allocator, &list, components, 2, 1, &result));
        assert(result.part_count == 2 && result.parts[0].start == 0 && result.parts[0].end == 0);
        assert(result.parts[0].unit == QJS_TEMPORAL_DAY && result.parts[1].unit == QJS_TEMPORAL_UNIT_COUNT);
        qjs_intl_duration_result_clear(allocator, &result);
    }
    list = (QJSIntlFormatted){0};
    assert(!join_fixture(allocator, &list, NULL, 0, 1, &result));
    assert(!result.length && !result.part_count && result.text && !result.parts);
    qjs_intl_duration_result_clear(allocator, &result);
}

static void test_utf16(const QJSIntlAllocator *allocator)
{
    uint16_t text[] = {0xd800, 0, 0xd83d, 0xde00};
    QJSIntlPart part = {0, 4, QJS_INTL_PART_ELEMENT};
    QJSIntlFormatted list = {text, 4, &part, 1};
    QJSIntlDurationPart number = {0, 4, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_SECOND};
    QJSIntlDurationComponent component = {{text, 4}, &number, 1};
    QJSIntlDurationFormatted result;
    assert(!join_fixture(allocator, &list, &component, 1, 4, &result));
    assert(result.length == 4 && !memcmp(result.text, text, sizeof(text)));
    text[0] = 0;
    assert(result.text[0] == 0xd800);
    qjs_intl_duration_result_clear(allocator, &result);
}

static void test_text_transfer(const QJSIntlAllocator *allocator, Allocation *state)
{
    QJSIntlPart part = {0, 1, QJS_INTL_PART_ELEMENT};
    QJSIntlDurationPart number = {0, 1, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_HOUR};
    QJSIntlDurationFormatted result, zero = {0};
    QJSIntlFormatted list;
    QJSIntlDurationComponent component;
    uint16_t *text = allocator->malloc(allocator->opaque, sizeof(*text));

    assert(text);
    text[0] = '7';
    list = (QJSIntlFormatted){text, 1, &part, 1};
    component = (QJSIntlDurationComponent){{text, 1}, &number, 1};
    state->fail_at = state->calls + 1;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_duration_join_parts(allocator, &list, &component, 1, 1,
                                        &result) == QJS_INTL_NO_MEMORY);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(list.text == text && list.length == 1 && list.parts == &part && text[0] == '7');
    assert(state->live == 1);
    state->fail_at = 0;
    assert(!qjs_intl_duration_join_parts(allocator, &list, &component, 1, 1, &result));
    assert(result.text == text && !list.text && !list.length && list.parts == &part);
    assert(result.part_count == 1 && result.parts[0].unit == QJS_TEMPORAL_HOUR);
    qjs_intl_duration_result_clear(allocator, &result);
    assert(!state->live);

    list = (QJSIntlFormatted){0};
    assert(!qjs_intl_duration_join_parts(allocator, &list, NULL, 0, 1, &result));
    assert(!result.text && !result.parts && !result.length && !result.part_count);
    assert(!state->live);
}

int main(void)
{
    Allocation allocation = {0};
    QJSIntlAllocator allocator = {&allocation, allocate, NULL, release};
    test_text_transfer(&allocator, &allocation);
    test_composition(&allocator, &allocation);
    test_digital_composition(&allocator, &allocation);
    test_equal_and_empty_elements(&allocator);
    test_utf16(&allocator);
    assert(!allocation.live);
    return 0;
}
