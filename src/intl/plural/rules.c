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
#include "rules.h"
#include "../../cutils/cutils.h"
#include <string.h>

/* Keep ordinary digit slices small; only shifted integer operands carry
   the extra context for a second borrowed span and a virtual zero tail. */
typedef struct PluralValue {
    const char *digits;
    const struct PluralShift *shifted;
    size_t length, fraction_length;
    uint64_t small;
    int is_small, negative; /* is_small < 0 denotes a shifted digit slice. */
} PluralValue;

typedef struct PluralShift {
    const char *suffix;
    size_t length;
    uint32_t zeros;
} PluralShift;

typedef struct PluralDecimal {
    const char *integer, *fraction;
    size_t integer_length, fraction_length, trimmed_fraction;
    const PluralShift *shifted;
    int32_t exponent;
} PluralDecimal;

typedef struct PluralParser {
    const char *at, *end;
    const PluralDecimal *decimal;
    int error;
} PluralParser;

static force_inline int plural_decimal(const QJSIntlPluralOperands *number, PluralDecimal *decimal)
{
    const char *point;
    size_t integer, fraction;
    if (!number || !number->decimal || !number->length || number->length > PTRDIFF_MAX)
        return -1;
    point = memchr(number->decimal, '.', number->length);
    integer = point ? (size_t)(point - number->decimal) : number->length;
    fraction = point ? number->length - integer - 1 : 0;
    if (!integer || (point && !fraction))
        return -1;
    for (size_t i = 0; i < number->length; i++) {
        if (point && i == integer)
            continue;
        if (number->decimal[i] < '0' || number->decimal[i] > '9')
            return -1;
    }
    decimal->integer = number->decimal;
    decimal->integer_length = integer;
    decimal->fraction = point ? point + 1 : number->decimal + number->length;
    decimal->fraction_length = fraction;
    while (fraction && decimal->fraction[fraction - 1] == '0')
        fraction--;
    decimal->trimmed_fraction = fraction;
    decimal->shifted = NULL;
    decimal->exponent = number->exponent;
    return 0;
}

static force_inline PluralValue plural_operand(const PluralDecimal *number, char operand, int shifted)
{
    PluralValue value = {0};
    switch (operand) {
    case 'n':
        value.fraction_length = number->trimmed_fraction;
        /* fall through */
    case 'i':
        value.digits = number->integer;
        value.length = number->integer_length;
        if (shifted) {
            value.shifted = number->shifted;
            value.is_small = -1;
        }
        break;
    case 'f':
    case 't':
        value.digits = number->fraction;
        value.length = operand == 'f' ? number->fraction_length : number->trimmed_fraction;
        break;
    case 'v':
    case 'w':
        value.is_small = 1;
        value.small = operand == 'v' ? number->fraction_length : number->trimmed_fraction;
        break;
    case 'c':
    case 'e':
        value.is_small = 1;
        value.negative = number->exponent < 0;
        value.small = value.negative ? -(int64_t)number->exponent : number->exponent;
        break;
    }
    return value;
}

/* Keep the uncommon virtual-tail machinery out of ordinary relation code. */
static no_inline uint64_t shifted_modulo(const PluralShift *number, uint64_t remainder,
                                         uint32_t modulus)
{
    uint32_t zeros = number->zeros;
    for (size_t i = 0; i < number->length; i++)
        remainder = (remainder * 10 + number->suffix[i] - '0') % modulus;
    /* Modular products are below (UINT32_MAX - 1)^2. Never iterate over
       an exponent-sized tail, even for a zero coefficient. */
    if (zeros && remainder) {
        uint64_t power = 10 % modulus;
        do {
            if (zeros & 1)
                remainder = remainder * power % modulus;
            zeros >>= 1;
            if (zeros)
                power = power * power % modulus;
        } while (zeros);
    }
    return remainder;
}

static force_inline void plural_modulo(PluralValue *value, uint32_t modulus)
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
    if (!remainder)
        value->negative = 0;
}

static void plural_modulo_shifted(PluralValue *value, uint32_t modulus)
{
    int shifted = value->is_small < 0;
    if (shifted)
        value->is_small = 0;
    plural_modulo(value, modulus);
    if (shifted)
        value->small = shifted_modulo(value->shifted, value->small, modulus);
}

static no_inline int shifted_integer_compare(const PluralValue *value, uint32_t constant)
{
    const PluralShift *number = value->shifted;
    const char *first = value->digits, *suffix = number->suffix;
    size_t length = value->length, suffix_length = number->length;
    char digits[10];
    size_t count = 0;
    uint64_t significant;
    uint32_t n = constant;
    do {
        digits[count++] = '0' + n % 10;
        n /= 10;
    } while (n);
    while (length && *first == '0') {
        first++;
        length--;
    }
    if (!length) {
        while (suffix_length && *suffix == '0') {
            suffix++;
            suffix_length--;
        }
    }
    significant = (uint64_t)length + suffix_length;
    if (!significant)
        return constant ? -1 : 0;
    significant += number->zeros;
    if (significant != count)
        return significant < count ? -1 : 1;
    for (size_t i = 0; i < count; i++) {
        char left = i < length ? first[i]
                    : i - length < suffix_length ? suffix[i - length] : '0';
        char right = digits[count - i - 1];
        if (left != right)
            return left < right ? -1 : 1;
    }
    return 0;
}

static int plural_compare(const PluralValue *value, uint32_t constant)
{
    char digits[10];
    size_t count = 0, first = 0;
    int relation = 0;
    if (value->negative && value->small)
        return -1;
    if (value->is_small) {
        relation = value->small < constant ? -1 : value->small > constant;
    } else {
        uint32_t n = constant;
        do {
            digits[count++] = '0' + n % 10;
            n /= 10;
        } while (n);
        while (first < value->length && value->digits[first] == '0')
            first++;
        if (first == value->length) {
            relation = constant ? -1 : 0;
        } else if (value->length - first != count) {
            relation = value->length - first < count ? -1 : 1;
        } else {
            for (size_t i = 0; i < count; i++) {
                char left = value->digits[first + i], right = digits[count - i - 1];
                if (left != right) {
                    relation = left < right ? -1 : 1;
                    break;
                }
            }
        }
    }
    /* Fraction is nonzero exactly when the trimmed slice is nonempty. */
    if (!relation && value->fraction_length)
        return 1;
    return relation;
}

static int plural_compare_shifted(const PluralValue *value, uint32_t constant)
{
    int relation;
    if (value->is_small >= 0)
        return plural_compare(value, constant);
    relation = shifted_integer_compare(value, constant);
    return !relation && value->fraction_length ? 1 : relation;
}

static void plural_space(PluralParser *parser)
{
    /* LDML permits Pattern_White_Space between tokens, not locale-sensitive
       C whitespace or arbitrary Unicode separators such as NBSP. */
    while (parser->at < parser->end) {
        const unsigned char *p = (const unsigned char *)parser->at;
        size_t left = (size_t)(parser->end - parser->at);
        if (p[0] == ' ' || (p[0] >= 9 && p[0] <= 13))
            parser->at++;
        else if (left >= 2 && p[0] == 0xc2 && p[1] == 0x85)
            parser->at += 2;
        else if (left >= 3 && p[0] == 0xe2 && p[1] == 0x80 &&
                 (p[2] == 0x8e || p[2] == 0x8f || p[2] == 0xa8 || p[2] == 0xa9))
            parser->at += 3;
        else
            break;
    }
}

static int plural_word(PluralParser *parser, const char *word)
{
    size_t length = strlen(word);
    plural_space(parser);
    if ((size_t)(parser->end - parser->at) < length || memcmp(parser->at, word, length))
        return 0;
    parser->at += length;
    return 1;
}

static int plural_uint(PluralParser *parser, uint32_t *value)
{
    uint32_t result = 0;
    int seen = 0;
    plural_space(parser);
    while (parser->at < parser->end && *parser->at >= '0' && *parser->at <= '9') {
        unsigned int digit = *parser->at++ - '0';
        if (result > (UINT32_MAX - digit) / 10) {
            parser->error = 1;
            return -1;
        }
        result = result * 10 + digit;
        seen = 1;
    }
    if (!seen) {
        parser->error = 1;
        return -1;
    }
    *value = result;
    return 0;
}

/* Specialize the numeric operations, keeping one grammar implementation and
   no virtual-digit classification in the ordinary relation handler. */
static force_inline int plural_relation_impl(PluralParser *parser, int shifted)
{
    PluralValue value;
    uint32_t modulus, lower, upper;
    int negate = 0, integer_only = 1, single = 0, result = 0, has_mod;
    char operand;
    plural_space(parser);
    if (parser->at == parser->end || (!*parser->at || !strchr("nivwftce", *parser->at)))
        goto invalid;
    operand = *parser->at++;
    value = plural_operand(parser->decimal, operand, shifted);
    plural_space(parser);
    if (parser->at < parser->end && *parser->at == '%') {
        parser->at++;
        has_mod = 1;
    } else {
        has_mod = plural_word(parser, "mod");
    }
    if (has_mod) {
        if (plural_uint(parser, &modulus) < 0 || !modulus)
            goto invalid;
        if (shifted)
            plural_modulo_shifted(&value, modulus);
        else
            plural_modulo(&value, modulus);
    }
    plural_space(parser);
    if (parser->at < parser->end && *parser->at == '=') {
        parser->at++;
    } else if ((size_t)(parser->end - parser->at) >= 2 && parser->at[0] == '!' &&
               parser->at[1] == '=') {
        parser->at += 2;
        negate = 1;
    } else if (plural_word(parser, "is")) {
        negate = plural_word(parser, "not");
        integer_only = 0;
        single = 1;
    } else {
        negate = plural_word(parser, "not");
        if (plural_word(parser, "within"))
            integer_only = 0;
        else if (!plural_word(parser, "in"))
            goto invalid;
    }
    do {
        if (plural_uint(parser, &lower) < 0)
            goto invalid;
        upper = lower;
        plural_space(parser);
        if (!single && (size_t)(parser->end - parser->at) >= 2 && parser->at[0] == '.' &&
            parser->at[1] == '.') {
            parser->at += 2;
            if (plural_uint(parser, &upper) < 0 || lower > upper)
                goto invalid;
        }
        if ((shifted ? plural_compare_shifted(&value, lower) : plural_compare(&value, lower)) >= 0 &&
            (shifted ? plural_compare_shifted(&value, upper) : plural_compare(&value, upper)) <= 0)
            result = 1;
        plural_space(parser);
        if (single || parser->at == parser->end || *parser->at != ',')
            break;
        parser->at++;
    } while (1);
    if (integer_only && value.fraction_length)
        result = 0;
    return negate ? !result : result;
invalid:
    parser->error = 1;
    return 0;
}

static int plural_relation(PluralParser *parser)
{
    return plural_relation_impl(parser, 0);
}

static int plural_relation_shifted(PluralParser *parser)
{
    return plural_relation_impl(parser, 1);
}

static force_inline int plural_conjunction_impl(PluralParser *parser,
                                                int (*relation)(PluralParser *))
{
    int result = relation(parser);
    while (!parser->error && plural_word(parser, "and")) {
        int next = relation(parser);
        result = result && next;
    }
    return result;
}

static int plural_conjunction(PluralParser *parser)
{
    return plural_conjunction_impl(parser, plural_relation);
}

static int plural_conjunction_shifted(PluralParser *parser)
{
    return plural_conjunction_impl(parser, plural_relation_shifted);
}

static force_inline QJSIntlStatus plural_evaluate(QJSIntlBytes rule, const PluralDecimal *decimal,
                                                  int (*conjunction)(PluralParser *), int *matches)
{
    PluralParser parser;
    int result;
    parser.at = rule.data;
    parser.end = rule.data + rule.length;
    parser.decimal = decimal;
    parser.error = 0;
    plural_space(&parser);
    if (parser.at == parser.end) {
        *matches = 1;
        return QJS_INTL_OK;
    }
    result = conjunction(&parser);
    while (!parser.error && plural_word(&parser, "or")) {
        int next = conjunction(&parser);
        result = result || next;
    }
    plural_space(&parser);
    if (parser.error || parser.at != parser.end)
        return QJS_INTL_DATA_ERROR;
    *matches = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_plural_rule_evaluate(QJSIntlBytes rule, const QJSIntlPluralOperands *number,
                                            int *matches)
{
    PluralDecimal decimal;
    if (matches)
        *matches = 0;
    if (!rule.data || !matches || rule.length > PTRDIFF_MAX || plural_decimal(number, &decimal) < 0)
        return QJS_INTL_INVALID_ARGUMENT;
    return plural_evaluate(rule, &decimal, plural_conjunction, matches);
}

QJSIntlStatus qjs_intl_plural_rule_evaluate_shifted(QJSIntlBytes rule,
                                                    const QJSIntlPluralOperands *number,
                                                    uint32_t decimal_shift, int *matches)
{
    PluralDecimal decimal;
    PluralShift shift;
    if (!decimal_shift)
        return qjs_intl_plural_rule_evaluate(rule, number, matches);
    if (matches)
        *matches = 0;
    if (!rule.data || !matches || rule.length > PTRDIFF_MAX || plural_decimal(number, &decimal) < 0)
        return QJS_INTL_INVALID_ARGUMENT;
    shift.suffix = decimal.fraction;
    shift.length = decimal.fraction_length < decimal_shift ? decimal.fraction_length : decimal_shift;
    shift.zeros = decimal_shift - shift.length;
    decimal.fraction += shift.length;
    decimal.fraction_length -= shift.length;
    decimal.trimmed_fraction = decimal.trimmed_fraction > shift.length
                                  ? decimal.trimmed_fraction - shift.length : 0;
    decimal.shifted = &shift;
    return plural_evaluate(rule, &decimal, plural_conjunction_shifted, matches);
}
