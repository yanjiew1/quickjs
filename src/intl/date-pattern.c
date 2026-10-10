/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "date-pattern.h"
#include <limits.h>

int qjs_intl_date_scalar(uint32_t cp)
{
    return cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
}

QJSIntlStatus qjs_intl_date_utf8_next(QJSIntlBytes s, size_t *at, uint32_t *out)
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

static int letter(unsigned int ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static QJSIntlStatus field(unsigned int ch, unsigned int n, int *f, int *v)
{
    if (!n) return QJS_INTL_DATA_ERROR;
    switch (ch) {
    case 'G':
        if (n > 5) return QJS_INTL_DATA_ERROR;
        *f = QJS_DATE_ERA; *v = n == 5 ? 2 : n == 4 ? 4 : 3; break;
    case 'y':
        if (n > 20) return QJS_INTL_DATA_ERROR;
        *f = QJS_DATE_YEAR; *v = n == 2 ? 0 : 1; break;
    case 'r':
        if (n > 20) return QJS_INTL_DATA_ERROR;
        *f = QJS_DATE_YEAR; *v = QJS_DATE_NUMERIC; break;
    case 'U':
        if (n > 5) return QJS_INTL_DATA_ERROR;
        *f = QJS_DATE_YEAR; *v = QJS_DATE_NUMERIC; break;
    case 'M': case 'L':
        if (n > 5) return QJS_INTL_DATA_ERROR;
        *f = QJS_DATE_MONTH; *v = n == 5 ? 2 : n == 4 ? 4 : n == 3 ? 3 : n == 2 ? 0 : 1; break;
    case 'd':
        if (n > 3) return QJS_INTL_DATA_ERROR;
        if (n == 3) return QJS_INTL_UNSUPPORTED;
        *f = QJS_DATE_DAY; *v = n == 2 ? 0 : 1; break;
    case 'E': case 'e': case 'c':
        if (n > 6) return QJS_INTL_DATA_ERROR;
        if (ch != 'E' && n < 3) return QJS_INTL_UNSUPPORTED;
        *f = QJS_DATE_WEEKDAY; *v = n == 5 ? 2 : n == 4 ? 4 : 3; break;
    case 'a':
        if (n > 5) return QJS_INTL_DATA_ERROR;
        *f = -1; *v = -1; break;
    case 'B': case 'b':
        if (n > 5) return QJS_INTL_DATA_ERROR;
        *f = QJS_DATE_DAY_PERIOD; *v = n == 5 ? 2 : n == 4 ? 4 : 3; break;
    case 'h': case 'H': case 'K': case 'k': case 'm': case 's':
        if (n > 2) return QJS_INTL_DATA_ERROR;
        *f = ch == 'm' ? QJS_DATE_MINUTE : ch == 's' ? QJS_DATE_SECOND : QJS_DATE_HOUR;
        *v = n == 2 ? 0 : 1; break;
    case 'S':
        if (n > 3) return QJS_INTL_UNSUPPORTED;
        *f = QJS_DATE_FRACTION; *v = (int)n; break;
    case 'z':
        if (n > 4) return QJS_INTL_DATA_ERROR;
        *f = QJS_DATE_ZONE; *v = n == 4 ? 1 : 0; break;
    case 'v':
        if (n != 1 && n != 4) return QJS_INTL_DATA_ERROR;
        *f = QJS_DATE_ZONE; *v = n == 4 ? 5 : 4; break;
    case 'O':
        if (n != 1 && n != 4) return QJS_INTL_DATA_ERROR;
        *f = QJS_DATE_ZONE; *v = n == 4 ? 3 : 2; break;
    default: return QJS_INTL_UNSUPPORTED;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_pattern_visit(QJSIntlBytes pattern,
    QJSIntlDatePatternVisitor visit, void *opaque)
{
    size_t at = 0, start;
    uint32_t cp;
    unsigned int count;
    int quote = 0, f, v;
    QJSIntlStatus r;
    if ((!pattern.data && pattern.length) || !pattern.length)
        return QJS_INTL_INVALID_ARGUMENT;
    while (at < pattern.length) {
        QJSIntlBytes literal = { NULL, 0 };
        start = at;
        r = qjs_intl_date_utf8_next(pattern, &at, &cp);
        if (r) return r;
        if (!cp) return QJS_INTL_DATA_ERROR;
        if (cp == '\'') {
            if (at < pattern.length && pattern.data[at] == '\'') {
                literal.data = pattern.data + start;
                literal.length = 1;
                at++;
            } else { quote = !quote; continue; }
        } else if (!quote && letter(cp)) {
            count = 1;
            while (at < pattern.length && (unsigned char)pattern.data[at] == cp) {
                if (count == UINT_MAX) return QJS_INTL_OVERFLOW;
                at++; count++;
            }
            r = field(cp, count, &f, &v);
            if (r) return r;
            if (visit && (r = visit(opaque, literal, cp, count)) != QJS_INTL_OK)
                return r;
            continue;
        } else {
            literal.data = pattern.data + start;
            literal.length = at - start;
        }
        if (visit && (r = visit(opaque, literal, 0, 0)) != QJS_INTL_OK)
            return r;
    }
    return quote ? QJS_INTL_DATA_ERROR : QJS_INTL_OK;
}

typedef struct Fields {
    int *fields;
    unsigned int family;
    unsigned int year_symbols;
} Fields;
static QJSIntlStatus fields_visit(void *opaque, QJSIntlBytes literal,
                                 unsigned int ch, unsigned int n)
{
    Fields *state = opaque;
    int f, v;
    QJSIntlStatus r;
    (void)literal;
    if (!ch) return QJS_INTL_OK;
    r = field(ch, n, &f, &v);
    if (r) return r;
    if (f >= 0) {
        if (f == QJS_DATE_YEAR) {
            unsigned int bit = ch == 'y' ? 1u : ch == 'r' ? 2u : 4u;
            if ((state->year_symbols & bit) ||
                (state->year_symbols && (bit == 1 || (state->year_symbols & 1))))
                return QJS_INTL_DATA_ERROR;
            state->year_symbols |= bit;
        } else if (state->fields[f] >= 0) return QJS_INTL_DATA_ERROR;
        state->fields[f] = v;
    }
    if (ch == 'h' || ch == 'K') state->family = 1;
    if (ch == 'H' || ch == 'k') state->family = 2;
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_date_pattern_fields(QJSIntlBytes pattern, int *fields,
                                         unsigned int *family)
{
    Fields state;
    QJSIntlStatus r;
    unsigned int i;
    if (!fields || !family) return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < QJS_DATE_FIELD_COUNT; i++) fields[i] = -1;
    *family = 0;
    state.fields = fields; state.family = 0; state.year_symbols = 0;
    r = qjs_intl_date_pattern_visit(pattern, fields_visit, &state);
    if (!r) *family = state.family;
    return r;
}

int qjs_intl_date_basic_score(const int *requested, const int *candidate)
{
    int i, a, b, delta, score = 0;
    for (i = 0; i < QJS_DATE_FIELD_COUNT; i++) {
        a = requested[i]; b = candidate[i];
        if (a < 0 && b >= 0) score -= 20;
        else if (a >= 0 && b < 0) score -= 120;
        else if (a == b) continue;
        else if (i == QJS_DATE_ZONE) {
            if (a == 0 || a == 4) {
                if (b == 2) score -= 1;
                else if (b == 3) score -= 4;
                else if ((a == 0 && b == 1) || (a == 4 && b == 5)) score -= 3;
                else score -= 120;
            } else if (a == 2 && b == 3) score -= 3;
            else if (a == 1 || a == 5) {
                if (b == 3) score -= 1;
                else if (b == 2) score -= 9;
                else if ((a == 1 && b == 0) || (a == 5 && b == 4)) score -= 8;
                else score -= 120;
            } else if (a == 3 && b == 2) score -= 8;
            else score -= 120;
        } else {
            delta = b - a;
            if (delta > 2) delta = 2;
            if (delta < -2) delta = -2;
            if (delta == 2) score -= 6;
            else if (delta == 1) score -= 3;
            else if (delta == -1) score -= 6;
            else if (delta == -2) score -= 8;
        }
    }
    return score;
}

QJSIntlStatus qjs_intl_date_range_template_visit(QJSIntlBytes s,
    QJSIntlDateRangeTemplateVisitor visitor, void *opaque)
{
    size_t i = 0, start = 0;
    unsigned int seen = 0;
    uint32_t cp;
    QJSIntlStatus r;
    if (!s.data || !s.length) return QJS_INTL_DATA_ERROR;
    /* First validate the complete input, including every literal scalar. */
    while (i < s.length) {
        if (s.data[i] == '{') {
            unsigned int bit;
            if (s.length - i < 3 || s.data[i + 2] != '}' ||
                (s.data[i + 1] != '0' && s.data[i + 1] != '1'))
                return QJS_INTL_DATA_ERROR;
            bit = 1u << (s.data[i + 1] - '0');
            if (seen & bit) return QJS_INTL_DATA_ERROR;
            seen |= bit; i += 3;
        } else {
            if (s.data[i] == '}') return QJS_INTL_DATA_ERROR;
            r = qjs_intl_date_utf8_next(s, &i, &cp);
            if (r || !cp) return QJS_INTL_DATA_ERROR;
        }
    }
    if (seen != 3) return QJS_INTL_DATA_ERROR;
    if (!visitor) return QJS_INTL_OK;
    for (i = 0; i < s.length; i++) {
        QJSIntlBytes literal;
        if (s.data[i] != '{') continue;
        literal.data = s.data + start; literal.length = i - start;
        if (literal.length && (r = visitor(opaque, literal, 2))) return r;
        literal.length = 0;
        r = visitor(opaque, literal, (unsigned int)(s.data[i + 1] - '0'));
        if (r) return r;
        i += 2; start = i + 1;
    }
    if (start < s.length) {
        QJSIntlBytes literal = { s.data + start, s.length - start };
        return visitor(opaque, literal, 2);
    }
    return QJS_INTL_OK;
}
