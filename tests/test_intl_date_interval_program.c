/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

/* Interval compilation keeps real field kinds, ordering and owned literals. */
#include "../src/intl/datetime/interval-program.h"
#include <assert.h>
#include <stdlib.h>

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *value;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    value = malloc(size);
    if (value)
        state->live++;
    return value;
}

static void release(void *opaque, void *value)
{
    Allocation *state = opaque;
    assert(value && state->live);
    state->live--;
    free(value);
}

static QJSIntlBytes bytes(const char *value) { return (QJSIntlBytes){value, strlen(value)}; }

static QJSIntlDatePatternInfo ordinary(const QJSIntlAllocator *allocator, const char *source)
{
    QJSIntlDatePattern pattern;
    QJSIntlDatePatternInfo info;
    assert(!qjs_intl_date_pattern_compile(allocator, bytes(source), &pattern));
    assert(!qjs_intl_date_pattern_info(&pattern, &info));
    qjs_intl_date_pattern_clear(allocator, &pattern);
    return info;
}

static void failed(const QJSIntlAllocator *allocator, const QJSIntlDatePatternInfo *info,
                    const char *source, QJSIntlDateHourCycle cycle, QJSIntlStatus expected)
{
    QJSIntlDateIntervalProgram program, zero = {0};
    memset(&program, 0xff, sizeof(program));
    assert(qjs_intl_date_interval_program_open(allocator, bytes(source), info, cycle, &program) == expected);
    assert(!memcmp(&program, &zero, sizeof(program)));
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlDatePatternInfo info = ordinary(&allocator, "MMMM d, y");
    QJSIntlDateIntervalProgram program;
    const uint8_t expected[] = {0, 0, 1, 0, 2, 0, 0};
    size_t calls, i;
    char source[] = "MMM d – d, y";
    state.calls = 0;
    assert(!qjs_intl_date_interval_program_open(&allocator, bytes(source), &info, QJS_INTL_DATE_H23, &program));
    calls = state.calls;
    assert(program.pattern.count == sizeof(expected) && program.split == 4);
    assert(!memcmp(program.sources, expected, sizeof(expected)));
    assert(program.pattern.tokens[0].symbol == 'M' && program.pattern.tokens[0].width == 4);
    assert(program.components.values[QJS_INTL_DATETIME_MONTH] == QJS_INTL_DATETIME_LONG);
    source[3] = '!';
    assert(program.pattern.tokens[1].literal.length == 1 && program.pattern.tokens[1].literal.data[0] == ' ');
    qjs_intl_date_interval_program_clear(&program);
    qjs_intl_date_interval_program_clear(&program);
    assert(!state.live);
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        failed(&allocator, &info, "MMM d – d, y", QJS_INTL_DATE_H23, QJS_INTL_NO_MEMORY);
        assert(!state.live);
    }
    state.calls = state.fail_at = 0;
    {
        const char *sources[] = {"Y MMM d – Y MMM d", "MMM d Y – d Y"};
        size_t j;

        /* Correct the complete format before splitting: one endpoint can
           omit the shared month but still needs calendar-year interpretation. */
        for (j = 0; j < sizeof(sources) / sizeof(*sources); j++) {
            assert(!qjs_intl_date_interval_program_open(&allocator, bytes(sources[j]),
                                                        &info, QJS_INTL_DATE_H23, &program));
            for (i = 0; i < program.pattern.count; i++)
                assert(program.pattern.tokens[i].symbol != 'Y');
            qjs_intl_date_interval_program_clear(&program);
        }
        failed(&allocator, &info, "Y w MMM d – Y w MMM d", QJS_INTL_DATE_H23, QJS_INTL_UNSUPPORTED);
    }
    assert(!qjs_intl_date_interval_program_open(&allocator, bytes("latestFirst:MMM d – d, y"),
                                                &info, QJS_INTL_DATE_H23, &program));
    assert(program.pattern.order == QJS_INTL_DATE_INTERVAL_LATEST_FIRST &&
           program.sources[2] == QJS_INTL_RANGE_END && program.sources[4] == QJS_INTL_RANGE_START);
    qjs_intl_date_interval_program_clear(&program);
    /* Quoted and unquoted adjacent literals coalesce, but endpoint-local
       affixes must still follow latestFirst rather than textual position. */
    info = ordinary(&allocator, "d");
    assert(!qjs_intl_date_interval_program_open(&allocator, bytes("latestFirst:'from 'd' to ' d' end'!"),
                                                &info, QJS_INTL_DATE_H23, &program));
    assert(program.pattern.count == 5 && program.split == 3);
    assert(program.sources[0] == QJS_INTL_RANGE_END &&
           program.sources[1] == QJS_INTL_RANGE_END &&
           program.sources[2] == QJS_INTL_RANGE_SHARED &&
           program.sources[3] == QJS_INTL_RANGE_START &&
           program.sources[4] == QJS_INTL_RANGE_START);
    assert(program.pattern.tokens[2].literal.length == 5 &&
           !memcmp(program.pattern.tokens[2].literal.data, " to  ", 5));
    assert(program.pattern.tokens[4].literal.length == 5 &&
           !memcmp(program.pattern.tokens[4].literal.data, " end!", 5));
    qjs_intl_date_interval_program_clear(&program);
    // Related and cyclic years coexist; U must not prematurely split the first half.
    info = ordinary(&allocator, "r MMM d U");
    assert(!qjs_intl_date_interval_program_open(&allocator, bytes("r U M d – r U M d"),
                                                &info, QJS_INTL_DATE_H23, &program));
    assert(program.split == 8 && program.sources[0] == QJS_INTL_RANGE_START &&
           program.sources[2] == QJS_INTL_RANGE_START && program.sources[8] == QJS_INTL_RANGE_END);
    qjs_intl_date_interval_program_clear(&program);
    // Range metadata may add era/date context absent from the ordinary format.
    info = ordinary(&allocator, "y");
    assert(!qjs_intl_date_interval_program_open(&allocator, bytes("MMM d y G – MMM d y G"),
                                                &info, QJS_INTL_DATE_H23, &program));
    assert(program.components.values[QJS_INTL_DATETIME_ERA] == QJS_INTL_DATETIME_SHORT &&
           program.components.values[QJS_INTL_DATETIME_MONTH] == QJS_INTL_DATETIME_SHORT);
    qjs_intl_date_interval_program_clear(&program);
    failed(&allocator, &info, "M/d/y – MMM/d/y", QJS_INTL_DATE_H23, QJS_INTL_UNSUPPORTED);
    failed(&allocator, &info, "MMM d, y", QJS_INTL_DATE_H23, QJS_INTL_UNSUPPORTED);
    failed(&allocator, &info, "QQQ – QQQ", QJS_INTL_DATE_H23, QJS_INTL_UNSUPPORTED);
    failed(&allocator, &info, "d – d – d", QJS_INTL_DATE_H23, QJS_INTL_UNSUPPORTED);
    failed(&allocator, &info, "MMM d' – d", QJS_INTL_DATE_H23, QJS_INTL_DATA_ERROR);
    info = ordinary(&allocator, "hh:mm a");
    assert(!qjs_intl_date_interval_program_open(&allocator, bytes("h:mm – h:mm a"),
                                                &info, QJS_INTL_DATE_H12, &program));
    assert(program.pattern.tokens[0].width == 2 && program.pattern.tokens[0].symbol == 'h' &&
           program.sources[program.pattern.count - 1] == QJS_INTL_RANGE_SHARED);
    qjs_intl_date_interval_program_clear(&program);
    failed(&allocator, &info, "h:mm – h:mm a", QJS_INTL_DATE_H23, QJS_INTL_UNSUPPORTED);
    failed(&allocator, &info, "H:mm – H:mm", QJS_INTL_DATE_H12, QJS_INTL_UNSUPPORTED);
    info = ordinary(&allocator, "H B");
    assert(!qjs_intl_date_interval_program_open(&allocator, bytes("h B – h B"),
                                                &info, QJS_INTL_DATE_H23, &program));
    assert(program.pattern.tokens[0].symbol == 'H');
    qjs_intl_date_interval_program_clear(&program);
    failed(&allocator, &info, "HHH B – HHH B", QJS_INTL_DATE_H23, QJS_INTL_UNSUPPORTED);
    assert(!state.live);
    return 0;
}
