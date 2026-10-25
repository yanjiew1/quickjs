/* Localized numeric bodies: parts, grouping, UTF-16 and bounded allocation.
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
#include "../src/intl/number/body.h"
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
    return (QJSIntlBytes){text, strlen(text)};
}

static QJSIntlNumberBody layout(void)
{
    static const uint16_t decimal[] = {'.'}, group[] = {','};
    static const uint16_t nan[] = {'N', 'a', 'N'}, infinity[] = {0x221e};
    QJSIntlNumberBody result = {0};
    unsigned int i;
    for (i = 0; i < 10; i++)
        result.digits[i] = '0' + i;
    result.primary_grouping = result.secondary_grouping = 3;
    result.minimum_grouping_digits = 1;
    result.grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    result.decimal = (QJSIntlUTF16){decimal, 1};
    result.group = (QJSIntlUTF16){group, 1};
    result.nan = (QJSIntlUTF16){nan, 3};
    result.infinity = (QJSIntlUTF16){infinity, 1};
    result.maximum_output_length = 1000;
    return result;
}

static void partitioned(const QJSIntlFormatted *result)
{
    size_t i, at = 0;
    assert(result->text && result->length && result->parts && result->part_count);
    for (i = 0; i < result->part_count; i++) {
        assert(result->parts[i].start == at);
        at = result->parts[i].end;
        assert(at > result->parts[i].start && at <= result->length);
    }
    assert(at == result->length);
}

static QJSIntlFormatted format(const QJSIntlAllocator *allocator, const QJSIntlNumberBody *body,
                               const char *input)
{
    QJSIntlFormatted result;
    assert(!qjs_intl_number_body_format(allocator, body, QJS_INTL_FINITE, bytes(input), &result));
    partitioned(&result);
    return result;
}

static void expect_ascii(const QJSIntlAllocator *allocator, const QJSIntlNumberBody *body,
                         const char *input, const char *expected)
{
    QJSIntlFormatted result = format(allocator, body, input);
    size_t i;
    assert(result.length == strlen(expected));
    for (i = 0; i < result.length; i++)
        assert(result.text[i] == (unsigned char)expected[i]);
    qjs_intl_number_body_clear(allocator, &result);
    assert(!result.text && !result.parts && !result.length && !result.part_count);
    qjs_intl_number_body_clear(allocator, &result);
}

static void rejected(const QJSIntlAllocator *allocator, const QJSIntlNumberBody *body,
                     QJSIntlMathematicalKind kind, QJSIntlBytes input, QJSIntlStatus expected)
{
    QJSIntlFormatted result, zero = {0};
    Allocation *state = allocator->opaque;
    size_t calls = state->calls;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_number_body_format(allocator, body, kind, input, &result) == expected);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(!state->live && calls == state->calls);
}

static void test_grouping(const QJSIntlAllocator *allocator)
{
    QJSIntlNumberBody body = layout();
    QJSIntlFormatted result = format(allocator, &body, "1234567.0800");
    static const QJSIntlPartType types[] = {
        QJS_INTL_PART_INTEGER, QJS_INTL_PART_GROUP,   QJS_INTL_PART_INTEGER, QJS_INTL_PART_GROUP,
        QJS_INTL_PART_INTEGER, QJS_INTL_PART_DECIMAL, QJS_INTL_PART_FRACTION};
    static const size_t ends[] = {1, 2, 5, 6, 9, 10, 14};
    unsigned int i;
    assert(result.part_count == sizeof(types) / sizeof(types[0]));
    for (i = 0; i < result.part_count; i++) {
        assert(result.parts[i].type == types[i]);
        assert(result.parts[i].end == ends[i]);
    }
    qjs_intl_number_body_clear(allocator, &result);
    expect_ascii(allocator, &body, "0", "0");
    expect_ascii(allocator, &body, "123", "123");
    expect_ascii(allocator, &body, "1234", "1,234");
    expect_ascii(allocator, &body, "1234567.0800", "1,234,567.0800");
    expect_ascii(allocator, &body, "000001.00", "000,001.00");
    body.minimum_grouping_digits = 2;
    expect_ascii(allocator, &body, "1234", "1234");
    expect_ascii(allocator, &body, "12345", "12,345");
    body.grouping = QJS_INTL_NUMBER_GROUP_ALWAYS;
    expect_ascii(allocator, &body, "1234", "1,234");
    body.grouping = QJS_INTL_NUMBER_GROUP_MIN2;
    body.minimum_grouping_digits = 1;
    expect_ascii(allocator, &body, "1234", "1234");
    expect_ascii(allocator, &body, "1234567", "1,234,567");
    body.secondary_grouping = 2;
    expect_ascii(allocator, &body, "1234567", "12,34,567");
    expect_ascii(allocator, &body, "123456", "1,23,456");
    expect_ascii(allocator, &body, "12345", "12,345");
    body.grouping = QJS_INTL_NUMBER_GROUP_OFF;
    expect_ascii(allocator, &body, "1234567.0800", "1234567.0800");
    body.grouping = QJS_INTL_NUMBER_GROUP_ALWAYS;
    body.primary_grouping = body.secondary_grouping = 0;
    expect_ascii(allocator, &body, "1234567", "1234567");
}

static void test_unicode(const QJSIntlAllocator *allocator)
{
    QJSIntlNumberBody body = layout();
    QJSIntlFormatted result;
    static const uint16_t group[] = {0x200f, 0x066c}, decimal[] = {0x066b};
    static const uint16_t expected[] = {0x0661, 0x200f, 0x066c, 0x0662, 0x0663,
                                        0x0664, 0x066b, 0x0665, 0x0660};
    unsigned int i;
    for (i = 0; i < 10; i++)
        body.digits[i] = 0x0660 + i;
    body.group = (QJSIntlUTF16){group, 2};
    body.decimal = (QJSIntlUTF16){decimal, 1};
    result = format(allocator, &body, "1234.50");
    assert(result.length == sizeof(expected) / sizeof(expected[0]));
    assert(!memcmp(result.text, expected, sizeof(expected)));
    assert(result.parts[1].start == 1 && result.parts[1].end == 3);
    qjs_intl_number_body_clear(allocator, &result);
    for (i = 0; i < 10; i++)
        body.digits[i] = 0x1d7ce + i;
    result = format(allocator, &body, "1234.50");
    assert(result.length == 15);
    assert(result.text[0] == 0xd835 && result.text[1] == 0xdfcf);
    assert(result.parts[0].end == 2 && result.parts[1].end == 4);
    assert(result.parts[2].end == 10 && result.parts[3].end == 11);
    qjs_intl_number_body_clear(allocator, &result);
    body.maximum_output_length = 14;
    rejected(allocator, &body, QJS_INTL_FINITE, bytes("1234.50"), QJS_INTL_OVERFLOW);
    body.maximum_output_length = 15;
    result = format(allocator, &body, "1234.50");
    qjs_intl_number_body_clear(allocator, &result);
    /* Symbol slices are UTF-16 strings, not C strings or scalar-only data. */
    {
        static const uint16_t exact[] = {0, 0xd800, 0xdc00, 0xdfff};
        body = layout();
        body.decimal = (QJSIntlUTF16){exact, 4};
        result = format(allocator, &body, "1.2");
        assert(result.length == 6 && !memcmp(result.text + 1, exact, sizeof(exact)));
        assert(result.parts[1].start == 1 && result.parts[1].end == 5);
        qjs_intl_number_body_clear(allocator, &result);
    }
}

static void test_nonfinite(const QJSIntlAllocator *allocator)
{
    QJSIntlNumberBody body = layout();
    QJSIntlFormatted result;
    int kind;
    for (kind = QJS_INTL_NAN; kind <= QJS_INTL_NEGATIVE_INFINITY; kind++) {
        QJSIntlUTF16 expected = kind == QJS_INTL_NAN ? body.nan : body.infinity;
        assert(!qjs_intl_number_body_format(allocator, &body, kind, (QJSIntlBytes){0}, &result));
        partitioned(&result);
        assert(result.part_count == 1);
        assert(result.parts[0].type ==
               (kind == QJS_INTL_NAN ? QJS_INTL_PART_NAN : QJS_INTL_PART_INFINITY));
        assert(result.length == expected.length &&
               !memcmp(result.text, expected.data, expected.length * 2));
        qjs_intl_number_body_clear(allocator, &result);
        rejected(allocator, &body, kind, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    }
    assert(!qjs_intl_number_body_format(allocator, &body, QJS_INTL_NEGATIVE_ZERO, bytes("0.00"),
                                        &result));
    assert(result.length == 4 && result.parts[2].type == QJS_INTL_PART_FRACTION);
    qjs_intl_number_body_clear(allocator, &result);
}

static void test_errors(const QJSIntlAllocator *allocator)
{
    static const char *const invalid[] = {"",   ".",  ".5",  "1.",  "1.2.3", "-0",
                                          "+1", " 1", "1e3", "NaN", "0x10",  "1x"};
    QJSIntlNumberBody body = layout(), copy;
    QJSIntlFormatted result;
    QJSIntlAllocator bad = *allocator;
    Allocation *state = allocator->opaque;
    size_t i, before;
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
        rejected(allocator, &body, QJS_INTL_FINITE, bytes(invalid[i]), QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, &body, QJS_INTL_FINITE, (QJSIntlBytes){NULL, 1}, QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, &body, -1, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, NULL, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.grouping = -1;
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.primary_grouping = 10;
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.secondary_grouping = 0;
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.minimum_grouping_digits = 0;
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.minimum_grouping_digits = 10;
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.digits[0] = copy.digits[1];
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.digits[0] = 0xd800;
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.digits[0] = 0x110000;
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.maximum_output_length = 0;
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.group.data = NULL;
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1"), QJS_INTL_INVALID_ARGUMENT);
    copy = body;
    copy.decimal = (QJSIntlUTF16){0};
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1.2"), QJS_INTL_DATA_ERROR);
    expect_ascii(allocator, &copy, "1", "1");
    copy = body;
    copy.group = (QJSIntlUTF16){0};
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1234"), QJS_INTL_DATA_ERROR);
    expect_ascii(allocator, &copy, "123", "123");
    copy = body;
    copy.nan = (QJSIntlUTF16){0};
    rejected(allocator, &copy, QJS_INTL_NAN, (QJSIntlBytes){0}, QJS_INTL_DATA_ERROR);
    copy = body;
    copy.infinity = (QJSIntlUTF16){0};
    rejected(allocator, &copy, QJS_INTL_NEGATIVE_INFINITY, (QJSIntlBytes){0}, QJS_INTL_DATA_ERROR);
    copy = body;
    copy.maximum_output_length = 4;
    rejected(allocator, &copy, QJS_INTL_FINITE, bytes("1234"), QJS_INTL_OVERFLOW);
    copy.maximum_output_length = 5;
    expect_ascii(allocator, &copy, "1234", "1,234");
    assert(qjs_intl_number_body_format(NULL, &body, QJS_INTL_FINITE, bytes("1"), &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!result.text && !result.parts);
    bad.malloc = NULL;
    assert(qjs_intl_number_body_format(&bad, &body, QJS_INTL_FINITE, bytes("1"), &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_body_format(allocator, &body, QJS_INTL_FINITE, bytes("1"), NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    before = state->calls;
    for (i = 1; i <= 2; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_body_format(allocator, &body, QJS_INTL_FINITE, bytes("1234.50"),
                                           &result) == QJS_INTL_NO_MEMORY);
        assert(!state->live && !result.text && !result.parts && !result.length &&
               !result.part_count);
    }
    assert(state->calls == before + 3);
    state->fail_at = 0;
    expect_ascii(allocator, &body, "1234.50", "1,234.50");
    qjs_intl_number_body_clear(allocator, NULL);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    test_grouping(&allocator);
    test_unicode(&allocator);
    test_nonfinite(&allocator);
    test_errors(&allocator);
    assert(!state.live);
    return 0;
}
