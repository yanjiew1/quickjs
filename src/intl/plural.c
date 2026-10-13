/*
 * QuickJS native Intl services and tests
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#include "plural.h"
#include <string.h>

/* A digit slice represents an arbitrarily large nonnegative integer. */
typedef struct PluralValue {
    const char *digits, *fraction;
    size_t length, fraction_length;
    uint64_t small;
    int is_small, negative;
} PluralValue;

typedef struct PluralDecimal {
    const char *integer, *fraction;
    size_t integer_length, fraction_length, trimmed_fraction;
    int32_t exponent;
} PluralDecimal;

typedef struct PluralParser {
    const char *at, *end;
    const PluralDecimal *decimal;
    int error;
} PluralParser;

static int plural_decimal(const JSIntlPluralOperands *number,
                          PluralDecimal *decimal)
{
    const char *point;
    size_t integer, fraction;
    if (!number || !number->decimal || !number->length) return -1;
    point = memchr(number->decimal, '.', number->length);
    integer = point ? (size_t)(point - number->decimal) : number->length;
    fraction = point ? number->length - integer - 1 : 0;
    if (!integer || (point && !fraction)) return -1;
    for (size_t i = 0; i < number->length; i++) {
        if (point && i == integer) continue;
        if (number->decimal[i] < '0' || number->decimal[i] > '9') return -1;
    }
    decimal->integer = number->decimal;
    decimal->integer_length = integer;
    decimal->fraction = point ? point + 1 : number->decimal + number->length;
    decimal->fraction_length = fraction;
    while (fraction && decimal->fraction[fraction - 1] == '0') fraction--;
    decimal->trimmed_fraction = fraction;
    decimal->exponent = number->exponent;
    return 0;
}

static PluralValue plural_operand(const PluralDecimal *number, char operand)
{
    PluralValue value = {0};
    switch (operand) {
    case 'n':
        value.fraction = number->fraction;
        value.fraction_length = number->trimmed_fraction;
        /* fall through */
    case 'i':
        value.digits = number->integer;
        value.length = number->integer_length;
        break;
    case 'f': case 't':
        value.digits = number->fraction;
        value.length = operand == 'f' ? number->fraction_length :
                                       number->trimmed_fraction;
        break;
    case 'v': case 'w':
        value.is_small = 1;
        value.small = operand == 'v' ? number->fraction_length :
                                      number->trimmed_fraction;
        break;
    case 'c': case 'e':
        value.is_small = 1;
        value.negative = number->exponent < 0;
        value.small = value.negative ? -(int64_t)number->exponent : number->exponent;
        break;
    }
    return value;
}

static void plural_modulo(PluralValue *value, uint32_t modulus)
{
    uint64_t remainder = 0;
    if (value->is_small) {
        remainder = value->small % modulus;
    } else {
        for (size_t i = 0; i < value->length; i++)
            remainder = (remainder * 10 + value->digits[i] - '0') % modulus;
    }
    value->is_small = 1;
    value->small = remainder;
    if (!remainder) value->negative = 0;
}

static int plural_compare(const PluralValue *value, uint32_t constant)
{
    char digits[10];
    size_t count = 0, first = 0;
    int relation = 0;
    if (value->negative && value->small) return -1;
    if (value->is_small) {
        relation = value->small < constant ? -1 : value->small > constant;
    } else {
        uint32_t n = constant;
        do { digits[count++] = '0' + n % 10; n /= 10; } while (n);
        while (first < value->length && value->digits[first] == '0') first++;
        if (first == value->length) {
            relation = constant ? -1 : 0;
        } else if (value->length - first != count) {
            relation = value->length - first < count ? -1 : 1;
        } else {
            for (size_t i = 0; i < count; i++) {
                char left = value->digits[first + i], right = digits[count - i - 1];
                if (left != right) { relation = left < right ? -1 : 1; break; }
            }
        }
    }
    /* Fraction is nonzero exactly when the trimmed slice is nonempty. */
    if (!relation && value->fraction_length) return 1;
    return relation;
}

static void plural_space(PluralParser *parser)
{
    while (parser->at < parser->end &&
           (*parser->at == ' ' || *parser->at == '\t' ||
            *parser->at == '\r' || *parser->at == '\n')) parser->at++;
}

static int plural_word(PluralParser *parser, const char *word)
{
    size_t length = strlen(word);
    plural_space(parser);
    if ((size_t)(parser->end - parser->at) < length ||
        memcmp(parser->at, word, length)) return 0;
    if (parser->at + length < parser->end &&
        ((parser->at[length] >= 'a' && parser->at[length] <= 'z') ||
         (parser->at[length] >= 'A' && parser->at[length] <= 'Z') ||
         (parser->at[length] >= '0' && parser->at[length] <= '9') ||
         parser->at[length] == '_')) return 0;
    parser->at += length;
    return 1;
}

static int plural_uint(PluralParser *parser, uint32_t *value)
{
    uint32_t result = 0;
    int seen = 0;
    plural_space(parser);
    while (parser->at < parser->end &&
           *parser->at >= '0' && *parser->at <= '9') {
        unsigned int digit = *parser->at++ - '0';
        if (result > (UINT32_MAX - digit) / 10) {
            parser->error = 1;
            return -1;
        }
        result = result * 10 + digit;
        seen = 1;
    }
    if (!seen) { parser->error = 1; return -1; }
    *value = result;
    return 0;
}

static int plural_relation(PluralParser *parser)
{
    PluralValue value;
    uint32_t modulus, lower, upper;
    int negate = 0, integer_only = 1, single = 0, result = 0, has_mod;
    char operand;
    plural_space(parser);
    if (parser->at == parser->end ||
        (!*parser->at || !strchr("nivwftce", *parser->at))) goto invalid;
    operand = *parser->at++;
    value = plural_operand(parser->decimal, operand);
    plural_space(parser);
    if (parser->at < parser->end && *parser->at == '%') {
        parser->at++;
        has_mod = 1;
    } else {
        has_mod = plural_word(parser, "mod");
    }
    if (has_mod) {
        if (plural_uint(parser, &modulus) < 0 || !modulus) goto invalid;
        plural_modulo(&value, modulus);
    }
    plural_space(parser);
    if (parser->at < parser->end && *parser->at == '=') {
        parser->at++;
    } else if ((size_t)(parser->end - parser->at) >= 2 &&
               parser->at[0] == '!' && parser->at[1] == '=') {
        parser->at += 2;
        negate = 1;
    } else if (plural_word(parser, "is")) {
        negate = plural_word(parser, "not");
        integer_only = 0;
        single = 1;
    } else {
        negate = plural_word(parser, "not");
        if (plural_word(parser, "within")) integer_only = 0;
        else if (!plural_word(parser, "in")) goto invalid;
    }
    do {
        if (plural_uint(parser, &lower) < 0) goto invalid;
        upper = lower;
        plural_space(parser);
        if (!single && (size_t)(parser->end - parser->at) >= 2 &&
            parser->at[0] == '.' && parser->at[1] == '.') {
            parser->at += 2;
            if (plural_uint(parser, &upper) < 0 || lower > upper) goto invalid;
        }
        if (plural_compare(&value, lower) >= 0 &&
            plural_compare(&value, upper) <= 0) result = 1;
        plural_space(parser);
        if (single || parser->at == parser->end || *parser->at != ',') break;
        parser->at++;
    } while (1);
    if (integer_only && value.fraction_length) result = 0;
    return negate ? !result : result;
 invalid:
    parser->error = 1;
    return 0;
}

static int plural_conjunction(PluralParser *parser)
{
    int result = plural_relation(parser);
    while (!parser->error && plural_word(parser, "and")) {
        int next = plural_relation(parser);
        result = result && next;
    }
    return result;
}

int js_intl_plural_rule_evaluate(const char *rule, size_t rule_length,
                               const JSIntlPluralOperands *number, int *matches)
{
    PluralDecimal decimal;
    PluralParser parser;
    const char *samples;
    int result;
    if (!rule || !matches || plural_decimal(number, &decimal) < 0) return -1;
    /* Sample lists are metadata; Unicode ellipses never enter the parser. */
    samples = memchr(rule, '@', rule_length);
    parser.at = rule;
    parser.end = samples ? samples : rule + rule_length;
    parser.decimal = &decimal;
    parser.error = 0;
    plural_space(&parser);
    if (parser.at == parser.end) { *matches = 1; return 0; }
    result = plural_conjunction(&parser);
    while (!parser.error && plural_word(&parser, "or")) {
        int next = plural_conjunction(&parser);
        result = result || next;
    }
    plural_space(&parser);
    if (parser.error || parser.at != parser.end) return -1;
    *matches = result;
    return 0;
}
