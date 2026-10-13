/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATE_PATTERN_H
#define QJS_INTL_DATE_PATTERN_H
#include "provider.h"

enum {
    QJS_DATE_WEEKDAY, QJS_DATE_ERA, QJS_DATE_YEAR, QJS_DATE_MONTH,
    QJS_DATE_DAY, QJS_DATE_DAY_PERIOD, QJS_DATE_HOUR, QJS_DATE_MINUTE,
    QJS_DATE_SECOND, QJS_DATE_FRACTION, QJS_DATE_ZONE, QJS_DATE_FIELD_COUNT
};
enum {
    QJS_DATE_TWO_DIGIT, QJS_DATE_NUMERIC, QJS_DATE_NARROW,
    QJS_DATE_SHORT, QJS_DATE_LONG
};
enum {
    QJS_DATE_ZONE_SHORT, QJS_DATE_ZONE_LONG, QJS_DATE_ZONE_SHORT_OFFSET,
    QJS_DATE_ZONE_LONG_OFFSET, QJS_DATE_ZONE_SHORT_GENERIC,
    QJS_DATE_ZONE_LONG_GENERIC
};
/* literal.data points into the supplied UTF8 pattern. Quotes are consumed;
 * doubled apostrophes yield one literal apostrophe. A field has empty literal.
 * No allocations, ICU, locale data, or formatter implementation dependency.
 */
typedef QJSIntlStatus (*QJSIntlDatePatternVisitor)(void *, QJSIntlBytes literal,
                                                 unsigned int symbol,
                                                 unsigned int count);
QJSIntlStatus qjs_intl_date_pattern_visit(QJSIntlBytes,
    QJSIntlDatePatternVisitor, void *);
QJSIntlStatus qjs_intl_date_pattern_fields(QJSIntlBytes, int *, unsigned int *);
/* Decoded literal range template: {0} start, {1} end, each exactly once,
 * either order. Other braces are invalid. Apostrophes are literal here;
 * CLDR extraction must decode or reject MessageFormat quoting beforehand.
 * argument 2 marks a literal; 0/1 marks the corresponding endpoint.
 * Validation completes before visitors run. No allocation or formatter link.
 */
typedef QJSIntlStatus (*QJSIntlDateRangeTemplateVisitor)(void *, QJSIntlBytes,
                                                       unsigned int argument);
QJSIntlStatus qjs_intl_date_range_template_visit(QJSIntlBytes,
    QJSIntlDateRangeTemplateVisitor, void *);
/* Family:0 no hour,1 twelve-hour (h/K),2 twenty-four-hour (H/k).
 * fields use -1 absent; fraction1..3; other widths use the enums above.
 * Related Gregorian year r and cyclic year-name U share the public YEAR
 * component; r(U) is valid and emits two part types. U width1..5 and r1..20
 * preserve exact pattern grammar. The subset excludes week/quarter/ordinal.
 * Unsupported field letters return UNSUPPORTED, malformed UTF8 or
 * quotation DATA_ERROR. This distinction does not confer data capabilities.
 */
int qjs_intl_date_basic_score(const int *requested, const int *candidate);
int qjs_intl_date_scalar(uint32_t);
static inline QJSIntlStatus qjs_intl_date_utf8_next(QJSIntlBytes s, size_t *at, uint32_t *out)
{
    const unsigned char *p = (const unsigned char *)s.data;
    uint32_t cp, minimum;
    size_t i;
    unsigned int n, j;
    if (!at || !out || (!s.data && s.length) || *at >= s.length)
        return QJS_INTL_INVALID_ARGUMENT;
    i = *at;
    cp = p[i++];
    if (cp < 0x80) { *at = i; *out = cp; return QJS_INTL_OK; }
    if (cp >= 0xc2 && cp <= 0xdf) { n = 1; minimum = 0x80; cp &= 31; }
    else if (cp >= 0xe0 && cp <= 0xef) { n = 2; minimum = 0x800; cp &= 15; }
    else if (cp >= 0xf0 && cp <= 0xf4) { n = 3; minimum = 0x10000; cp &= 7; }
    else return QJS_INTL_DATA_ERROR;
    if (n > s.length - i) return QJS_INTL_DATA_ERROR;
    for (j = 0; j < n; j++) {
        unsigned int ch = p[i++];
        if ((ch & 0xc0) != 0x80) return QJS_INTL_DATA_ERROR;
        cp = (cp << 6) | (ch & 63);
    }
    if (cp < minimum || !qjs_intl_date_scalar(cp)) return QJS_INTL_DATA_ERROR;
    *at = i;
    *out = cp;
    return QJS_INTL_OK;
}
#endif
