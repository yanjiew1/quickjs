/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/date-pattern.h"
#include <assert.h>
#include <string.h>

static QJSIntlBytes text(const char *s)
{
    QJSIntlBytes result = {s, strlen(s)};
    return result;
}
static QJSIntlStatus range_piece(void *opaque, QJSIntlBytes literal, unsigned int argument)
{
    unsigned int *counts = opaque;
    (void)literal;
    assert(argument <= 2);
    counts[argument]++;
    return QJS_INTL_OK;
}
int main(void)
{
    int fields[QJS_DATE_FIELD_COUNT];
    unsigned int family, counts[3] = {0};
    assert(qjs_intl_date_pattern_fields(text("yyyy-MM-dd 'at' HH:mm"), fields,
                                       &family) == QJS_INTL_OK);
    assert(fields[QJS_DATE_YEAR] == QJS_DATE_NUMERIC);
    assert(fields[QJS_DATE_MONTH] == QJS_DATE_TWO_DIGIT);
    assert(fields[QJS_DATE_DAY] == QJS_DATE_TWO_DIGIT);
    assert(fields[QJS_DATE_HOUR] == QJS_DATE_TWO_DIGIT && family == 2);
    assert(qjs_intl_date_pattern_fields(text("h:mm 'o''clock' a"), fields,
                                       &family) == QJS_INTL_OK && family == 1);
    assert(qjs_intl_date_pattern_fields(text("yyyy 'unterminated"), fields,
                                       &family) == QJS_INTL_DATA_ERROR);
    assert(qjs_intl_date_pattern_fields(text("yyyy \xed\xa0\x80"), fields,
                                       &family) == QJS_INTL_DATA_ERROR);
    assert(qjs_intl_date_pattern_fields(text("QQQ"), fields,
                                       &family) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_date_range_template_visit(text("{1} - {0}"), range_piece,
                                             counts) == QJS_INTL_OK);
    assert(counts[0] == 1 && counts[1] == 1);
    memset(counts, 0, sizeof(counts));
    assert(qjs_intl_date_range_template_visit(text("{0} - {0}"), range_piece,
                                             counts) == QJS_INTL_DATA_ERROR);
    assert(!counts[0] && !counts[1] && !counts[2]);
    return 0;
}
