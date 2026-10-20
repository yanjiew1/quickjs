/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "ordinary-programs.h"
#include "../common/format-builder.h"
#include <string.h>

#define BIT(field) (1U << QJS_INTL_DATETIME_ ## field)
#define DATE_MASK (BIT(YEAR) | BIT(MONTH) | BIT(DAY))
#define CLOCK_MASK (BIT(HOUR) | BIT(MINUTE) | BIT(SECOND))

typedef struct Build {
    QJSIntlDateOrdinaryPrograms result;
    const QJSIntlDateSourceCandidates *pool;
    const QJSIntlDateAvailableSources *append;
    const QJSIntlDateCombinationData *connectors;
    uint32_t locale, calendar;
    QJSIntlBytes decimal;
    size_t capacity;
} Build;

static unsigned int mask(const QJSIntlDateTimeComponents *components)
{
    unsigned int result = 0;
    size_t i;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if (components->values[i])
            result |= 1U << i;
    return result;
}

static int equal(const QJSIntlDateTimeComponents *a, const QJSIntlDateTimeComponents *b)
{
    size_t i;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if (a->values[i] != b->values[i])
            return 0;
    return 1;
}

static size_t find(const Build *build, unsigned int wanted)
{
    size_t i;
    for (i = 0; i < build->result.count; i++)
        if (mask(&build->result.records[i].single.components) == wanted)
            return i;
    return SIZE_MAX;
}

static void initialize(Build *build, QJSIntlDateOrdinaryProgram *record)
{
    memset(record, 0, sizeof(*record));
    record->first = record->second = SIZE_MAX;
    record->single.allocator = build->result.allocator;
    record->single.normal.source_index = record->single.twelve.source_index = SIZE_MAX;
}

static QJSIntlStatus add(Build *build, QJSIntlDateOrdinaryProgram *record)
{
    QJSIntlDatePatternInfo normal, twelve;
    QJSIntlStatus status = qjs_intl_date_pattern_info(&record->single.normal.pattern, &normal);
    size_t i;
    if (!status && record->single.twelve.pattern.count) {
        status = qjs_intl_date_pattern_info(&record->single.twelve.pattern, &twelve);
        if (!status && (!equal(&normal.components, &twelve.components) ||
                        normal.year_kinds != twelve.year_kinds ||
                        (twelve.hour_symbol != 'h' && twelve.hour_symbol != 'K') ||
                        (normal.hour_symbol != 'H' && normal.hour_symbol != 'k') ||
                        twelve.period_symbol != (normal.period_symbol ? normal.period_symbol : 'a')))
            status = QJS_INTL_DATA_ERROR;
    } else if (!status && normal.hour_symbol) {
        status = QJS_INTL_DATA_ERROR;
    }
    if (status)
        goto done;
    if (!equal(&record->single.components, &normal.components)) {
        status = QJS_INTL_DATA_ERROR;
        goto done;
    }
    for (i = 0; i < build->result.count; i++)
        if (equal(&normal.components, &build->result.records[i].single.components))
            goto done;
    if (build->result.count == build->capacity) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    build->result.records[build->result.count++] = *record;
    memset(record, 0, sizeof(*record));
done:
    qjs_intl_date_single_programs_clear(&record->single);
    return status;
}

static QJSIntlStatus append_field(Build *build, size_t base, const char *key,
                                  unsigned int symbol, size_t width)
{
    QJSIntlDateOrdinaryProgram record;
    QJSIntlDatePatternToken token = {{0}, width, symbol};
    QJSIntlDatePattern field = {0};
    QJSIntlPairPattern layout;
    QJSIntlStatus status = QJS_INTL_UNSUPPORTED;
    size_t i;
    initialize(build, &record);
    field.tokens = &token;
    field.count = 1;
    for (i = 0; i < build->append->count; i++) {
        QJSIntlDateAvailableGroup group;
        status = qjs_intl_date_available_source_at(build->append, i, &record.append, &record.origin);
        if (!status)
            status = qjs_intl_date_available_group_at(&build->append->data, record.origin.group, &group);
        if (status)
            return status;
        if (group.family != QJS_INTL_DATE_AVAILABLE_APPEND)
            return QJS_INTL_INVALID_ARGUMENT;
        if (record.append.plural != QJS_INTL_DATE_NO_PLURAL || record.append.alt.length ||
            record.append.code.length != strlen(key) || memcmp(record.append.code.data, key, strlen(key)))
            continue;
        if (record.append.blocked || !record.append.text.length)
            return QJS_INTL_UNSUPPORTED;
        status = qjs_intl_pair_pattern_parse(record.append.text, &layout);
        if (status)
            return status;
        status = qjs_intl_date_pattern_join(&build->result.allocator, &layout,
                   &build->result.records[base].single.normal.pattern, &field, &record.single.normal.pattern);
        if (status)
            return status;
        record.first = base;
        record.single.components = build->result.records[base].single.components;
        record.single.components.values[symbol == 'y' ? QJS_INTL_DATETIME_YEAR :
                                                        QJS_INTL_DATETIME_WEEKDAY] =
            symbol == 'y' ? QJS_INTL_DATETIME_NUMERIC : QJS_INTL_DATETIME_SHORT;
        record.construction = QJS_INTL_DATE_ORDINARY_APPEND;
        return add(build, &record);
    }
    return QJS_INTL_UNSUPPORTED;
}

/* Construct a plain clock pair. An explicit B layout can supply AM/PM's
   position, but its B-bearing public component record is never relabeled. */
static QJSIntlStatus plain_clock(Build *build, unsigned int wanted)
{
    QJSIntlDateOrdinaryProgram record;
    QJSIntlDateTimeComponents target = {{0}};
    size_t normal = SIZE_MAX, twelve = SIZE_MAX, flexible = SIZE_MAX, i;
    QJSIntlStatus status;
    initialize(build, &record);
    for (i = 0; i < build->pool->count; i++) {
        const QJSIntlDatePatternInfo *info = &build->pool->records[i].info;
        unsigned int fields = mask(&info->components);
        if (fields == wanted && !info->period_symbol && normal == SIZE_MAX) {
            normal = i;
            target = info->components;
        }
        if (fields == wanted && info->period_symbol == 'a' && twelve == SIZE_MAX)
            twelve = i;
        if (fields == (wanted | BIT(DAY_PERIOD)) && info->period_symbol == 'B' && flexible == SIZE_MAX)
            flexible = i;
    }
    if (normal == SIZE_MAX || (twelve == SIZE_MAX && flexible == SIZE_MAX))
        return QJS_INTL_UNSUPPORTED;
    record.single.components = target;
    status = qjs_intl_date_program_adapt(&build->result.allocator,
                 &build->pool->records[normal].pattern, &target, QJS_INTL_DATE_ADAPT_HOUR_WIDTH,
                 0, &record.single.normal.pattern, &record.single.normal.adaptations);
    if (status)
        return status;
    record.single.normal.source_index = normal;
    record.single.normal.source_info = build->pool->records[normal].info;
    {
        QJSIntlDatePattern replaced = {0};
        const QJSIntlDatePattern *source;
        size_t chosen = twelve == SIZE_MAX ? flexible : twelve;
        source = &build->pool->records[chosen].pattern;
        if (twelve == SIZE_MAX) {
            QJSIntlDatePatternToken token = {{0}, 1, 'a'};
            QJSIntlDatePattern period = {0};
            period.tokens = &token;
            period.count = 1;
            status = qjs_intl_date_pattern_replace(&build->result.allocator, source, 'B', &period, &replaced);
            source = &replaced;
            record.construction |= QJS_INTL_DATE_ORDINARY_PERIOD;
        }
        if (!status)
            status = qjs_intl_date_program_adapt(&build->result.allocator, source, &target,
                         QJS_INTL_DATE_ADAPT_HOUR_WIDTH | QJS_INTL_DATE_ADAPT_MINUTE_WIDTH |
                         QJS_INTL_DATE_ADAPT_SECOND_WIDTH, 1, &record.single.twelve.pattern,
                         &record.single.twelve.adaptations);
        qjs_intl_date_pattern_clear(&build->result.allocator, &replaced);
        record.single.twelve.source_index = chosen;
        record.single.twelve.source_info = build->pool->records[chosen].info;
    }
    if (status) {
        qjs_intl_date_single_programs_clear(&record.single);
        return status;
    }
    return add(build, &record);
}

static QJSIntlStatus fractional_pattern(Build *build, const QJSIntlDatePattern *base,
                                        size_t digits, QJSIntlDatePattern *out)
{
    QJSIntlDatePatternToken tokens[3] = {{{0}, 0, 0}, {build->decimal, 0, 0}, {{0}, digits, 'S'}};
    QJSIntlDatePattern replacement = {0};
    size_t i;
    for (i = 0; i < base->count; i++)
        if (base->tokens[i].symbol == 's')
            break;
    if (i == base->count)
        return QJS_INTL_DATA_ERROR;
    tokens[0] = base->tokens[i];
    replacement.tokens = tokens;
    replacement.count = 3;
    return qjs_intl_date_pattern_replace(&build->result.allocator, base, 's', &replacement, out);
}

static QJSIntlStatus fraction(Build *build, size_t base)
{
    size_t digits;
    for (digits = 1; digits <= 3; digits++) {
        QJSIntlDateOrdinaryProgram record;
        QJSIntlStatus status;
        initialize(build, &record);
        record.first = base;
        record.single.components = build->result.records[base].single.components;
        record.single.components.values[QJS_INTL_DATETIME_FRACTION] = digits;
        record.construction = QJS_INTL_DATE_ORDINARY_FRACTION;
        status = fractional_pattern(build, &build->result.records[base].single.normal.pattern,
                                     digits, &record.single.normal.pattern);
        if (!status)
            status = fractional_pattern(build, &build->result.records[base].single.twelve.pattern,
                                         digits, &record.single.twelve.pattern);
        if (status) {
            qjs_intl_date_single_programs_clear(&record.single);
            return status;
        }
        status = add(build, &record);
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus combine(Build *build, size_t date, size_t time)
{
    QJSIntlDateOrdinaryProgram record;
    QJSIntlDatePattern connector = {0};
    QJSIntlStatus status;
    initialize(build, &record);
    status = qjs_intl_date_combination_record_at(build->connectors, build->locale, build->calendar,
                 QJS_INTL_DATE_STYLE_SHORT, QJS_INTL_DATE_COMBINATION_STANDARD, 0, &record.connector);
    if (status)
        return status;
    if (record.connector.state != QJS_INTL_DATE_COMBINATION_VALUE || !record.connector.text.length)
        return QJS_INTL_UNSUPPORTED;
    status = qjs_intl_date_combination_compile(&build->result.allocator, record.connector.text, &connector);
    if (!status)
        status = qjs_intl_date_pattern_combine(&build->result.allocator, &connector,
                     &build->result.records[time].single.normal.pattern,
                     &build->result.records[date].single.normal.pattern, &record.single.normal.pattern);
    if (!status)
        status = qjs_intl_date_pattern_combine(&build->result.allocator, &connector,
                     &build->result.records[time].single.twelve.pattern,
                     &build->result.records[date].single.normal.pattern, &record.single.twelve.pattern);
    qjs_intl_date_pattern_clear(&build->result.allocator, &connector);
    if (status) {
        qjs_intl_date_single_programs_clear(&record.single);
        return status;
    }
    {
        size_t i;
        record.single.components = build->result.records[date].single.components;
        for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
            int value = build->result.records[time].single.components.values[i];
            if (value) {
                if (record.single.components.values[i]) {
                    qjs_intl_date_single_programs_clear(&record.single);
                    return QJS_INTL_DATA_ERROR;
                }
                record.single.components.values[i] = value;
            }
        }
    }
    record.first = date;
    record.second = time;
    record.construction = QJS_INTL_DATE_ORDINARY_COMBINE;
    return add(build, &record);
}

static QJSIntlStatus ensure(Build *build, unsigned int wanted)
{
    size_t first, second;
    QJSIntlStatus status;
    if (find(build, wanted) != SIZE_MAX)
        return QJS_INTL_OK;
    if (wanted & BIT(FRACTION)) {
        status = ensure(build, wanted & ~BIT(FRACTION));
        return status ? status : fraction(build, find(build, wanted & ~BIT(FRACTION)));
    }
    if ((wanted & DATE_MASK) && (wanted & BIT(HOUR))) {
        unsigned int date = wanted & (DATE_MASK | BIT(WEEKDAY)), time = wanted & ~date;
        status = ensure(build, date);
        if (!status)
            status = ensure(build, time);
        if (status)
            return status;
        first = find(build, date);
        second = find(build, time);
        return combine(build, first, second);
    }
    if (wanted & BIT(WEEKDAY)) {
        status = ensure(build, wanted & ~BIT(WEEKDAY));
        return status ? status : append_field(build, find(build, wanted & ~BIT(WEEKDAY)), "Day-Of-Week", 'E', 3);
    }
    if (wanted & BIT(YEAR)) {
        status = ensure(build, wanted & ~BIT(YEAR));
        return status ? status : append_field(build, find(build, wanted & ~BIT(YEAR)), "Year", 'y', 1);
    }
    if ((wanted & BIT(HOUR)) && !(wanted & BIT(DAY_PERIOD)))
        return plain_clock(build, wanted);
    return QJS_INTL_UNSUPPORTED;
}

void qjs_intl_date_ordinary_programs_clear(QJSIntlDateOrdinaryPrograms *programs)
{
    size_t i;
    if (!programs)
        return;
    for (i = 0; i < programs->count; i++)
        qjs_intl_date_single_programs_clear(&programs->records[i].single);
    if (programs->records)
        programs->allocator.free(programs->allocator.opaque, programs->records);
    memset(programs, 0, sizeof(*programs));
}

QJSIntlStatus qjs_intl_date_ordinary_programs_open(const QJSIntlDateSourceCandidates *pool,
                                                   const QJSIntlDateFormatSources *groups,
                                                   const QJSIntlDateAvailableSources *append,
                                                   const QJSIntlDateCombinationData *connectors,
                                                   uint32_t locale, uint32_t calendar,
                                                   QJSIntlBytes decimal,
                                                   const QJSIntlAllocator *allocator,
                                                   QJSIntlDateOrdinaryPrograms *out)
{
    static const unsigned int required[] = {
        BIT(MONTH), BIT(MONTH) | BIT(DAY), BIT(YEAR) | BIT(MONTH), DATE_MASK,
        BIT(WEEKDAY) | DATE_MASK, BIT(HOUR) | BIT(MINUTE), CLOCK_MASK,
        CLOCK_MASK | BIT(FRACTION), BIT(DAY_PERIOD) | BIT(HOUR),
        BIT(DAY_PERIOD) | BIT(HOUR) | BIT(MINUTE), BIT(DAY_PERIOD) | CLOCK_MASK,
        BIT(DAY_PERIOD) | CLOCK_MASK | BIT(FRACTION), BIT(WEEKDAY) | DATE_MASK | CLOCK_MASK,
        BIT(WEEKDAY) | DATE_MASK | CLOCK_MASK | BIT(FRACTION)
    };
    Build build = {0};
    QJSIntlFormatBuilder check = {0};
    QJSIntlStatus status;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !pool || !groups || !append || !connectors || !allocator ||
        !allocator->malloc || !allocator->free || pool->count != groups->source_count ||
        !decimal.data || !decimal.length)
        return QJS_INTL_INVALID_ARGUMENT;
    check.limit = SIZE_MAX;
    status = qjs_intl_format_builder_text(&check, decimal, NULL, NULL);
    if (status)
        return status;
    build.result.allocator = *allocator;
    build.pool = pool;
    build.append = append;
    build.connectors = connectors;
    build.locale = locale;
    build.calendar = calendar;
    build.decimal = decimal;
    if (groups->count > SIZE_MAX / sizeof(*build.result.records) - 32)
        return QJS_INTL_OVERFLOW;
    /* Fourteen required targets, with up to two additional widths for each
       of the four fractional targets, fit within these 32 extra records. */
    build.capacity = groups->count + 32;
    build.result.records = allocator->malloc(allocator->opaque, build.capacity * sizeof(*build.result.records));
    if (!build.result.records)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i < groups->count; i++) {
        QJSIntlDateOrdinaryProgram record;
        initialize(&build, &record);
        status = qjs_intl_date_single_programs_open(pool, groups, i, allocator, &record.single);
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (!status)
            status = add(&build, &record);
        if (status)
            goto fail;
    }
    for (i = 0; i < sizeof(required) / sizeof(*required); i++) {
        status = ensure(&build, required[i]);
        if (status)
            goto fail;
        if (find(&build, required[i]) == SIZE_MAX) {
            status = QJS_INTL_DATA_ERROR;
            goto fail;
        }
    }
    *out = build.result;
    return QJS_INTL_OK;
fail:
    qjs_intl_date_ordinary_programs_clear(&build.result);
    return status;
}
