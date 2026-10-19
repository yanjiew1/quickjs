/* Typed affix composition: literals, signs, body parts, UTF-8 and failures.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/number/affix.h"
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

static QJSIntlNumberPatternToken token(QJSIntlNumberTokenKind kind, const char *text)
{
    return (QJSIntlNumberPatternToken){kind, bytes(text)};
}

static QJSIntlFormatted body(void)
{
    static uint16_t text[] = {'1', ',', '2', '3', '4', '.', '5', '0'};
    static QJSIntlPart parts[] = {{0, 1, QJS_INTL_PART_INTEGER},
                                  {1, 2, QJS_INTL_PART_GROUP},
                                  {2, 5, QJS_INTL_PART_INTEGER},
                                  {5, 6, QJS_INTL_PART_DECIMAL},
                                  {6, 8, QJS_INTL_PART_FRACTION}};
    return (QJSIntlFormatted){text, 8, parts, 5};
}

static QJSIntlNumberAffixSymbols symbols(void)
{
    return (QJSIntlNumberAffixSymbols){bytes("-"), bytes("+"), bytes("%"), bytes("‰"),
                                       bytes("USD"), {0}, {0}};
}

static QJSIntlFormatted format(const QJSIntlAllocator *allocator,
                               const QJSIntlNumberPatternToken *tokens, size_t count,
                               const QJSIntlNumberAffixSymbols *signs, unsigned int plus,
                               const char *expected)
{
    QJSIntlFormatted number = body(), result;
    size_t i, at = 0;
    assert(!qjs_intl_number_affix_format(allocator, tokens, count, signs, &number, plus, 100,
                                         &result));
    if (expected) {
        assert(result.length == strlen(expected));
        for (i = 0; i < result.length; i++)
            assert(result.text[i] == (unsigned char)expected[i]);
    }
    for (i = 0; i < result.part_count; i++) {
        assert(result.parts[i].start == at);
        at = result.parts[i].end;
        assert(at <= result.length);
    }
    assert(at == result.length);
    return result;
}

static void test_forms(const QJSIntlAllocator *allocator)
{
    QJSIntlNumberAffixSymbols signs = symbols();
    QJSIntlNumberPatternToken tokens[5];
    QJSIntlFormatted result;
    unsigned int i;
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_MINUS, "");
    tokens[1] = token(QJS_INTL_NUMBER_TOKEN_NUMBER, "");
    tokens[2] = token(QJS_INTL_NUMBER_TOKEN_PERCENT, "");
    result = format(allocator, tokens, 3, &signs, 0, "-1,234.50%");
    assert(result.part_count == 7 && result.parts[0].type == QJS_INTL_PART_MINUS_SIGN);
    assert(result.parts[1].start == 1 && result.parts[5].end == 9);
    assert(result.parts[6].type == QJS_INTL_PART_PERCENT_SIGN);
    qjs_intl_number_body_clear(allocator, &result);
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_NUMBER, "");
    tokens[1] = token(QJS_INTL_NUMBER_TOKEN_PLUS, "");
    result = format(allocator, tokens, 2, &signs, 1, "1,234.50+");
    assert(result.part_count == 6 && result.parts[5].type == QJS_INTL_PART_PLUS_SIGN);
    qjs_intl_number_body_clear(allocator, &result);
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_LITERAL, "(");
    tokens[1] = token(QJS_INTL_NUMBER_TOKEN_CURRENCY, "");
    tokens[2] = token(QJS_INTL_NUMBER_TOKEN_LITERAL, " ");
    tokens[3] = token(QJS_INTL_NUMBER_TOKEN_NUMBER, "");
    tokens[4] = token(QJS_INTL_NUMBER_TOKEN_LITERAL, ")");
    result = format(allocator, tokens, 5, &signs, 0, "(USD 1,234.50)");
    assert(result.part_count == 9 && result.parts[1].type == QJS_INTL_PART_CURRENCY);
    qjs_intl_number_body_clear(allocator, &result);
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_CURRENCY, "");
    tokens[1] = token(QJS_INTL_NUMBER_TOKEN_NUMBER, "");
    for (i = QJS_INTL_NUMBER_TOKEN_CURRENCY; i <= QJS_INTL_NUMBER_TOKEN_CURRENCY_NARROW; i++) {
        tokens[0].kind = i;
        result = format(allocator, tokens, 2, &signs, 1, "+USD1,234.50");
        assert(result.parts[0].type == QJS_INTL_PART_PLUS_SIGN);
        assert(result.parts[1].type == QJS_INTL_PART_CURRENCY);
        qjs_intl_number_body_clear(allocator, &result);
    }
    /* Exact empty replacement still contributes its specified part record. */
    signs.currency = (QJSIntlBytes){0};
    result = format(allocator, tokens, 2, &signs, 0, "1,234.50");
    assert(result.parts[0].type == QJS_INTL_PART_CURRENCY);
    assert(!result.parts[0].start && !result.parts[0].end && result.part_count == 6);
    qjs_intl_number_body_clear(allocator, &result);
    signs.currency = bytes("\xf0\x9f\x92\xb0");
    result = format(allocator, tokens, 2, &signs, 0, NULL);
    assert(result.length == 10 && result.text[0] == 0xd83d && result.text[1] == 0xdcb0);
    assert(result.parts[0].end == 2 && result.parts[1].start == 2);
    qjs_intl_number_body_clear(allocator, &result);
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_MINUS, "");
    signs.minus = bytes("\xe2\x80\x8f\xe2\x88\x92");
    result = format(allocator, tokens, 2, &signs, 0, NULL);
    assert(result.length == 10 && result.text[0] == 0x200f && result.text[1] == 0x2212);
    assert(result.parts[0].end == 2);
    qjs_intl_number_body_clear(allocator, &result);
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_LITERAL, "'quoted' {0}");
    result = format(allocator, tokens, 2, &signs, 0, "'quoted' {0}1,234.50");
    assert(result.parts[0].type == QJS_INTL_PART_LITERAL);
    qjs_intl_number_body_clear(allocator, &result);
}

static void test_spacing(const QJSIntlAllocator *allocator)
{
    QJSIntlNumberAffixSymbols signs = symbols();
    QJSIntlNumberPatternToken tokens[] = {token(QJS_INTL_NUMBER_TOKEN_CURRENCY, ""),
                                          token(QJS_INTL_NUMBER_TOKEN_NUMBER, "")};
    QJSIntlFormatted result;
    signs.before_currency = bytes("_");
    signs.after_currency = bytes("~");
    result = format(allocator, tokens, 2, &signs, 0, "USD~1,234.50");
    assert(result.parts[1].type == QJS_INTL_PART_LITERAL && result.parts[1].start == 3);
    qjs_intl_number_body_clear(allocator, &result);
    tokens[0].kind = QJS_INTL_NUMBER_TOKEN_NUMBER;
    tokens[1].kind = QJS_INTL_NUMBER_TOKEN_CURRENCY_ISO;
    result = format(allocator, tokens, 2, &signs, 0, "1,234.50_USD");
    assert(result.parts[5].type == QJS_INTL_PART_LITERAL && result.parts[5].start == 8);
    qjs_intl_number_body_clear(allocator, &result);
    signs.currency = (QJSIntlBytes){0};
    result = format(allocator, tokens, 2, &signs, 0, "1,234.50");
    qjs_intl_number_body_clear(allocator, &result);
}

static void rejected(const QJSIntlAllocator *allocator, const QJSIntlNumberPatternToken *tokens,
                     size_t count, const QJSIntlNumberAffixSymbols *signs,
                     const QJSIntlFormatted *number, unsigned int plus, size_t limit,
                     QJSIntlStatus expected)
{
    QJSIntlFormatted result, zero = {0};
    Allocation *state = allocator->opaque;
    size_t calls = state->calls;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_number_affix_format(allocator, tokens, count, signs, number, plus, limit,
                                        &result) == expected);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state->live && state->calls == calls);
}

static void test_errors(const QJSIntlAllocator *allocator)
{
    QJSIntlNumberPatternToken tokens[] = {token(QJS_INTL_NUMBER_TOKEN_LITERAL, "USD "),
                                          token(QJS_INTL_NUMBER_TOKEN_NUMBER, "")};
    QJSIntlNumberAffixSymbols signs = symbols();
    QJSIntlFormatted number = body(), bad, result;
    QJSIntlPart parts[5];
    Allocation *state = allocator->opaque;
    size_t i;
    static const char *const invalid[] = {"\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80",
                                          "\xe2\x80", "\x80"};
    rejected(allocator, NULL, 1, &signs, &number, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, tokens, 0, &signs, &number, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, tokens, 2, NULL, &number, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, tokens, 2, &signs, NULL, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, tokens, 2, &signs, &number, 2, 100, QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, tokens, 2, &signs, &number, 0, 0, QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, tokens, 2, &signs, &number, 0, 11, QJS_INTL_OVERFLOW);
    bad = number;
    bad.part_count--;
    rejected(allocator, tokens, 2, &signs, &bad, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    memcpy(parts, number.parts, sizeof(parts));
    bad = number;
    bad.parts = parts;
    parts[1].start++;
    rejected(allocator, tokens, 2, &signs, &bad, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    parts[1] = number.parts[1];
    parts[1].end = number.length + 1;
    rejected(allocator, tokens, 2, &signs, &bad, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    parts[1] = number.parts[1];
    parts[1].type = -1;
    rejected(allocator, tokens, 2, &signs, &bad, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    parts[1].type = QJS_INTL_PART_ELEMENT;
    rejected(allocator, tokens, 2, &signs, &bad, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_NUMBER, "");
    rejected(allocator, tokens, 2, &signs, &number, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_LITERAL, "prefix");
    tokens[1].kind = QJS_INTL_NUMBER_TOKEN_MINUS;
    rejected(allocator, tokens, 2, &signs, &number, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    tokens[1].kind = QJS_INTL_NUMBER_TOKEN_NUMBER;
    tokens[0].kind = -1;
    rejected(allocator, tokens, 2, &signs, &number, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_MINUS, "bad");
    rejected(allocator, tokens, 2, &signs, &number, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_LITERAL, "");
    tokens[0].text = (QJSIntlBytes){NULL, 1};
    rejected(allocator, tokens, 2, &signs, &number, 0, 100, QJS_INTL_INVALID_ARGUMENT);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        tokens[0].text = bytes(invalid[i]);
        rejected(allocator, tokens, 2, &signs, &number, 0, 100, QJS_INTL_DATA_ERROR);
    }
    tokens[0].text = (QJSIntlBytes){"\0", 1};
    rejected(allocator, tokens, 2, &signs, &number, 0, 100, QJS_INTL_DATA_ERROR);
    tokens[0] = token(QJS_INTL_NUMBER_TOKEN_LITERAL, "USD ");
    for (i = 1; i <= 2; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_affix_format(allocator, tokens, 2, &signs, &number, 0, 100,
                                            &result) == QJS_INTL_NO_MEMORY);
        assert(!state->live && !result.text && !result.parts && !result.length &&
               !result.part_count);
    }
    state->fail_at = 0;
    result = format(allocator, tokens, 2, &signs, 0, "USD 1,234.50");
    assert(result.length == 12);
    qjs_intl_number_body_clear(allocator, &result);
    assert(qjs_intl_number_affix_format(NULL, tokens, 2, &signs, &number, 0, 100, &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_affix_format(allocator, tokens, 2, &signs, &number, 0, 100, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
}

static void test_signs(void)
{
    static const struct {
        QJSIntlMathematicalKind kind;
        int negative;
        const char *digits;
        unsigned int forms[5];
    } cases[] = {
        {QJS_INTL_FINITE, 0, "1", {0, 0, 2, 2, 0}},
        {QJS_INTL_FINITE, 1, "1", {1, 0, 1, 1, 1}},
        {QJS_INTL_FINITE, 0, "0", {0, 0, 2, 0, 0}},
        {QJS_INTL_NEGATIVE_ZERO, 0, "", {1, 0, 1, 0, 0}},
        {QJS_INTL_NAN, 0, "", {0, 0, 2, 0, 0}},
        {QJS_INTL_POSITIVE_INFINITY, 0, "", {0, 0, 2, 2, 0}},
        {QJS_INTL_NEGATIVE_INFINITY, 0, "", {1, 0, 1, 1, 1}},
    };
    QJSIntlRoundedNumber number = {0};
    size_t i;
    unsigned int mode, form;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        number.kind = cases[i].kind;
        number.number.rounded = (QJSIntlDecimal){(char *)cases[i].digits, strlen(cases[i].digits),
                                                 0, cases[i].negative};
        for (mode = 0; mode < 5; mode++) {
            assert(!qjs_intl_number_sign_pattern(mode, &number, &form));
            assert(form == cases[i].forms[mode]);
        }
    }
    assert(qjs_intl_number_sign_pattern(-1, &number, &form) == QJS_INTL_INVALID_ARGUMENT && !form);
    assert(qjs_intl_number_sign_pattern(0, NULL, &form) == QJS_INTL_INVALID_ARGUMENT && !form);
    assert(qjs_intl_number_sign_pattern(0, &number, NULL) == QJS_INTL_INVALID_ARGUMENT);
    number.kind = -1;
    assert(qjs_intl_number_sign_pattern(0, &number, &form) == QJS_INTL_INVALID_ARGUMENT && !form);
    number.kind = QJS_INTL_FINITE;
    number.number.rounded = (QJSIntlDecimal){0};
    assert(qjs_intl_number_sign_pattern(0, &number, &form) == QJS_INTL_INVALID_ARGUMENT && !form);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    test_signs();
    test_forms(&allocator);
    test_spacing(&allocator);
    test_errors(&allocator);
    assert(!state.live);
    return 0;
}
