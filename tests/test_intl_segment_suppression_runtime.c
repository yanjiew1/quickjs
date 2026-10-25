/* Literal matching, protected decisions, tail barriers and atomic failure.
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
#include "../src/intl/segment/suppression.h"
#include "../src/intl/segment/suppression-data.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t qjs_intl_suppression_runtime_bytes[];
extern const size_t qjs_intl_suppression_runtime_length;

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;

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

static void prepare(const QJSIntlAllocator *allocator, const QJSIntlSegmentProgram *program,
                    const uint16_t *units, size_t length, QJSIntlSegmentText *text,
                    QJSIntlSegmentScan *scan)
{
    QJSIntlUTF16 input = {units, length};
    assert(!qjs_intl_segment_text_prepare(allocator, &input, text));
    assert(!qjs_intl_segment_scan_prepare(allocator, program, text, scan));
}

static void check_case(const QJSIntlAllocator *allocator, const QJSIntlSegmentProgram *program,
                       const QJSIntlSegmentLiteralGraph *list, const uint16_t *units, size_t length,
                       const size_t *removed, size_t removed_count)
{
    Allocation *state = allocator->opaque;
    QJSIntlSegmentText text;
    QJSIntlSegmentScan scan;
    size_t i, j, live;
    prepare(allocator, program, units, length, &text, &scan);
    live = state->live;
    assert(!qjs_intl_segment_suppression_apply(allocator, program, list, &text, &scan));
    assert(state->live == live);
    for (i = 0; i <= text.count; i++) {
        int expected = 1;
        for (j = 0; j < removed_count; j++)
            if (removed[j] == i)
                expected = 0;
        assert(scan.breaks[i] == expected);
    }
    /* Applying the policy again cannot recreate a removed boundary. */
    assert(!qjs_intl_segment_suppression_apply(allocator, program, list, &text, &scan));
    assert(text.offsets[text.count] == length);
    qjs_intl_segment_scan_clear(&scan);
    qjs_intl_segment_text_clear(&text);
    assert(!state->live);
}

static void check_ascii(const QJSIntlAllocator *allocator, const QJSIntlSegmentProgram *program,
                        const QJSIntlSegmentLiteralGraph *list, const char *source,
                        const size_t *removed, size_t count)
{
    uint16_t units[64];
    size_t i, length = strlen(source);
    assert(length < sizeof(units) / sizeof(units[0]));
    for (i = 0; i < length; i++)
        units[i] = (uint8_t)source[i];
    check_case(allocator, program, list, units, length, removed, count);
}

static void check_failure(const QJSIntlAllocator *allocator, const QJSIntlSegmentProgram *program,
                          const QJSIntlSegmentLiteralGraph *list)
{
    Allocation *state = allocator->opaque;
    QJSIntlSegmentText text;
    QJSIntlSegmentScan scan;
    const uint16_t units[] = {'M', 'r', '.', ' ', 'X'};
    uint8_t original[6];
    size_t live;
    prepare(allocator, program, units, 5, &text, &scan);
    memcpy(original, scan.breaks, sizeof(original));
    live = state->live;
    state->fail_at = state->calls + 1;
    assert(qjs_intl_segment_suppression_apply(allocator, program, list, &text, &scan) == QJS_INTL_NO_MEMORY);
    assert(state->live == live && !memcmp(original, scan.breaks, sizeof(original)));
    state->fail_at = 0;
    /* A later invalid rule must not publish removals found earlier in the scan. */
    scan.rules[4] = QJS_INTL_SEGMENT_DEFAULT_RULE - 1;
    assert(qjs_intl_segment_suppression_apply(allocator, program, list, &text, &scan) == QJS_INTL_INVALID_ARGUMENT);
    assert(state->live == live && !memcmp(original, scan.breaks, sizeof(original)));
    scan.rules[4] = 1;
    scan.rules[1] = QJS_INTL_SEGMENT_DEFAULT_RULE;
    scan.breaks[2] = 0;
    assert(!qjs_intl_segment_suppression_apply(allocator, program, list, &text, &scan));
    assert(scan.breaks[1] && !scan.breaks[2] && !scan.breaks[3] && !scan.breaks[4]);
    qjs_intl_segment_scan_clear(&scan);
    qjs_intl_segment_text_clear(&text);
}

static void check_long_overlap(const QJSIntlAllocator *allocator, const QJSIntlSegmentProgram *program,
                               const QJSIntlSegmentLiteralGraph *list)
{
    QJSIntlSegmentText text;
    QJSIntlSegmentScan scan;
    const size_t prefix = 4097, length = prefix + 3;
    uint16_t *units = malloc(length * sizeof(*units));
    size_t i;
    assert(units && list->max_scalars == 256);
    for (i = 0; i < prefix; i++)
        units[i] = 'a';
    units[prefix] = '.';
    units[prefix + 1] = ' ';
    units[prefix + 2] = 'X';
    prepare(allocator, program, units, length, &text, &scan);
    assert(!qjs_intl_segment_suppression_apply(allocator, program, list, &text, &scan));
    for (i = 0; i <= length; i++)
        assert(scan.breaks[i] == !(i > prefix - 255 && i < length));
    qjs_intl_segment_scan_clear(&scan);
    qjs_intl_segment_text_clear(&text);
    free(units);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData shared;
    QJSIntlSegmentProgramData programs;
    QJSIntlSegmentSuppressionData lists;
    QJSIntlSegmentProgram program;
    QJSIntlSegmentLiteralGraph list, empty = {0};
    const size_t four[] = {1, 2, 3, 4}, eight[] = {1, 2, 3, 4, 5, 6, 7, 8};
    const size_t protected[] = {1, 3, 4, 5}, three[] = {1, 2, 3};
    const size_t shifted[] = {2, 3, 4, 5};
    const size_t eleven[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    const uint16_t tail[] = {'M', 'r', '.', ')', ' ', 0x308, 'X'};
    const uint16_t extended[] = {'M', 'r', '.', 0x308, ')', 0x308, ')', ' ', 0x308, ' ', 0x308, 'X'};
    const uint16_t close_barrier[] = {'M', 'r', '.', ' ', ')', 0x308, 'X'};
    const uint16_t text_barrier[] = {'M', 'r', '.', 'Z', 0x308, 'X'};
    const uint16_t paragraph[] = {'M', 'r', '.', '\n', 0x308, 'X'};
    const uint16_t inside[] = {'M', 0x308, 'r', '.', ' ', 'X'};
    const uint16_t supplementary[] = {0xd840, 0xdc00, '.', ' ', 'X'};
    const uint16_t lone[] = {0xd800, 'M', 'r', '.', ' ', 0xdc00};
    const uint16_t nul[] = {'M', 0, 'r', '.', ' ', 'X'};
    assert(!qjs_intl_data_open(qjs_intl_suppression_runtime_bytes,
                              qjs_intl_suppression_runtime_length, &allocator, &shared));
    assert(!qjs_intl_segment_program_data_open(&shared, &programs));
    assert(!qjs_intl_segment_suppression_data_open(&shared, &allocator, &lists));
    assert(!qjs_intl_segment_program_at(&programs, 0, &program));
    assert(!qjs_intl_segment_suppression_at(&lists, 0, &list));
    assert(!state.live);
    check_ascii(&allocator, &program, &list, "Mr. Smith", four, 4);
    check_ascii(&allocator, &program, &list, "MR. Smith", NULL, 0);
    check_ascii(&allocator, &program, &list, "n. Chr. X", eight, 8);
    check_ascii(&allocator, &program, &list, "n. Crap X", NULL, 0);
    check_ascii(&allocator, &program, &list, "A.B. X", eight, 5);
    check_ascii(&allocator, &program, &list, "Mr. )X", four, 4);
    check_ascii(&allocator, &program, &list, "x#y. X", protected, 4);
    check_ascii(&allocator, &program, &list, "a\nb. X", NULL, 0);
    check_ascii(&allocator, &program, &empty, "Mr. Smith", NULL, 0);
    check_ascii(&allocator, &program, &list, "", NULL, 0);
    check_ascii(&allocator, &program, &list, "M", NULL, 0);
    check_case(&allocator, &program, &list, tail, 7, eight, 6);
    check_case(&allocator, &program, &list, extended, 12, eleven, 11);
    check_case(&allocator, &program, &list, close_barrier, 7, four, 4);
    check_case(&allocator, &program, &list, text_barrier, 6, three, 3);
    check_case(&allocator, &program, &list, paragraph, 6, three, 3);
    check_case(&allocator, &program, &list, inside, 6, NULL, 0);
    check_case(&allocator, &program, &list, supplementary, 5, three, 3);
    check_case(&allocator, &program, &list, lone, 6, shifted, 4);
    check_case(&allocator, &program, &list, nul, 6, NULL, 0);
    check_failure(&allocator, &program, &list);
    check_long_overlap(&allocator, &program, &list);
    assert(!state.live);
    return 0;
}
