/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "date-native.h"
#include <limits.h>
#include <string.h>

typedef struct PatternRow {
    QJSIntlDatePattern row;
    int fields[QJS_DATE_FIELD_COUNT];
} PatternRow;
struct QJSIntlNativeDate {
    QJSIntlAllocator allocator;
    QJSIntlDateOptions options;
    QJSIntlDateEnvironment environment;
    QJSIntlDateData data;
    PatternRow *patterns;
    char *strings, *selected;
    size_t selected_length;
    int resolved[QJS_DATE_FIELD_COUNT];
    int add_fraction;
};

int qjs_intl_native_date_hour_cycle(unsigned int preferred, int hour12)
{
    if (preferred > 3 || hour12 < -1 || hour12 > 1) return -1;
    if (hour12 < 0) return (int)preferred;
    return hour12 ? (preferred == 0 ? 0 : 1) : (preferred == 3 ? 3 : 2);
}

static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}
static int word(QJSIntlBytes a, const char *b)
{
    return a.length == strlen(b) && !memcmp(a.data, b, a.length);
}
static int size_add(size_t *size, size_t add)
{
    if (add > SIZE_MAX - *size) return 0;
    *size += add; return 1;
}
static int text_size(size_t *size, QJSIntlBytes s)
{
    size_t at = 0;
    uint32_t cp;
    if ((!s.data && s.length) || !size_add(size, s.length)) return 0;
    while (at < s.length)
        if (qjs_intl_date_utf8_next(s, &at, &cp) || !cp) return 0;
    return 1;
}
/* Compiled qualifier patterns contain exactly one literal argument. */
static int zone_pattern_valid(QJSIntlBytes pattern)
{
    size_t i;
    int seen = 0;
    for (i = 0; i < pattern.length; i++) {
        if (pattern.data[i] == '{') {
            if (seen || i + 2 >= pattern.length || pattern.data[i + 1] != '0' ||
                pattern.data[i + 2] != '}') return 0;
            seen = 1; i += 2;
        } else if (pattern.data[i] == '}') return 0;
    }
    return !pattern.length || seen;
}

static void *array_new(const QJSIntlAllocator *a, size_t count, size_t width)
{
    if (!count) return NULL;
    if (count > SIZE_MAX / width) return NULL;
    return a->malloc(a->opaque, count * width);
}
static QJSIntlBytes copy_text(char **cursor, QJSIntlBytes s)
{
    QJSIntlBytes r;
    r.data = *cursor; r.length = s.length;
    if (s.length) memcpy(*cursor, s.data, s.length);
    *cursor += s.length;
    return r;
}
void qjs_intl_native_date_close(QJSIntlNativeDate *p)
{
    QJSIntlAllocator a;
    if (!p) return;
    a = p->allocator;
    a.free(a.opaque, p->patterns);
    a.free(a.opaque, (void *)p->data.names);
    a.free(a.opaque, (void *)p->data.periods);
    a.free(a.opaque, (void *)p->data.zone_names);
    a.free(a.opaque, (void *)p->data.meta_periods);
    a.free(a.opaque, (void *)p->data.zone_formats);
    a.free(a.opaque, p->strings);
    a.free(a.opaque, p->selected);
    a.free(a.opaque, p);
}

static int options_valid(const QJSIntlDateOptions *o)
{
    int i;
    if (!o || !o->calendar.data || !o->calendar.length ||
        !o->time_zone.data || !o->time_zone.length ||
        o->hour_cycle > 3 || o->format_matcher > 1 ||
        o->date_style < -1 || o->date_style > 3 ||
        o->time_style < -1 || o->time_style > 3) return 0;
    for (i = 0; i < QJS_DATE_FIELD_COUNT; i++) {
        int v = o->fields[i];
        if (v < -1 || v > (i == QJS_DATE_ZONE ? 5 : i == QJS_DATE_FRACTION ? 3 : 4)) return 0;
        if (v >= 0 && (o->date_style >= 0 || o->time_style >= 0)) return 0;
        if (v >= 0 && (i == QJS_DATE_WEEKDAY || i == QJS_DATE_ERA ||
            i == QJS_DATE_DAY_PERIOD) && v < 2) return 0;
        if (v >= 2 && (i == QJS_DATE_YEAR || i == QJS_DATE_DAY ||
            i == QJS_DATE_HOUR || i == QJS_DATE_MINUTE || i == QJS_DATE_SECOND)) return 0;
        if (i == QJS_DATE_FRACTION && v == 0) return 0;
    }
    return 1;
}
static unsigned int desired_family(const QJSIntlNativeDate *p)
{
    return p->options.hour_cycle < 2 ? 1 : 2;
}
static int family_allowed(const QJSIntlNativeDate *p, const PatternRow *r)
{
    return !r->row.family || r->row.family == desired_family(p);
}
static int pure_date(const PatternRow *r)
{
    int i, have = 0;
    for (i = 0; i < QJS_DATE_FIELD_COUNT; i++) {
        if (i <= QJS_DATE_DAY) have |= r->fields[i] >= 0;
        else if (r->fields[i] >= 0) return 0;
    }
    return have;
}
static int pure_time(const PatternRow *r)
{
    int i, have = 0;
    for (i = 0; i < QJS_DATE_FIELD_COUNT; i++) {
        if (i <= QJS_DATE_DAY && r->fields[i] >= 0) return 0;
        if (i > QJS_DATE_DAY) have |= r->fields[i] >= 0;
    }
    return have;
}
/* Join literals use LDML pattern quoting. Braces must denote each argument
 * exactly once and outside quotes. Copying patterns preserves all literals.
 */
static QJSIntlStatus join_pattern(QJSIntlNativeDate *p, QJSIntlBytes join,
                                  QJSIntlBytes date, QJSIntlBytes time)
{
    size_t i, length = 0, at = 0;
    unsigned int seen = 0;
    int quote = 0;
    char *result;
    for (i = 0; i < join.length; i++) {
        char c = join.data[i];
        if (c == '\'') {
            if (i + 1 < join.length && join.data[i + 1] == '\'') {
                if (!size_add(&length, 2)) return QJS_INTL_OVERFLOW;
                i++; continue;
            }
            quote = !quote;
        }
        if (!quote && c == '{') {
            unsigned int bit;
            QJSIntlBytes s;
            if (i + 2 >= join.length || join.data[i + 2] != '}' ||
                (join.data[i + 1] != '0' && join.data[i + 1] != '1')) return QJS_INTL_DATA_ERROR;
            bit = 1u << (join.data[i + 1] - '0');
            if (seen & bit) return QJS_INTL_DATA_ERROR;
            seen |= bit;
            s = bit == 1 ? time : date;
            if (!size_add(&length, s.length)) return QJS_INTL_OVERFLOW;
            i += 2;
        } else if (!quote && c == '}') return QJS_INTL_DATA_ERROR;
        else if (!size_add(&length, 1)) return QJS_INTL_OVERFLOW;
    }
    if (quote || seen != 3 || !length) return QJS_INTL_DATA_ERROR;
    result = p->allocator.malloc(p->allocator.opaque, length);
    if (!result) return QJS_INTL_NO_MEMORY;
    quote = 0;
    for (i = 0; i < join.length; i++) {
        char c = join.data[i];
        if (c == '\'') {
            if (i + 1 < join.length && join.data[i + 1] == '\'') {
                result[at++] = c; result[at++] = c; i++; continue;
            }
            quote = !quote;
        }
        if (!quote && c == '{') {
            QJSIntlBytes s = join.data[i + 1] == '0' ? time : date;
            if (s.length) memcpy(result + at, s.data, s.length);
            at += s.length; i += 2;
        } else result[at++] = c;
    }
    p->selected = result; p->selected_length = length;
    return QJS_INTL_OK;
}
static const PatternRow *style_row(const QJSIntlNativeDate *p, unsigned int kind,
                                   unsigned int style)
{
    size_t i;
    for (i = 0; i < p->data.pattern_count; i++)
        if (p->patterns[i].row.kind == kind && p->patterns[i].row.style == style)
            return &p->patterns[i];
    return NULL;
}
static QJSIntlStatus select_pattern(QJSIntlNativeDate *p)
{
    const PatternRow *first = NULL, *second = NULL, *join = NULL;
    size_t i, j;
    int best = INT_MIN, score, combined[QJS_DATE_FIELD_COUNT], k;
    QJSIntlStatus r;
    if (p->options.date_style >= 0 || p->options.time_style >= 0) {
        if (p->options.date_style >= 0)
            first = style_row(p, QJS_DATE_DATE_STYLE, (unsigned int)p->options.date_style);
        if (p->options.time_style >= 0) {
            const PatternRow *base = style_row(p, QJS_DATE_TIME_STYLE,
                                               (unsigned int)p->options.time_style);
            if (!base) return QJS_INTL_UNSUPPORTED;
            second = base;
            if (!family_allowed(p, base)) {
                second = NULL;
                for (i = 0; i < p->data.pattern_count; i++) {
                    const PatternRow *candidate = &p->patterns[i];
                    if (candidate->row.kind != QJS_DATE_AVAILABLE ||
                        !pure_time(candidate) || !family_allowed(p, candidate)) continue;
                    score = qjs_intl_date_basic_score(base->fields, candidate->fields);
                    if (score > best) { best = score; second = candidate; }
                }
                if (!second) return QJS_INTL_UNSUPPORTED;
                for (k = 0; k < QJS_DATE_FIELD_COUNT; k++)
                    if ((base->fields[k] >= 0) != (second->fields[k] >= 0))
                        return QJS_INTL_UNSUPPORTED;
            }
            if (!first) { first = second; second = NULL; }
        }
        if (!first) return QJS_INTL_UNSUPPORTED;
        if (second) join = style_row(p, QJS_DATE_JOIN, (unsigned int)p->options.date_style);
    } else {
        for (i = 0; i < p->data.pattern_count; i++) {
            const PatternRow *candidate = &p->patterns[i];
            if (candidate->row.kind != QJS_DATE_AVAILABLE || !family_allowed(p, candidate)) continue;
            score = qjs_intl_date_basic_score(p->options.fields, candidate->fields);
            if (score > best) { best = score; first = candidate; second = NULL; }
        }
        join = style_row(p, QJS_DATE_JOIN, QJS_DATE_MEDIUM);
        if (join) {
            /* Conceptual formats include these complete date/time pairs in
             * source row order after standalone available formats. Equal
             * scores retain the first format, as BasicFormatMatcher requires.
             */
            for (i = 0; i < p->data.pattern_count; i++) {
                const PatternRow *date = &p->patterns[i];
                if (date->row.kind != QJS_DATE_AVAILABLE || !pure_date(date)) continue;
                for (j = 0; j < p->data.pattern_count; j++) {
                    const PatternRow *time = &p->patterns[j];
                    if (time->row.kind != QJS_DATE_AVAILABLE || !pure_time(time) || !family_allowed(p, time)) continue;
                    for (k = 0; k < QJS_DATE_FIELD_COUNT; k++)
                        combined[k] = date->fields[k] >= 0 ? date->fields[k] : time->fields[k];
                    score = qjs_intl_date_basic_score(p->options.fields, combined);
                    if (score > best) { best = score; first = date; second = time; }
                }
            }
        }
        if (!first) return QJS_INTL_UNSUPPORTED;
    }
    if (second) {
        if (!join) return QJS_INTL_UNSUPPORTED;
        r = join_pattern(p, join->row.pattern, first->row.pattern, second->row.pattern);
        if (r) return r;
    } else {
        p->selected_length = first->row.pattern.length;
        p->selected = p->allocator.malloc(p->allocator.opaque, p->selected_length);
        if (!p->selected) return QJS_INTL_NO_MEMORY;
        memcpy(p->selected, first->row.pattern.data, p->selected_length);
    }
    {
        unsigned int family;
        QJSIntlBytes pattern = { p->selected, p->selected_length };
        r = qjs_intl_date_pattern_fields(pattern, p->resolved, &family);
        if (r) return r;
    }
    if (p->options.date_style < 0 && p->options.time_style < 0 &&
        p->options.format_matcher == QJS_DATE_BEST_FIT) {
        /* Keep the BasicFormatMatcher result when a requested field cannot
         * be appended. BestFitFormatMatcher must still return a format record.
         * Check before changing widths or adding a fractional-second field. */
        for (k = 0; k < QJS_DATE_FIELD_COUNT; k++) {
            if (p->options.fields[k] >= 0 && p->resolved[k] < 0 &&
                !(k == QJS_DATE_FRACTION && p->resolved[QJS_DATE_SECOND] >= 0))
                return QJS_INTL_OK;
        }
        for (k = 0; k < QJS_DATE_FIELD_COUNT; k++) {
            int requested = p->options.fields[k];
            if (requested < 0) continue;
            if (p->resolved[k] < 0) {
                p->add_fraction = 1; p->resolved[k] = requested; continue;
            }
            if ((k == QJS_DATE_MINUTE || k == QJS_DATE_SECOND) && requested == QJS_DATE_NUMERIC)
                continue; /* retain contextual mm/ss from the locale pattern */
            p->resolved[k] = requested;
        }
    }
    if (p->options.time_style >= 0) {
        const PatternRow *base = style_row(p, QJS_DATE_TIME_STYLE, (unsigned int)p->options.time_style);
        for (k = QJS_DATE_DAY_PERIOD; k < QJS_DATE_FIELD_COUNT; k++)
            if (base->fields[k] >= 0 &&
                !(k == QJS_DATE_HOUR && base->row.family != desired_family(p)))
                p->resolved[k] = base->fields[k];
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_native_date_open(const QJSIntlAllocator *a,
    const QJSIntlDateData *d, const QJSIntlDateOptions *o,
    const QJSIntlDateEnvironment *e, QJSIntlNativeDate **out)
{
    QJSIntlNativeDate *p;
    size_t bytes = 0, i, j;
    char *cursor;
    QJSIntlStatus r;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !a->malloc || !a->free || !d || !options_valid(o) ||
        (!d->patterns && d->pattern_count) || (!d->names && d->name_count) ||
        (!d->periods && d->period_count) || (!d->zone_names && d->zone_name_count) ||
        (!d->meta_periods && d->meta_period_count) ||
        (!d->zone_formats && d->zone_format_count)) return QJS_INTL_INVALID_ARGUMENT;
    if (!word(o->calendar, "gregory") && !word(o->calendar, "iso8601") &&
        (!e || !e->calendar || !e->calendar_supported ||
         !e->calendar_supported(e->opaque, o->calendar))) return QJS_INTL_UNSUPPORTED;
    if (!d->pattern_count) return QJS_INTL_UNSUPPORTED;
    if (d->range_fallback.length &&
        qjs_intl_date_range_template_visit(d->range_fallback, NULL, NULL))
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < 10; i++) {
        if (!qjs_intl_date_scalar(d->digits[i]) || !d->digits[i]) return QJS_INTL_DATA_ERROR;
        for (j = 0; j < i; j++) if (d->digits[i] == d->digits[j]) return QJS_INTL_DATA_ERROR;
    }
    if (!text_size(&bytes, o->calendar) || !text_size(&bytes, o->time_zone) ||
        !text_size(&bytes, d->data_zone) || !text_size(&bytes, d->decimal) ||
        !text_size(&bytes, d->gmt_format) || !text_size(&bytes, d->gmt_zero) ||
        !text_size(&bytes, d->hour_positive) || !text_size(&bytes, d->hour_negative) ||
        !text_size(&bytes, d->range_fallback)) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < d->pattern_count; i++)
        if (!text_size(&bytes, d->patterns[i].skeleton) || !text_size(&bytes, d->patterns[i].pattern)) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < d->name_count; i++)
        if (!text_size(&bytes, d->names[i].name)) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < d->zone_name_count; i++) {
        if (!text_size(&bytes, d->zone_names[i].key) || !text_size(&bytes, d->zone_names[i].exemplar)) return QJS_INTL_DATA_ERROR;
        for (j = 0; j < 6; j++) if (!text_size(&bytes, d->zone_names[i].names[j])) return QJS_INTL_DATA_ERROR;
    }
    for (i = 0; i < d->meta_period_count; i++)
        if (!text_size(&bytes, d->meta_periods[i].zone) || !text_size(&bytes, d->meta_periods[i].metazone) ||
            d->meta_periods[i].from_ms >= d->meta_periods[i].before_ms ||
            d->meta_periods[i].has_name_offsets < 0 || d->meta_periods[i].has_name_offsets > 1 ||
            (d->meta_periods[i].has_name_offsets &&
             (d->meta_periods[i].standard_name_offset <= -86400 || d->meta_periods[i].standard_name_offset >= 86400 ||
              d->meta_periods[i].daylight_name_offset <= -86400 || d->meta_periods[i].daylight_name_offset >= 86400 ||
              d->meta_periods[i].standard_name_offset == d->meta_periods[i].daylight_name_offset)))
            return QJS_INTL_DATA_ERROR;
    for (i = 0; i < d->zone_format_count; i++) {
        const QJSIntlDateZoneFormat *row = &d->zone_formats[i];
        if (!row->zone.length || !text_size(&bytes, row->zone) ||
            !text_size(&bytes, row->metazone) || !text_size(&bytes, row->location) ||
            !text_size(&bytes, row->name_pattern) || !zone_pattern_valid(row->name_pattern) ||
            (row->name_pattern.length && !row->metazone.length))
            return QJS_INTL_DATA_ERROR;
    }
    if (d->pattern_count > SIZE_MAX / sizeof(PatternRow) ||
        d->name_count > SIZE_MAX / sizeof(QJSIntlDateName) ||
        d->period_count > SIZE_MAX / sizeof(QJSIntlDatePeriodRule) ||
        d->zone_name_count > SIZE_MAX / sizeof(QJSIntlDateZoneName) ||
        d->meta_period_count > SIZE_MAX / sizeof(QJSIntlDateMetaPeriod) ||
        d->zone_format_count > SIZE_MAX / sizeof(QJSIntlDateZoneFormat)) return QJS_INTL_OVERFLOW;
    p = a->malloc(a->opaque, sizeof(*p));
    if (!p) return QJS_INTL_NO_MEMORY;
    memset(p, 0, sizeof(*p)); p->allocator = *a; p->options = *o;
    if (e) p->environment = *e;
    p->data = *d;
    p->data.names = NULL; p->data.periods = NULL;
    p->data.zone_names = NULL; p->data.meta_periods = NULL;
    p->data.zone_formats = NULL;
    p->patterns = array_new(a, d->pattern_count, sizeof(*p->patterns));
    p->data.names = array_new(a, d->name_count, sizeof(*d->names));
    p->data.periods = array_new(a, d->period_count, sizeof(*d->periods));
    p->data.zone_names = array_new(a, d->zone_name_count, sizeof(*d->zone_names));
    p->data.meta_periods = array_new(a, d->meta_period_count, sizeof(*d->meta_periods));
    p->data.zone_formats = array_new(a, d->zone_format_count, sizeof(*d->zone_formats));
    p->strings = a->malloc(a->opaque, bytes ? bytes : 1);
    if (!p->patterns || (d->name_count && !p->data.names) ||
        (d->period_count && !p->data.periods) || (d->zone_name_count && !p->data.zone_names) ||
        (d->meta_period_count && !p->data.meta_periods) ||
        (d->zone_format_count && !p->data.zone_formats) || !p->strings) {
        qjs_intl_native_date_close(p); return QJS_INTL_NO_MEMORY;
    }
    cursor = p->strings;
    p->options.calendar = copy_text(&cursor, o->calendar);
    p->options.time_zone = copy_text(&cursor, o->time_zone);
    p->data.data_zone = copy_text(&cursor, d->data_zone);
    p->data.decimal = copy_text(&cursor, d->decimal);
    p->data.gmt_format = copy_text(&cursor, d->gmt_format);
    p->data.gmt_zero = copy_text(&cursor, d->gmt_zero);
    p->data.hour_positive = copy_text(&cursor, d->hour_positive);
    p->data.hour_negative = copy_text(&cursor, d->hour_negative);
    p->data.range_fallback = copy_text(&cursor, d->range_fallback);
    for (i = 0; i < d->pattern_count; i++) {
        unsigned int family;
        PatternRow *row = &p->patterns[i];
        row->row = d->patterns[i];
        row->row.skeleton = copy_text(&cursor, row->row.skeleton);
        row->row.pattern = copy_text(&cursor, row->row.pattern);
        if (row->row.kind > QJS_DATE_JOIN || row->row.family > 2 ||
            (row->row.kind ? row->row.style > 3 : row->row.style != 255)) {
            qjs_intl_native_date_close(p); return QJS_INTL_DATA_ERROR;
        }
        if (row->row.kind == QJS_DATE_JOIN) {
            int k; for (k = 0; k < QJS_DATE_FIELD_COUNT; k++) row->fields[k] = -1;
        } else {
            r = qjs_intl_date_pattern_fields(row->row.pattern, row->fields, &family);
            if (r || family != row->row.family) {
                qjs_intl_native_date_close(p); return r ? r : QJS_INTL_DATA_ERROR;
            }
        }
    }
    for (i = 0; i < d->name_count; i++) {
        QJSIntlDateName *row = (QJSIntlDateName *)p->data.names + i;
        *row = d->names[i]; row->name = copy_text(&cursor, row->name);
        if (row->field > QJS_DATE_NAME_LEAP_TEMPLATE || row->context > 1 || row->width > 3 || !row->name.length ||
            (row->field == 0 && row->context) ||
            (row->field == 1 && (row->index < 1 || row->index > 14 ||
                (row->index == 14 && !word(o->calendar, "hebrew")))) ||
            (row->field == 2 && (row->index < 1 || row->index > 7)) ||
            (row->field == 3 && row->index >= QJS_DATE_PERIOD_COUNT) ||
            (row->field == QJS_DATE_NAME_CYCLIC_YEAR && (row->context ||
                row->index < 1 || row->index > 60 ||
                (!word(o->calendar, "chinese") && !word(o->calendar, "dangi")))) ||
            (row->field == QJS_DATE_NAME_LEAP_TEMPLATE && (row->index > 1 ||
                (!row->index && (row->context || row->width)) ||
                (!word(o->calendar, "chinese") && !word(o->calendar, "dangi")))) ||
            (row->width == 3 && row->field != QJS_DATE_NAME_WEEKDAY)) {
            qjs_intl_native_date_close(p); return QJS_INTL_DATA_ERROR;
        }
    }
    if (d->period_count) memcpy((void *)p->data.periods, d->periods, d->period_count * sizeof(*d->periods));
    for (i = 0; i < d->period_count; i++) {
        const QJSIntlDatePeriodRule *rule = &d->periods[i];
        if (rule->period >= QJS_DATE_PERIOD_COUNT || rule->exact > 1 ||
            rule->from_second >= 86400 || rule->before_second > 86400 ||
            (rule->exact ? rule->from_second != rule->before_second : rule->from_second == rule->before_second)) {
            qjs_intl_native_date_close(p); return QJS_INTL_DATA_ERROR;
        }
    }
    for (i = 0; i < d->zone_name_count; i++) {
        QJSIntlDateZoneName *row = (QJSIntlDateZoneName *)p->data.zone_names + i;
        *row = d->zone_names[i]; row->key = copy_text(&cursor, row->key);
        if (row->metazone > 1 || !row->key.length || (row->metazone && row->exemplar.length)) {
            qjs_intl_native_date_close(p); return QJS_INTL_DATA_ERROR;
        }
        row->exemplar = copy_text(&cursor, row->exemplar);
        for (j = 0; j < 6; j++) row->names[j] = copy_text(&cursor, row->names[j]);
    }
    for (i = 0; i < d->meta_period_count; i++) {
        QJSIntlDateMetaPeriod *row = (QJSIntlDateMetaPeriod *)p->data.meta_periods + i;
        *row = d->meta_periods[i]; row->zone = copy_text(&cursor, row->zone);
        row->metazone = copy_text(&cursor, row->metazone);
    }
    for (i = 0; i < d->zone_format_count; i++) {
        QJSIntlDateZoneFormat *row = (QJSIntlDateZoneFormat *)p->data.zone_formats + i;
        *row = d->zone_formats[i];
        row->zone = copy_text(&cursor, row->zone);
        row->metazone = copy_text(&cursor, row->metazone);
        row->location = copy_text(&cursor, row->location);
        row->name_pattern = copy_text(&cursor, row->name_pattern);
    }
    p->data.patterns = NULL; /* private PatternRow owns the snapshot */
    r = select_pattern(p);
    if (r) { qjs_intl_native_date_close(p); return r; }
    *out = p; return QJS_INTL_OK;
}

typedef struct Builder {
    QJSIntlNativeDate *owner;
    QJSIntlFormatted result;
    size_t text_capacity, part_capacity;
    QJSIntlDateFields fields;
    QJSIntlDateZoneInfo zone;
    int64_t epoch_ms, epoch_seconds;
    int zone_stability_checked, zone_stability_proven;
    int utc_view;
    QJSIntlPartSource source;
} Builder;
static QJSIntlStatus grow(Builder *b, size_t units)
{
    QJSIntlAllocator *a = &b->owner->allocator;
    size_t needed, capacity;
    uint16_t *text;
    if (units > SIZE_MAX - b->result.length) return QJS_INTL_OVERFLOW;
    needed = b->result.length + units;
    if (needed <= b->text_capacity) return QJS_INTL_OK;
    capacity = b->text_capacity ? b->text_capacity : 32;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) { capacity = needed; break; }
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*text)) return QJS_INTL_OVERFLOW;
    text = a->malloc(a->opaque, capacity * sizeof(*text));
    if (!text) return QJS_INTL_NO_MEMORY;
    if (b->result.length) memcpy(text, b->result.text, b->result.length * sizeof(*text));
    a->free(a->opaque, b->result.text);
    b->result.text = text; b->text_capacity = capacity;
    return QJS_INTL_OK;
}
static QJSIntlStatus scalar(Builder *b, uint32_t cp)
{
    QJSIntlStatus r;
    if (!qjs_intl_date_scalar(cp)) return QJS_INTL_DATA_ERROR;
    r = grow(b, cp > 0xffff ? 2 : 1);
    if (r) return r;
    if (cp > 0xffff) {
        cp -= 0x10000;
        b->result.text[b->result.length++] = (uint16_t)(0xd800 + (cp >> 10));
        b->result.text[b->result.length++] = (uint16_t)(0xdc00 + (cp & 1023));
    } else b->result.text[b->result.length++] = (uint16_t)cp;
    return QJS_INTL_OK;
}
static QJSIntlStatus utf8(Builder *b, QJSIntlBytes s)
{
    size_t at = 0;
    uint32_t cp;
    QJSIntlStatus r;
    while (at < s.length) {
        r = qjs_intl_date_utf8_next(s, &at, &cp);
        if (r || (r = scalar(b, cp)) != QJS_INTL_OK) return r;
    }
    return QJS_INTL_OK;
}
static QJSIntlStatus part(Builder *b, QJSIntlPartType type, size_t start)
{
    QJSIntlAllocator *a = &b->owner->allocator;
    QJSIntlPart *parts, *item;
    size_t capacity;
    if (start == b->result.length) return QJS_INTL_OK;
    if (type == QJS_INTL_PART_LITERAL && b->result.part_count &&
        b->result.parts[b->result.part_count - 1].type == type &&
        b->result.parts[b->result.part_count - 1].source == b->source) {
        b->result.parts[b->result.part_count - 1].end = b->result.length;
        return QJS_INTL_OK;
    }
    if (b->result.part_count == b->part_capacity) {
        capacity = b->part_capacity ? b->part_capacity : 16;
        if (b->part_capacity) {
            if (capacity > SIZE_MAX / 2) return QJS_INTL_OVERFLOW;
            capacity *= 2;
        }
        if (capacity > SIZE_MAX / sizeof(*parts)) return QJS_INTL_OVERFLOW;
        parts = a->malloc(a->opaque, capacity * sizeof(*parts));
        if (!parts) return QJS_INTL_NO_MEMORY;
        if (b->result.part_count) memcpy(parts, b->result.parts, b->result.part_count * sizeof(*parts));
        a->free(a->opaque, b->result.parts); b->result.parts = parts; b->part_capacity = capacity;
    }
    item = &b->result.parts[b->result.part_count++];
    memset(item, 0, sizeof(*item)); item->type = type;
    item->start = start; item->end = b->result.length;
    item->source = b->source;
    return QJS_INTL_OK;
}
static QJSIntlStatus number(Builder *b, uint64_t value, unsigned int minimum,
                            int two_digit)
{
    unsigned int buffer[24], n = 0, i;
    QJSIntlStatus r;
    if (two_digit) value %= 100;
    do { buffer[n++] = (unsigned int)(value % 10); value /= 10; } while (value);
    while (n < minimum) buffer[n++] = 0;
    for (i = n; i > 0; i--) {
        r = scalar(b, b->owner->data.digits[buffer[i - 1]]);
        if (r) return r;
    }
    return QJS_INTL_OK;
}
static const QJSIntlDateName *find_name(const Builder *b, unsigned int field,
    unsigned int context, unsigned int width, unsigned int index)
{
    size_t i;
    for (i = 0; i < b->owner->data.name_count; i++) {
        const QJSIntlDateName *n = &b->owner->data.names[i];
        if (n->field == field && n->context == context && n->width == width && n->index == index)
            return n;
    }
    return NULL;
}
static QJSIntlStatus name(Builder *b, unsigned int field, unsigned int context,
                          unsigned int width, unsigned int index)
{
    const QJSIntlDateName *n = find_name(b, field, context, width, index);
    return n && n->name.length ? utf8(b, n->name) : QJS_INTL_UNSUPPORTED;
}
static unsigned int name_width(int resolved)
{
    return resolved == QJS_DATE_NARROW ? 0 : resolved == QJS_DATE_LONG ? 2 : 1;
}
static QJSIntlStatus month_value(Builder *b, unsigned int symbol, int width)
{
    if (width < 2) return number(b, b->fields.month, width == 0 ? 2 : 1, 0);
    return name(b, QJS_DATE_NAME_MONTH, symbol == 'L', name_width(width),
        b->fields.month_name_index ? b->fields.month_name_index : b->fields.month);
}
static QJSIntlStatus month(Builder *b, unsigned int symbol, int width)
{
    const QJSIntlDateName *n;
    QJSIntlBytes literal;
    size_t i, start = 0;
    unsigned int seen = 0;
    QJSIntlStatus r;
    if (!b->fields.leap_month) return month_value(b, symbol, width);
    n = find_name(b, QJS_DATE_NAME_LEAP_TEMPLATE, width < 2 ? 0 : symbol == 'L',
        width < 2 ? 0 : name_width(width), width < 2 ? 0 : 1);
    if (!n) return QJS_INTL_UNSUPPORTED;
    /* Templates are literal CLDR text, not LDML patterns. All text in this
     * expansion belongs to the single month part, including the leap marker.
     * Validate completely before appending so malformed braces cannot leak.
     */
    for (i = 0; i < n->name.length; i++) {
        if (n->name.data[i] == '{') {
            if (seen || i + 2 >= n->name.length || n->name.data[i + 1] != '0' ||
                n->name.data[i + 2] != '}') return QJS_INTL_DATA_ERROR;
            seen = 1; i += 2;
        } else if (n->name.data[i] == '}') return QJS_INTL_DATA_ERROR;
    }
    if (!seen) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < n->name.length; i++) {
        if (n->name.data[i] != '{') continue;
        literal.data = n->name.data + start; literal.length = i - start;
        if ((r = utf8(b, literal)) || (r = month_value(b, symbol, width))) return r;
        start = i + 3; i += 2;
    }
    literal.data = n->name.data + start; literal.length = n->name.length - start;
    return utf8(b, literal);
}
static QJSIntlStatus related_year(Builder *b, unsigned int minimum)
{
    int64_t value = b->fields.related_year;
    QJSIntlStatus r;
    if (!b->fields.has_related_year) return QJS_INTL_UNSUPPORTED;
    if (value < 0 && (r = scalar(b, '-'))) return r;
    return number(b, value < 0 ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value,
                  minimum, 0);
}
static QJSIntlStatus period_index(const Builder *b, unsigned int symbol,
                                  unsigned int *out)
{
    unsigned int second = b->fields.hour * 3600 + b->fields.minute * 60 + b->fields.second;
    unsigned int selected = b->fields.hour < 12 ? QJS_DATE_AM : QJS_DATE_PM;
    size_t i;
    if (symbol == 'b' && !b->fields.minute && !b->fields.second && !b->fields.millisecond &&
        (b->fields.hour == 0 || b->fields.hour == 12))
        selected = b->fields.hour ? QJS_DATE_NOON : QJS_DATE_MIDNIGHT;
    if (symbol == 'B') {
        int found = 0;
        for (i = 0; i < b->owner->data.period_count; i++) {
            const QJSIntlDatePeriodRule *rule = &b->owner->data.periods[i];
            if (rule->exact && !b->fields.millisecond && second == rule->from_second) {
                selected = rule->period; found = 1; break;
            }
        }
        if (!found) for (i = 0; i < b->owner->data.period_count; i++) {
            const QJSIntlDatePeriodRule *rule = &b->owner->data.periods[i];
            int match = rule->from_second < rule->before_second ?
                second >= rule->from_second && second < rule->before_second :
                second >= rule->from_second || second < rule->before_second;
            if (!rule->exact && match) { selected = rule->period; found = 1; break; }
        }
        if (!found) return QJS_INTL_UNSUPPORTED;
    }
    *out = selected;
    return QJS_INTL_OK;
}
static QJSIntlStatus period(Builder *b, unsigned int symbol, unsigned int width)
{
    unsigned int selected;
    QJSIntlStatus r = period_index(b, symbol, &selected);
    return r ? r : name(b, QJS_DATE_NAME_PERIOD, 0, width, selected);
}
static QJSIntlStatus offset_body(Builder *b, int long_form)
{
    int64_t signed_offset = b->zone.offset_seconds;
    uint64_t offset = signed_offset < 0 ? (uint64_t)-signed_offset : (uint64_t)signed_offset;
    QJSIntlBytes pattern = signed_offset < 0 ? b->owner->data.hour_negative : b->owner->data.hour_positive;
    size_t h = 0, he, m, me;
    QJSIntlBytes prefix, separator, suffix;
    QJSIntlStatus r;
    while (h < pattern.length && pattern.data[h] != 'H') h++;
    he = h; while (he < pattern.length && pattern.data[he] == 'H') he++;
    m = he; while (m < pattern.length && pattern.data[m] != 'm') m++;
    me = m; while (me < pattern.length && pattern.data[me] == 'm') me++;
    if (h == he || m == me || h > m || offset >= 86400) return QJS_INTL_DATA_ERROR;
    prefix.data = pattern.data; prefix.length = h;
    separator.data = pattern.data + he; separator.length = m - he;
    suffix.data = pattern.data + me; suffix.length = pattern.length - me;
    if ((r = utf8(b, prefix)) || (r = number(b, offset / 3600, long_form ? 2 : 1, 0))) return r;
    if (long_form || offset % 3600) {
        if ((r = utf8(b, separator)) || (r = number(b, (offset / 60) % 60, 2, 0))) return r;
        if (offset % 60 && ((r = utf8(b, separator)) || (r = number(b, offset % 60, 2, 0)))) return r;
    }
    return utf8(b, suffix);
}
static QJSIntlStatus offset_name(Builder *b, int long_form)
{
    QJSIntlBytes pattern = b->owner->data.gmt_format;
    size_t i, start = 0;
    int seen = 0;
    QJSIntlStatus r;
    if (!b->zone.offset_seconds) return b->owner->data.gmt_zero.length ? utf8(b, b->owner->data.gmt_zero) : QJS_INTL_UNSUPPORTED;
    for (i = 0; i < pattern.length; i++) {
        if (pattern.data[i] != '{' && pattern.data[i] != '}') continue;
        if (pattern.data[i] != '{' || i + 2 >= pattern.length ||
            pattern.data[i + 1] != '0' || pattern.data[i + 2] != '}' || seen) return QJS_INTL_DATA_ERROR;
        {
            QJSIntlBytes prefix = { pattern.data + start, i - start };
            if ((r = utf8(b, prefix)) || (r = offset_body(b, long_form))) return r;
        }
        seen = 1; i += 2; start = i + 1;
    }
    if (!seen) return QJS_INTL_UNSUPPORTED;
    {
        QJSIntlBytes suffix = { pattern.data + start, pattern.length - start };
        return utf8(b, suffix);
    }
}
/* CLDR48.2 TR35 Dates Type Fallback1/2. The expensive provider proof is
 * requested only when a missing generic has a usable standard alternative.
 * The whole +/-184-day window must stay inside the active metazone period,
 * keeping its explicit name-offset policy unchanged. */
static QJSIntlStatus zone_stable(Builder *b, const QJSIntlDateMetaPeriod *period,
                                 int *proven)
{
    const int64_t days = INT64_C(184) * 86400, ms = days * 1000;
    QJSIntlNativeDate *p = b->owner;
    QJSIntlStatus r;
    *proven = 0;
    if (!period || b->epoch_ms - ms < period->from_ms ||
        b->epoch_ms + ms >= period->before_ms || !p->environment.zone_name_stable)
        return QJS_INTL_OK;
    if (!b->zone_stability_checked) {
        r = p->environment.zone_name_stable(p->environment.opaque,
            p->options.time_zone, b->epoch_seconds - days, b->epoch_seconds + days,
            &b->zone_stability_proven);
        if (r) return r;
        if (b->zone_stability_proven < 0 || b->zone_stability_proven > 1) return QJS_INTL_DATA_ERROR;
        b->zone_stability_checked = 1;
    }
    *proven = b->zone_stability_proven; return QJS_INTL_OK;
}
static QJSIntlStatus zone_label(Builder *b, const QJSIntlDateZoneName *row,
    unsigned int base, int requested, int enhanced,
    const QJSIntlDateMetaPeriod *period, QJSIntlBytes *out)
{
    int proven;
    QJSIntlStatus r;
    *out = (QJSIntlBytes){ NULL, 0 };
    if (requested >= 0 && row->names[base + (unsigned int)requested].length) {
        *out = row->names[base + (unsigned int)requested]; return QJS_INTL_OK;
    }
    if (enhanced && !row->names[1].length && !row->names[4].length) {
        if (row->names[base + 2].length) *out = row->names[base + 2];
        else if (row->names[base].length) *out = row->names[base];
    } else if (enhanced && requested == 2 && row->names[base].length) {
        r = zone_stable(b, period, &proven);
        if (r) return r;
        if (proven) *out = row->names[base];
    }
    return QJS_INTL_OK;
}
static const QJSIntlDateZoneFormat *zone_format(const QJSIntlNativeDate *p,
                                                QJSIntlBytes meta)
{
    size_t i;
    for (i = 0; i < p->data.zone_format_count; i++) {
        const QJSIntlDateZoneFormat *row = &p->data.zone_formats[i];
        if (equal(row->zone, p->data.data_zone) && equal(row->metazone, meta))
            return row;
    }
    return NULL;
}
static QJSIntlStatus qualified_zone(Builder *b, QJSIntlBytes label,
                                    const QJSIntlDateZoneFormat *format)
{
    QJSIntlBytes pattern;
    size_t i, start = 0;
    QJSIntlStatus r;
    if (!format || !format->name_pattern.length) return utf8(b, label);
    pattern = format->name_pattern;
    for (i = 0; i < pattern.length; i++) {
        if (pattern.data[i] != '{') continue;
        {
            QJSIntlBytes prefix = { pattern.data + start, i - start };
            if ((r = utf8(b, prefix)) || (r = utf8(b, label))) return r;
        }
        i += 2; start = i + 1;
    }
    {
        QJSIntlBytes suffix = { pattern.data + start, pattern.length - start };
        return utf8(b, suffix);
    }
}
static QJSIntlStatus zone_name(Builder *b, int style)
{
    QJSIntlNativeDate *p = b->owner;
    QJSIntlBytes meta = { NULL, 0 }, label;
    const QJSIntlDateZoneFormat *format;
    const QJSIntlDateMetaPeriod *period = NULL;
    QJSIntlStatus r;
    size_t i;
    unsigned int base = style == 1 || style == 5 ? 3 : 0;
    int requested = style >= 4 ? 2 : b->zone.daylight;
    if (b->utc_view) return offset_name(b, style == 1 || style == 3 || style == 5);
    if (style == 2 || style == 3) return offset_name(b, style == 3);
    for (i = 0; i < p->data.meta_period_count; i++) {
        const QJSIntlDateMetaPeriod *row = &p->data.meta_periods[i];
        if (equal(row->zone, p->data.data_zone) &&
            b->epoch_ms >= row->from_ms && b->epoch_ms < row->before_ms) {
            period = row; meta = row->metazone; break;
        }
    }
    if (requested != 2 && period && period->has_name_offsets) {
        requested = b->zone.offset_seconds == period->standard_name_offset ? 0 :
            b->zone.offset_seconds == period->daylight_name_offset ? 1 : -1;
    }
    for (i = 0; i < p->data.zone_name_count; i++) {
        const QJSIntlDateZoneName *row = &p->data.zone_names[i];
        if (!row->metazone && equal(row->key, p->data.data_zone)) {
            r = zone_label(b, row, base, requested, p->data.zone_format_count != 0, period, &label);
            if (r) return r;
            if (label.length) return utf8(b, label); /* explicit TZID translation */
        }
    }
    format = zone_format(p, meta);
    if (meta.length) for (i = 0; i < p->data.zone_name_count; i++) {
        const QJSIntlDateZoneName *row = &p->data.zone_names[i];
        if (row->metazone && equal(row->key, meta)) {
            r = zone_label(b, row, base, requested, p->data.zone_format_count != 0, period, &label);
            if (r) return r;
            if (label.length) return qualified_zone(b, label, format);
        }
    }
    if (style >= 4) {
        QJSIntlBytes empty = { NULL, 0 };
        if (!format) format = zone_format(p, empty);
        if (format && format->location.length) return utf8(b, format->location);
    }
    return offset_name(b, style == 1 || style == 5);
}

static QJSIntlStatus render(void *opaque, QJSIntlBytes literal,
                            unsigned int ch, unsigned int count)
{
    Builder *b = opaque;
    QJSIntlNativeDate *p = b->owner;
    QJSIntlPartType type = QJS_INTL_PART_LITERAL;
    size_t start = b->result.length;
    unsigned int width, hour, fraction;
    int v;
    QJSIntlStatus r;
    if (!ch) {
        if ((r = utf8(b, literal))) return r;
        return part(b, type, start);
    }
    switch (ch) {
    case 'G':
        type = QJS_INTL_PART_ERA;
        r = name(b, QJS_DATE_NAME_ERA, 0, name_width(p->resolved[QJS_DATE_ERA]), b->fields.era); break;
    case 'y':
        type = QJS_INTL_PART_YEAR; v = p->resolved[QJS_DATE_YEAR];
        r = number(b, (uint64_t)b->fields.year, v == 0 ? 2 : count == 2 ? 1 : count, v == 0); break;
    case 'r':
        type = QJS_INTL_PART_RELATED_YEAR;
        r = related_year(b, count); break;
    case 'U':
        type = QJS_INTL_PART_YEAR_NAME;
        r = b->fields.year_name_index ? name(b, QJS_DATE_NAME_CYCLIC_YEAR, 0,
            count == 5 ? 0 : count == 4 ? 2 : 1, b->fields.year_name_index) : QJS_INTL_UNSUPPORTED;
        break;
    case 'M': case 'L':
        type = QJS_INTL_PART_MONTH; v = p->resolved[QJS_DATE_MONTH];
        r = month(b, ch, v); break;
    case 'd':
        type = QJS_INTL_PART_DAY;
        r = number(b, b->fields.day, p->resolved[QJS_DATE_DAY] == 0 ? 2 : 1, 0); break;
    case 'E': case 'e': case 'c':
        type = QJS_INTL_PART_WEEKDAY; width = name_width(p->resolved[QJS_DATE_WEEKDAY]);
        if (count == 6 && p->resolved[QJS_DATE_WEEKDAY] == QJS_DATE_SHORT) width = 3;
        r = name(b, QJS_DATE_NAME_WEEKDAY, ch == 'c', width, b->fields.weekday); break;
    case 'a': case 'b': case 'B':
        type = QJS_INTL_PART_DAY_PERIOD;
        width = ch == 'a' ? count == 5 ? 0 : count == 4 ? 2 : 1 : name_width(p->resolved[QJS_DATE_DAY_PERIOD]);
        r = period(b, ch, width); break;
    case 'h': case 'H': case 'k': case 'K':
        type = QJS_INTL_PART_HOUR; hour = b->fields.hour;
        if (p->options.hour_cycle < 2) hour %= 12;
        if (!hour && p->options.hour_cycle == QJS_DATE_H12) hour = 12;
        if (!hour && p->options.hour_cycle == QJS_DATE_H24) hour = 24;
        r = number(b, hour, p->resolved[QJS_DATE_HOUR] == 0 ? 2 : 1, 0); break;
    case 'm':
        type = QJS_INTL_PART_MINUTE;
        r = number(b, b->fields.minute, p->resolved[QJS_DATE_MINUTE] == 0 ? 2 : 1, 0); break;
    case 's':
        type = QJS_INTL_PART_SECOND;
        r = number(b, b->fields.second, p->resolved[QJS_DATE_SECOND] == 0 ? 2 : 1, 0); break;
    case 'S':
        type = QJS_INTL_PART_FRACTIONAL_SECOND; width = (unsigned int)p->resolved[QJS_DATE_FRACTION];
        fraction = b->fields.millisecond;
        if (width == 1) fraction /= 100;
        if (width == 2) fraction /= 10;
        r = number(b, fraction, width, 0); break;
    case 'z': case 'v': case 'O':
        type = QJS_INTL_PART_TIME_ZONE_NAME;
        r = zone_name(b, p->resolved[QJS_DATE_ZONE]); break;
    default: return QJS_INTL_UNSUPPORTED;
    }
    if (r || (r = part(b, type, start))) return r;
    if (ch == 's' && p->add_fraction) {
        start = b->result.length;
        if ((r = utf8(b, p->data.decimal)) || (r = part(b, QJS_INTL_PART_LITERAL, start))) return r;
        {
            QJSIntlBytes empty = { NULL, 0 };
            return render(b, empty, 'S', (unsigned int)p->resolved[QJS_DATE_FRACTION]);
        }
    }
    return QJS_INTL_OK;
}
static QJSIntlStatus zone_info(QJSIntlNativeDate *p, int64_t seconds,
                               QJSIntlDateZoneInfo *out)
{
    QJSIntlBytes id = p->options.time_zone;
    unsigned int hour, minute;
    out->offset_seconds = 0; out->daylight = -1;
    if (word(id, "UTC")) { out->daylight = 0; return QJS_INTL_OK; }
    if (id.data[0] != '+' && id.data[0] != '-')
        return p->environment.zone ? p->environment.zone(p->environment.opaque, id, seconds, out) : QJS_INTL_UNSUPPORTED;
    if (id.length != 6 || (id.data[0] != '+' && id.data[0] != '-') || id.data[3] != ':' ||
        id.data[1] < '0' || id.data[1] > '9' || id.data[2] < '0' || id.data[2] > '9' ||
        id.data[4] < '0' || id.data[4] > '9' || id.data[5] < '0' || id.data[5] > '9') return QJS_INTL_UNSUPPORTED;
    hour = (unsigned int)(id.data[1] - '0') * 10 + (unsigned int)(id.data[2] - '0');
    minute = (unsigned int)(id.data[4] - '0') * 10 + (unsigned int)(id.data[5] - '0');
    if (hour > 23 || minute > 59) return QJS_INTL_INVALID_ARGUMENT;
    out->offset_seconds = (int32_t)(hour * 3600 + minute * 60);
    if (id.data[0] == '-') out->offset_seconds = -out->offset_seconds;
    out->daylight = 0; return QJS_INTL_OK;
}
void qjs_intl_native_date_clear(const QJSIntlAllocator *a, QJSIntlFormatted *r)
{
    if (!r) return;
    if (a && a->free) { a->free(a->opaque, r->text); a->free(a->opaque, r->parts); }
    memset(r, 0, sizeof(*r));
}
/* PlainYearMonth may store any valid reference day in its boundary month.
 * The civil provider therefore allows thirty-two extra epoch days before
 * timezone adjustment and one more afterwards. The bank applies each
 * Temporal type's narrower civil policy; this internal guard also prevents
 * arbitrary int32-year values from reaching calendar/name rendering.
 */
static int extended_epoch_valid(QJSTemporalEpochNs epoch)
{
    QJSTemporalEpochNs quotient;
    uint64_t remainder;
    int64_t days;
    return !qjs_temporal_epoch_ns_divide(&quotient, &remainder, epoch,
                                        UINT64_C(86400000000000)) &&
           !qjs_temporal_epoch_ns_to_int64(&days, quotient) &&
           days >= -INT64_C(100000032) && days <= INT64_C(100000032);
}
static QJSIntlStatus prepare_ns(Builder *b, QJSIntlNativeDate *p,
                                QJSTemporalEpochNs epoch, int utc_view)
{
    QJSTemporalEpochNs offset, quotient;
    QJSTemporalISODateTime iso;
    QJSIntlStatus r;
    uint64_t remainder;
    int64_t seconds, days;
    if (!p || !extended_epoch_valid(epoch)) return QJS_INTL_INVALID_ARGUMENT;
    memset(b, 0, sizeof(*b)); b->owner = p; b->utc_view = utc_view;
    /* Both quotients are mathematical floor. Do not use the Instant-only
     * to_milliseconds helper for a valid extended plain reference date.
     */
    if (qjs_temporal_epoch_ns_divide(&quotient, &remainder, epoch,
                                    UINT64_C(1000000)) ||
        qjs_temporal_epoch_ns_to_int64(&b->epoch_ms, quotient) ||
        qjs_temporal_epoch_ns_divide(&quotient, &remainder, epoch,
                                    UINT64_C(1000000000)) ||
        qjs_temporal_epoch_ns_to_int64(&seconds, quotient)) return QJS_INTL_OVERFLOW;
    b->epoch_seconds = seconds;
    if (utc_view) {
        b->zone.offset_seconds = 0; b->zone.daylight = 0;
    } else {
        r = zone_info(p, seconds, &b->zone);
        if (r) return r;
    }
    if (b->zone.offset_seconds <= -86400 || b->zone.offset_seconds >= 86400 ||
        b->zone.daylight < -1 || b->zone.daylight > 1) return QJS_INTL_DATA_ERROR;
    offset = qjs_temporal_epoch_ns_from_int64((int64_t)b->zone.offset_seconds * INT64_C(1000000000));
    if (qjs_temporal_epoch_ns_add(&epoch, epoch, offset) ||
        qjs_temporal_iso_datetime_from_epoch_ns(&iso, epoch)) return QJS_INTL_OVERFLOW;
    if (qjs_temporal_iso_date_to_days(&days, iso.date) ||
        days < -INT64_C(100000033) || days > INT64_C(100000033))
        return QJS_INTL_INVALID_ARGUMENT;
    if (word(p->options.calendar, "gregory") || word(p->options.calendar, "iso8601")) {
        b->fields.era = iso.date.year > 0;
        b->fields.year = iso.date.year > 0 ? iso.date.year : 1 - (int64_t)iso.date.year;
        b->fields.calendar_year = iso.date.year; b->fields.has_calendar_year = 1;
        b->fields.month = (unsigned int)iso.date.month; b->fields.day = (unsigned int)iso.date.day;
        b->fields.hour = (unsigned int)iso.time.hour; b->fields.minute = (unsigned int)iso.time.minute;
        b->fields.second = (unsigned int)iso.time.second; b->fields.millisecond = (unsigned int)iso.time.millisecond;
    } else {
        r = p->environment.calendar(p->environment.opaque, p->options.calendar, &iso, &b->fields);
        if (r) return r;
    }
    b->fields.weekday = (unsigned int)(((days + 3) % 7 + 7) % 7 + 1);
    if (b->fields.year < 1 || b->fields.month < 1 || b->fields.month > 13 ||
        b->fields.day < 1 || b->fields.day > 31 || b->fields.weekday < 1 || b->fields.weekday > 7 ||
        b->fields.hour > 23 || b->fields.minute > 59 || b->fields.second > 59 || b->fields.millisecond > 999 ||
        b->fields.month_name_index > 14 || b->fields.year_name_index > 60 || b->fields.leap_month > 1 ||
        (b->fields.has_related_year != 0 && b->fields.has_related_year != 1) ||
        (b->fields.has_calendar_year != 0 && b->fields.has_calendar_year != 1))
        return QJS_INTL_DATA_ERROR;
    return QJS_INTL_OK;
}
static int number_valid(int64_t epoch_ms)
{
    return epoch_ms >= -INT64_C(8640000000000000) &&
           epoch_ms <= INT64_C(8640000000000000);
}
static QJSTemporalEpochNs number_epoch(int64_t epoch_ms)
{
    QJSTemporalEpochNs epoch = qjs_temporal_epoch_ns_from_int64(epoch_ms);
    /* A previously validated TimeClip value cannot overflow this product. */
    (void)qjs_temporal_epoch_ns_multiply(&epoch, epoch, 1000000);
    return epoch;
}
static QJSIntlStatus render_selected(Builder *b)
{
    QJSIntlBytes pattern = { b->owner->selected, b->owner->selected_length };
    return qjs_intl_date_pattern_visit(pattern, render, b);
}
QJSIntlStatus qjs_intl_native_date_format_ns(QJSIntlNativeDate *p,
    QJSTemporalEpochNs epoch, int is_plain, QJSIntlFormatted *out)
{
    Builder b;
    QJSIntlStatus r;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if ((is_plain != 0 && is_plain != 1) ||
        (!is_plain && !qjs_temporal_epoch_ns_is_valid(epoch)))
        return QJS_INTL_INVALID_ARGUMENT;
    r = prepare_ns(&b, p, epoch, is_plain);
    if (r) return r;
    r = render_selected(&b);
    if (r) { qjs_intl_native_date_clear(&p->allocator, &b.result); return r; }
    *out = b.result; return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_date_format(QJSIntlNativeDate *p, int64_t epoch_ms,
                                         QJSIntlFormatted *out)
{
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!number_valid(epoch_ms)) return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_native_date_format_ns(p, number_epoch(epoch_ms), 0, out);
}
QJSIntlBytes qjs_intl_native_date_calendar(const QJSIntlNativeDate *p)
{
    QJSIntlBytes empty = { NULL, 0 };
    return p ? p->options.calendar : empty;
}
void qjs_intl_native_date_resolved_fields(const QJSIntlNativeDate *p, int *out)
{
    int i;
    if (!out) return;
    for (i = 0; i < QJS_DATE_FIELD_COUNT; i++) out[i] = p ? p->resolved[i] : -1;
}
/* All optional Table 6 records are present and select the same complete
 * pattern pair as Default. Consequently every local field is relevant.
 * The locale's flexible-rule period identity is our ILD comparison String
 * representation (the stable token represented by its index); with no
 * flexible rule at that time, the AM/PM identity is used. Labels/widths do
 * not change this representation. DST/zone and weekday are absent from the
 * specified comparison table, even when a timezone label changes.
 */
static int relevant_equal(const Builder *a, const Builder *b)
{
    const QJSIntlDateFields *x = &a->fields, *y = &b->fields;
    unsigned int px, py, divisor = 1;
    int precision = a->owner->resolved[QJS_DATE_FRACTION];
    int64_t year1 = x->has_calendar_year ? x->calendar_year : x->year;
    int64_t year2 = y->has_calendar_year ? y->calendar_year : y->year;
    if (x->era != y->era || year1 != year2 || x->month != y->month ||
        x->month_name_index != y->month_name_index || x->leap_month != y->leap_month ||
        x->has_related_year != y->has_related_year ||
        (x->has_related_year && x->related_year != y->related_year) ||
        x->year_name_index != y->year_name_index ||
        x->day != y->day || (x->hour < 12) != (y->hour < 12)) return 0;
    if (period_index(a, 'B', &px)) px = x->hour < 12 ? QJS_DATE_AM : QJS_DATE_PM;
    if (period_index(b, 'B', &py)) py = y->hour < 12 ? QJS_DATE_AM : QJS_DATE_PM;
    if (px != py || x->hour != y->hour || x->minute != y->minute ||
        x->second != y->second) return 0;
    if (precision == 1) divisor = 100;
    else if (precision == 2) divisor = 10;
    return x->millisecond / divisor == y->millisecond / divisor;
}
typedef struct RangeBuilder {
    Builder output;
    const Builder *endpoints[2];
} RangeBuilder;
static QJSIntlStatus range_piece(void *opaque, QJSIntlBytes literal,
                                 unsigned int argument)
{
    RangeBuilder *range = opaque;
    Builder *b = &range->output;
    size_t start = b->result.length;
    QJSIntlStatus r;
    if (argument == 2) {
        b->source = QJS_INTL_SOURCE_SHARED;
        if ((r = utf8(b, literal))) return r;
        return part(b, QJS_INTL_PART_LITERAL, start);
    }
    if (argument > 1) return QJS_INTL_DATA_ERROR;
    b->fields = range->endpoints[argument]->fields;
    b->zone = range->endpoints[argument]->zone;
    b->epoch_ms = range->endpoints[argument]->epoch_ms;
    b->epoch_seconds = range->endpoints[argument]->epoch_seconds;
    b->zone_stability_checked = range->endpoints[argument]->zone_stability_checked;
    b->zone_stability_proven = range->endpoints[argument]->zone_stability_proven;
    b->utc_view = range->endpoints[argument]->utc_view;
    b->source = argument ? QJS_INTL_SOURCE_END_RANGE : QJS_INTL_SOURCE_START_RANGE;
    return render_selected(b);
}
QJSIntlStatus qjs_intl_native_date_range_ns(QJSIntlNativeDate *p,
    QJSTemporalEpochNs a, QJSTemporalEpochNs b, int is_plain,
    QJSIntlFormatted *out)
{
    Builder first, second, compare_first, compare_second;
    RangeBuilder range;
    QJSIntlStatus r;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!p || (is_plain != 0 && is_plain != 1) ||
        !extended_epoch_valid(a) || !extended_epoch_valid(b) ||
        (!is_plain && (!qjs_temporal_epoch_ns_is_valid(a) ||
                       !qjs_temporal_epoch_ns_is_valid(b))))
        return QJS_INTL_INVALID_ARGUMENT;
    /* The pinned Temporal range clause compares both exact epochs in the
     * formatter timezone even for IsPlain=true. Rendering then overrides
     * ToLocalTime's timezone with +00:00. Keep these two views explicit.
     */
    if ((r = prepare_ns(&compare_first, p, a, 0)) ||
        (r = prepare_ns(&compare_second, p, b, 0))) return r;
    if (!compare_first.fields.has_calendar_year || !compare_second.fields.has_calendar_year)
        return QJS_INTL_UNSUPPORTED;
    if (is_plain) {
        r = prepare_ns(&first, p, a, 1);
        if (r) return r;
    } else first = compare_first;
    if (relevant_equal(&compare_first, &compare_second)) {
        first.source = QJS_INTL_SOURCE_SHARED;
        r = render_selected(&first);
        if (r) { qjs_intl_native_date_clear(&p->allocator, &first.result); return r; }
        *out = first.result; return QJS_INTL_OK;
    }
    if (!p->data.range_fallback.length) return QJS_INTL_UNSUPPORTED;
    if (is_plain) {
        r = prepare_ns(&second, p, b, 1);
        if (r) return r;
    } else second = compare_second;
    memset(&range, 0, sizeof(range)); range.output.owner = p;
    range.endpoints[0] = &first; range.endpoints[1] = &second;
    r = qjs_intl_date_range_template_visit(p->data.range_fallback, range_piece, &range);
    if (r) { qjs_intl_native_date_clear(&p->allocator, &range.output.result); return r; }
    *out = range.output.result; return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_date_range(QJSIntlNativeDate *p, int64_t a, int64_t b,
                                        QJSIntlFormatted *out)
{
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    /* Both Number endpoints were converted by the frontend before TimeClip.
     * Validate both clipped values before consulting environment data.
     */
    if (!number_valid(a) || !number_valid(b)) return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_native_date_range_ns(p, number_epoch(a), number_epoch(b), 0, out);
}
