/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "style-hour.h"
#include <string.h>

typedef struct Choice {
    size_t index;
    unsigned int family, widths;
    int found;
} Choice;

static int hour12(unsigned int symbol)
{
    return symbol == 'h' || symbol == 'K';
}

static unsigned int expected_period(const QJSIntlDatePatternInfo *info, unsigned int twelve)
{
    unsigned int explicit_period = info->period_symbol == 'a' ? 0 : info->period_symbol;
    return explicit_period ? explicit_period : twelve ? 'a' : 0;
}

static unsigned int width_flag(size_t field)
{
    return field == QJS_INTL_DATETIME_HOUR ? QJS_INTL_DATE_ADAPT_HOUR_WIDTH :
           field == QJS_INTL_DATETIME_MINUTE ? QJS_INTL_DATE_ADAPT_MINUTE_WIDTH :
           field == QJS_INTL_DATETIME_SECOND ? QJS_INTL_DATE_ADAPT_SECOND_WIDTH : 0;
}

static int numeric(int width)
{
    return width == QJS_INTL_DATETIME_NUMERIC || width == QJS_INTL_DATETIME_TWO_DIGIT;
}

static int compatible(const QJSIntlDatePatternInfo *info, const QJSIntlDateTimeComponents *target,
                       unsigned int period, unsigned int allowed, unsigned int *widths)
{
    size_t i;
    *widths = 0;
    if (!info->hour_symbol || info->year_kinds || info->period_symbol != period)
        return 0;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        int a = info->components.values[i], b = target->values[i];
        if (a == b)
            continue;
        if (!(width_flag(i) & allowed) || !numeric(a) || !numeric(b))
            return 0;
        ++*widths;
    }
    return 1;
}

static Choice choose(const QJSIntlDateSourceCandidates *pool, const QJSIntlDateTimeComponents *target,
                      unsigned int period, unsigned int allowed, unsigned int twelve)
{
    Choice result = {0};
    size_t i;
    if (!pool)
        return result;
    for (i = 0; i < pool->count; i++) {
        const QJSIntlDatePatternInfo *info = &pool->records[i].info;
        unsigned int family, widths;
        if (!compatible(info, target, period, allowed, &widths))
            continue;
        family = (unsigned int)hour12(info->hour_symbol) != twelve;
        if (!result.found || family < result.family ||
            (family == result.family && widths < result.widths))
            result = (Choice){i, family, widths, 1};
    }
    return result;
}

static QJSIntlStatus append_layout(const QJSIntlDateAvailableSources *sources, QJSIntlPairPattern *out)
{
    size_t i;
    if (!sources)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < sources->count; i++) {
        QJSIntlDateAvailableEntry entry;
        QJSIntlDateAvailableNode origin;
        QJSIntlDateAvailableGroup group;
        QJSIntlStatus status = qjs_intl_date_available_source_at(sources, i, &entry, &origin);
        if (!status)
            status = qjs_intl_date_available_group_at(&sources->data, origin.group, &group);
        if (status)
            return status;
        if (group.family != QJS_INTL_DATE_AVAILABLE_APPEND)
            return QJS_INTL_INVALID_ARGUMENT;
        if (entry.code.length == 8 && !memcmp(entry.code.data, "Timezone", 8) &&
            entry.plural == QJS_INTL_DATE_NO_PLURAL && !entry.alt.length) {
            if (entry.blocked || !entry.text.length)
                return QJS_INTL_UNSUPPORTED;
            return qjs_intl_pair_pattern_parse(entry.text, out);
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

static QJSIntlStatus check(const QJSIntlDatePattern *pattern, const QJSIntlDateTimeComponents *target,
                            unsigned int period, unsigned int twelve)
{
    QJSIntlDatePatternInfo info;
    unsigned int widths;
    QJSIntlStatus status = qjs_intl_date_pattern_info(pattern, &info);
    if (status)
        return status;
    return compatible(&info, target, period, 0, &widths) && (unsigned int)hour12(info.hour_symbol) == twelve ?
               QJS_INTL_OK : QJS_INTL_DATA_ERROR;
}

static QJSIntlStatus build(const QJSIntlDateStyleSource *time, const QJSIntlDateSourceCandidates *pool,
                            const QJSIntlDateAvailableSources *append, const QJSIntlAllocator *allocator,
                            unsigned int twelve, QJSIntlDateSingleProgram *out)
{
    const QJSIntlDateTimeComponents *target = &time->info.components;
    unsigned int period = expected_period(&time->info, twelve), pass, widths;
    unsigned int allowed = QJS_INTL_DATE_ADAPT_HOUR_WIDTH;
    if (compatible(&time->info, target, period, 0, &widths)) {
        out->source_info = time->info;
        out->source_index = SIZE_MAX;
        return qjs_intl_date_program_adapt(allocator, &time->pattern, target, allowed, twelve,
                                           &out->pattern, &out->adaptations);
    }
    for (pass = 0; pass < 4; pass++) {
        QJSIntlDateTimeComponents wanted = *target;
        Choice choice;
        QJSIntlStatus status;
        if (pass & 1)
            allowed = QJS_INTL_DATE_ADAPT_HOUR_WIDTH | QJS_INTL_DATE_ADAPT_MINUTE_WIDTH | QJS_INTL_DATE_ADAPT_SECOND_WIDTH;
        else
            allowed = QJS_INTL_DATE_ADAPT_HOUR_WIDTH;
        if (pass >= 2) {
            if (!wanted.values[QJS_INTL_DATETIME_ZONE])
                break;
            wanted.values[QJS_INTL_DATETIME_ZONE] = 0;
        }
        choice = choose(pool, &wanted, period, allowed, twelve);
        if (!choice.found)
            continue;
        out->source_info = pool->records[choice.index].info;
        out->source_index = choice.index;
        if (pass < 2)
            return qjs_intl_date_program_adapt(allocator, &pool->records[choice.index].pattern, target,
                                               allowed, twelve, &out->pattern, &out->adaptations);
        else {
            QJSIntlDatePattern base = {0}, zone = {0};
            QJSIntlDatePatternToken zone_token;
            QJSIntlPairPattern layout;
            size_t i;
            status = append_layout(append, &layout);
            if (status)
                return status;
            for (i = 0; i < time->pattern.count; i++) {
                const QJSIntlDatePatternToken *token = &time->pattern.tokens[i];
                if (token->symbol == 'z' || token->symbol == 'v' || token->symbol == 'O') {
                    zone_token = *token;
                    zone.tokens = &zone_token;
                    zone.count = 1;
                    break;
                }
            }
            if (!zone.count)
                return QJS_INTL_DATA_ERROR;
            status = qjs_intl_date_program_adapt(allocator, &pool->records[choice.index].pattern, &wanted,
                                                allowed, twelve, &base, &out->adaptations);
            if (!status)
                status = qjs_intl_date_pattern_join(allocator, &layout, &base, &zone, &out->pattern);
            qjs_intl_date_pattern_clear(allocator, &base);
            if (status)
                return status;
            out->adaptations |= QJS_INTL_DATE_ADAPT_ZONE_APPEND;
            return check(&out->pattern, target, period, twelve);
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_date_style_time_programs_open(const QJSIntlDateStyleSource *time,
                                                   const QJSIntlDateSourceCandidates *pool,
                                                   const QJSIntlDateAvailableSources *append,
                                                   const QJSIntlAllocator *allocator,
                                                   QJSIntlDateSinglePrograms *out)
{
    QJSIntlDateSinglePrograms result = {0};
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !time || time->family != QJS_INTL_DATE_STYLE_TIME || !time->pattern.tokens ||
        !time->info.hour_symbol || !allocator || !allocator->malloc || !allocator->free ||
        (pool && pool->count && !pool->records))
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = *allocator;
    result.components = time->info.components;
    status = build(time, pool, append, allocator, 0, &result.normal);
    if (!status)
        status = build(time, pool, append, allocator, 1, &result.twelve);
    if (!status)
        status = check(&result.normal.pattern, &result.components, expected_period(&time->info, 0), 0);
    if (!status)
        status = check(&result.twelve.pattern, &result.components, expected_period(&time->info, 1), 1);
    if (status) {
        qjs_intl_date_single_programs_clear(&result);
        return status;
    }
    *out = result;
    return QJS_INTL_OK;
}
