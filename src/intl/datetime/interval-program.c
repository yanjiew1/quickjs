/* Compile and partition intervals without inventing locale punctuation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "interval-program.h"
#include <string.h>

/* r/U can coexist in one year representation; unlike M/L or h/H they must
   not make that first representation look like a repeated endpoint field. */
static unsigned int field_bit(unsigned int symbol)
{
    switch (symbol) {
    case 'G': return 1U << 0;
    case 'y': return 1U << 1;
    case 'r': return 1U << 2;
    case 'U': return 1U << 3;
    case 'M': case 'L': return 1U << 4;
    case 'd': return 1U << 5;
    case 'E': case 'e': case 'c': return 1U << 6;
    case 'a': return 1U << 7;
    case 'b': case 'B': return 1U << 8;
    case 'h': case 'H': case 'K': case 'k': return 1U << 9;
    case 'm': return 1U << 10;
    case 's': return 1U << 11;
    case 'S': return 1U << 12;
    case 'z': case 'v': case 'O': return 1U << 13;
    default: return 0;
    }
}

static void adapt(QJSIntlDatePatternToken *token, const QJSIntlDatePatternInfo *ordinary,
                   QJSIntlDateHourCycle cycle)
{
    int field, current, desired;
    switch (token->symbol) {
    case 'M': case 'L':
        if (token->width < 3)
            return;
        field = QJS_INTL_DATETIME_MONTH;
        break;
    case 'G': field = QJS_INTL_DATETIME_ERA; break;
    case 'E': case 'e': case 'c': field = QJS_INTL_DATETIME_WEEKDAY; break;
    case 'b': case 'B': field = QJS_INTL_DATETIME_DAY_PERIOD; break;
    case 'h': case 'H': case 'K': case 'k':
        token->symbol = cycle <= QJS_INTL_DATE_H12 ? 'h' : 'H';
        desired = ordinary->components.values[QJS_INTL_DATETIME_HOUR];
        if (desired)
            token->width = desired == QJS_INTL_DATETIME_TWO_DIGIT ? 2 : 1;
        return;
    default:
        return;
    }
    desired = ordinary->components.values[field];
    current = token->width == 5 ? QJS_INTL_DATETIME_NARROW :
              token->width == 4 ? QJS_INTL_DATETIME_LONG : QJS_INTL_DATETIME_SHORT;
    if (desired >= QJS_INTL_DATETIME_NARROW && desired != current)
        token->width = desired == QJS_INTL_DATETIME_NARROW ? 5 :
                       desired == QJS_INTL_DATETIME_LONG ? 4 : 3;
}

static QJSIntlStatus halves(const QJSIntlDateIntervalProgram *program, QJSIntlDatePatternInfo info[2])
{
    QJSIntlDatePattern view = {0};
    QJSIntlStatus status;
    view.tokens = program->pattern.tokens;
    view.count = program->split;
    status = qjs_intl_date_pattern_info(&view, &info[0]);
    if (!status) {
        view.tokens += program->split;
        view.count = program->pattern.count - program->split;
        status = qjs_intl_date_pattern_info(&view, &info[1]);
    }
    return status;
}

void qjs_intl_date_interval_program_clear(QJSIntlDateIntervalProgram *program)
{
    if (!program)
        return;
    qjs_intl_date_pattern_clear(&program->allocator, &program->pattern);
    if (program->sources)
        program->allocator.free(program->allocator.opaque, program->sources);
    memset(program, 0, sizeof(*program));
}

QJSIntlStatus qjs_intl_date_interval_program_open(const QJSIntlAllocator *allocator,
                                                 QJSIntlBytes source,
                                                 const QJSIntlDatePatternInfo *ordinary,
                                                 QJSIntlDateHourCycle cycle,
                                                 QJSIntlDateIntervalProgram *out)
{
    QJSIntlDateIntervalProgram result = {0};
    QJSIntlDatePatternInfo info[2];
    QJSIntlStatus status;
    unsigned int seen[2] = {0}, hour = 0, automatic = 0, period = 0;
    uint8_t first, last, left, right;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !ordinary || !allocator || !allocator->malloc || !allocator->free ||
        (unsigned int)cycle > QJS_INTL_DATE_H24)
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = *allocator;
    status = qjs_intl_date_interval_compile(allocator, source, &result.pattern);
    if (status)
        return status;
    result.split = result.pattern.count;
    for (i = 0; i < result.pattern.count; i++) {
        unsigned int symbol = result.pattern.tokens[i].symbol, bit;
        if (!symbol)
            continue;
        bit = field_bit(symbol);
        if (!bit) {
            status = QJS_INTL_UNSUPPORTED;
            goto fail;
        }
        if ((seen[0] & bit) && result.split == result.pattern.count)
            result.split = i;
        seen[i >= result.split] |= bit;
        hour |= (bit & (1U << 9)) != 0;
        automatic |= symbol == 'a';
        period |= symbol == 'a' || symbol == 'b' || symbol == 'B';
    }
    if (result.split == result.pattern.count || !result.split ||
        (cycle > QJS_INTL_DATE_H12 && automatic) ||
        (cycle <= QJS_INTL_DATE_H12 && hour && !period)) {
        status = QJS_INTL_UNSUPPORTED;
        goto fail;
    }
    /* Validate before adaptation so an invalid source width cannot be made
       acceptable merely by replacing it with the requested width. */
    status = halves(&result, info);
    if (status)
        goto fail;
    for (i = 0; i < result.pattern.count; i++)
        adapt(&result.pattern.tokens[i], ordinary, cycle);
    status = halves(&result, info);
    if (status)
        goto fail;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        int a = info[0].components.values[i], b = info[1].components.values[i];
        if (a && b && a != b) {
            status = QJS_INTL_UNSUPPORTED;
            goto fail;
        }
        result.components.values[i] = a ? a : b;
    }
    result.sources = allocator->malloc(allocator->opaque, result.pattern.count * sizeof(*result.sources));
    if (!result.sources) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    first = result.pattern.order == QJS_INTL_DATE_INTERVAL_LATEST_FIRST ? QJS_INTL_RANGE_END : QJS_INTL_RANGE_START;
    last = first == QJS_INTL_RANGE_START ? QJS_INTL_RANGE_END : QJS_INTL_RANGE_START;
    for (i = 0; i < result.pattern.count; i++) {
        unsigned int bit = field_bit(result.pattern.tokens[i].symbol);
        result.sources[i] = !bit || !(seen[0] & seen[1] & bit) ? QJS_INTL_RANGE_SHARED :
                            i < result.split ? first : last;
    }
    /* No-neighbour is distinct from a real shared field. This preserves
       endpoint-local prefixes/suffixes as well as shared inter-endpoint text. */
    right = UINT8_MAX;
    for (i = result.pattern.count; i-- > 0;) {
        if (result.pattern.tokens[i].symbol)
            right = result.sources[i];
        else
            result.sources[i] = right;
    }
    left = UINT8_MAX;
    for (i = 0; i < result.pattern.count; i++) {
        if (result.pattern.tokens[i].symbol) {
            left = result.sources[i];
        } else {
            right = result.sources[i];
            result.sources[i] = left == UINT8_MAX ? (right == UINT8_MAX ? QJS_INTL_RANGE_SHARED : right) :
                                right == UINT8_MAX || left == right ? left : QJS_INTL_RANGE_SHARED;
        }
    }
    *out = result;
    return QJS_INTL_OK;
fail:
    qjs_intl_date_interval_program_clear(&result);
    return status;
}
