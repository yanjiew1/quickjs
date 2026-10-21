/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "style-date.h"
#include <string.h>

static int date_field(size_t field)
{
    return field == QJS_INTL_DATETIME_ERA || field == QJS_INTL_DATETIME_YEAR ||
           field == QJS_INTL_DATETIME_MONTH || field == QJS_INTL_DATETIME_WEEKDAY ||
           field == QJS_INTL_DATETIME_DAY;
}

/* Remaining Y sources can request a real calendar-year alternative. The
   month/no-week CLDR data workaround already ran in selected-style compilation. */
static QJSIntlStatus target(const QJSIntlDatePattern *raw,
                            QJSIntlDateTimeComponents *components, unsigned int *symbols)
{
    size_t i, field;
    int seen = 0;
    memset(components, 0, sizeof(*components));
    memset(symbols, 0, QJS_INTL_DATETIME_COMPONENT_COUNT * sizeof(*symbols));
    for (i = 0; i < raw->count; i++) {
        const QJSIntlDatePatternToken *token = &raw->tokens[i];
        QJSIntlDatePattern one = {0};
        QJSIntlDatePatternInfo info;
        QJSIntlStatus status;
        if (!token->symbol)
            continue;
        if (token->symbol == 'Y') {
            if (seen++ || token->width != 1)
                return QJS_INTL_UNSUPPORTED;
            components->values[QJS_INTL_DATETIME_YEAR] = QJS_INTL_DATETIME_NUMERIC;
            symbols[QJS_INTL_DATETIME_YEAR] = 'y';
            continue;
        }
        one.tokens = raw->tokens + i;
        one.count = 1;
        status = qjs_intl_date_pattern_info(&one, &info);
        if (status)
            return status;
        if (info.year_kinds || info.period_symbol)
            return QJS_INTL_UNSUPPORTED;
        for (field = 0; field < QJS_INTL_DATETIME_COMPONENT_COUNT; field++) {
            if (!info.components.values[field])
                continue;
            if (!date_field(field) || components->values[field])
                return QJS_INTL_UNSUPPORTED;
            components->values[field] = info.components.values[field];
            symbols[field] = token->symbol;
        }
    }
    return seen ? QJS_INTL_OK : QJS_INTL_UNSUPPORTED;
}

static int name_field(unsigned int symbol)
{
    return symbol == 'M' || symbol == 'L' ? QJS_INTL_DATETIME_MONTH :
           symbol == 'E' || symbol == 'e' || symbol == 'c' ? QJS_INTL_DATETIME_WEEKDAY : -1;
}

static int compatible(const QJSIntlDateSourceCandidate *source,
                       const QJSIntlDateTimeComponents *wanted, const unsigned int *symbols,
                       size_t *changes)
{
    size_t field, i;
    *changes = 0;
    if (source->info.year_kinds != QJS_INTL_DATE_YEAR_CALENDAR ||
        source->info.period_symbol || source->info.hour_symbol)
        return 0;
    for (field = 0; field < QJS_INTL_DATETIME_COMPONENT_COUNT; field++) {
        int a = source->info.components.values[field], b = wanted->values[field];
        if (a == b)
            continue;
        if ((field != QJS_INTL_DATETIME_MONTH && field != QJS_INTL_DATETIME_WEEKDAY) ||
            a != QJS_INTL_DATETIME_SHORT || b != QJS_INTL_DATETIME_LONG)
            return 0;
        ++*changes;
    }
    for (i = 0; i < source->pattern.count; i++) {
        unsigned int symbol = source->pattern.tokens[i].symbol;
        int named = name_field(symbol);
        if (named >= 0 && symbol != symbols[named])
            return 0;
    }
    return 1;
}

/* Do not expand an intentional locale override: the skeleton and the actual
   name component must agree before requesting a different textual width. */
static QJSIntlStatus expansion(const QJSIntlDateSourceCandidate *source,
                               const QJSIntlDateTimeComponents *wanted,
                               const unsigned int *symbols, const QJSIntlAllocator *allocator)
{
    QJSIntlDatePattern key = {0};
    QJSIntlDatePatternInfo info;
    QJSIntlStatus status = qjs_intl_date_pattern_compile(allocator, source->source.code, &key);
    size_t field, i;

    if (!status)
        qjs_intl_date_pattern_fix_calendar_year(&key);
    if (!status)
        status = qjs_intl_date_pattern_info(&key, &info);
    if (!status) {
        for (field = 0; field < QJS_INTL_DATETIME_COMPONENT_COUNT; field++) {
            int found = 0;
            if (source->info.components.values[field] == wanted->values[field])
                continue;
            if (info.components.values[field] != source->info.components.values[field]) {
                status = QJS_INTL_UNSUPPORTED;
                break;
            }
            for (i = 0; i < key.count; i++) {
                if (key.tokens[i].symbol == symbols[field])
                    found = 1;
            }
            if (!found) {
                status = QJS_INTL_UNSUPPORTED;
                break;
            }
        }
    }
    qjs_intl_date_pattern_clear(allocator, &key);
    return status == QJS_INTL_NO_MEMORY || status == QJS_INTL_OVERFLOW ? status :
           status ? QJS_INTL_UNSUPPORTED : QJS_INTL_OK;
}

void qjs_intl_date_prepared_style_clear(QJSIntlDatePreparedStyle *style)
{
    if (!style)
        return;
    qjs_intl_date_style_source_clear(&style->style);
    memset(style, 0, sizeof(*style));
}

QJSIntlStatus qjs_intl_date_style_date_open(const QJSIntlDateStyleData *styles,
                                          uint32_t locale, uint32_t calendar,
                                          QJSIntlDateStyleWidth width, uint32_t alternate,
                                          const QJSIntlDateSourceCandidates *pool,
                                          const QJSIntlAllocator *allocator,
                                          QJSIntlDatePreparedStyle *out)
{
    QJSIntlDatePreparedStyle result = {0};
    QJSIntlDatePattern raw = {0};
    QJSIntlDateTimeComponents wanted;
    unsigned int symbols[QJS_INTL_DATETIME_COMPONENT_COUNT];
    size_t i, best = SIZE_MAX, best_changes = SIZE_MAX;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.source_index = SIZE_MAX;
    status = qjs_intl_date_style_source_open(styles, locale, calendar, QJS_INTL_DATE_STYLE_DATE,
                                              width, alternate, allocator, &result.style);
    if (!status) {
        *out = result;
        return QJS_INTL_OK;
    }
    if (status != QJS_INTL_UNSUPPORTED || !pool || !pool->count)
        return status;
    result.style.allocator = *allocator;
    result.style.family = QJS_INTL_DATE_STYLE_DATE;
    result.style.width = width;
    status = qjs_intl_date_style_record_at(styles, locale, calendar, QJS_INTL_DATE_STYLE_DATE,
                width, QJS_INTL_DATE_STYLE_PATTERN, alternate, &result.style.source);
    if (status)
        goto fail;
    if (result.style.source.state != QJS_INTL_DATE_STYLE_VALUE || !result.style.source.text.length) {
        status = QJS_INTL_UNSUPPORTED;
        goto fail;
    }
    status = qjs_intl_date_style_record_at(styles, locale, calendar, QJS_INTL_DATE_STYLE_DATE,
                width, QJS_INTL_DATE_STYLE_SKELETON, alternate, &result.style.skeleton);
    if (!status)
        status = qjs_intl_date_pattern_compile(allocator, result.style.source.text, &raw);
    if (!status)
        status = target(&raw, &wanted, symbols);
    if (status)
        goto fail;
    for (i = 0; i < pool->count; i++) {
        size_t changes;
        if (!compatible(&pool->records[i], &wanted, symbols, &changes) || changes >= best_changes)
            continue;
        status = changes ? expansion(&pool->records[i], &wanted, symbols, allocator) : QJS_INTL_OK;
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            goto fail;
        best = i;
        best_changes = changes;
        if (!changes)
            break;
    }
    if (best == SIZE_MAX) {
        status = QJS_INTL_UNSUPPORTED;
        goto fail;
    }
    status = qjs_intl_date_pattern_copy(allocator, &pool->records[best].pattern, &result.style.pattern);
    if (status)
        goto fail;
    for (i = 0; i < result.style.pattern.count; i++) {
        QJSIntlDatePatternToken *token = &result.style.pattern.tokens[i];
        int field = name_field(token->symbol);
        if (field >= 0 && wanted.values[field] != pool->records[best].info.components.values[field])
            token->width = 4;
    }
    status = qjs_intl_date_pattern_info(&result.style.pattern, &result.style.info);
    if (status)
        goto fail;
    if (result.style.info.year_kinds != QJS_INTL_DATE_YEAR_CALENDAR) {
        status = QJS_INTL_UNSUPPORTED;
        goto fail;
    }
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        if (result.style.info.components.values[i] != wanted.values[i]) {
            status = QJS_INTL_UNSUPPORTED;
            goto fail;
        }
    }
    result.replacement = pool->records[best].source;
    result.origin = pool->records[best].origin;
    result.source_index = best;
    result.adaptations = QJS_INTL_DATE_STYLE_REBUILT;
    qjs_intl_date_pattern_clear(allocator, &raw);
    *out = result;
    return QJS_INTL_OK;
fail:
    qjs_intl_date_pattern_clear(allocator, &raw);
    qjs_intl_date_prepared_style_clear(&result);
    return status;
}
