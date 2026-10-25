/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "ordinary-programs.h"
#include "append-layout.h"
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
        if (qjs_intl_datetime_component_mask(&build->result.records[i].single.components) == wanted)
            return i;
    return SIZE_MAX;
}

static size_t date_base(const Build *build, unsigned int wanted)
{
    size_t i;
    /* Default date/clock and date/zone combinations prefer a whole numeric
       date. Keep its actual padding in the metadata for final matching. */
    if ((wanted & ~BIT(ERA)) == DATE_MASK) {
        for (i = 0; i < build->result.count; i++) {
            const QJSIntlDateTimeComponents *components = &build->result.records[i].single.components;
            if (qjs_intl_datetime_component_mask(components) == wanted &&
                components->values[QJS_INTL_DATETIME_YEAR] == QJS_INTL_DATETIME_NUMERIC &&
                components->values[QJS_INTL_DATETIME_MONTH] <= QJS_INTL_DATETIME_NUMERIC &&
                components->values[QJS_INTL_DATETIME_DAY] <= QJS_INTL_DATETIME_NUMERIC)
                return i;
        }
    }
    return find(build, wanted);
}

static void initialize(Build *build, QJSIntlDateOrdinaryProgram *record)
{
    memset(record, 0, sizeof(*record));
    record->first = record->second = SIZE_MAX;
    record->single.allocator = build->result.allocator;
    record->single.normal.source_index = record->single.twelve.source_index = SIZE_MAX;
}

typedef struct SourceHints {
    unsigned int month, period;
} SourceHints;

static SourceHints skeleton_hints(QJSIntlBytes skeleton, unsigned int period_symbol)
{
    SourceHints result = {0};
    unsigned int period_seen = 0;
    size_t i = 0;

    while (i < skeleton.length) {
        unsigned int symbol = (unsigned char)skeleton.data[i];
        size_t first = i++;
        if (!symbol || !strchr("GyYuUrQqMLlwWdDFgEecabBhHKkjJCmsSAzZvVOXx", symbol))
            return (SourceHints){0};
        while (i < skeleton.length && (unsigned char)skeleton.data[i] == symbol)
            i++;
        if (symbol == 'M' || symbol == 'L') {
            if (result.month || i - first > 5)
                return (SourceHints){0};
            result.month = i - first <= 2 ? QJS_INTL_DATE_MONTH_SOURCE_NUMERIC : QJS_INTL_DATE_MONTH_SOURCE_TEXT;
        }
        if (symbol == 'b' || symbol == 'B') {
            if (period_seen++ || i - first > 5)
                return (SourceHints){0};
            if (symbol == period_symbol)
                result.period = 1U << (i - first);
        }
    }
    return result;
}

static void include_hints(QJSIntlDateOrdinaryProgram *record, size_t variant,
                           const QJSIntlDateSourceCandidate *source, unsigned int period_symbol)
{
    SourceHints hints = skeleton_hints(source->source.code, period_symbol);
    record->source_month_kinds[variant] |= hints.month;
    record->source_period_widths[variant] |= hints.period;
}

static int same_source_text(const QJSIntlDateSourceCandidate *a, const QJSIntlDateSourceCandidate *b)
{
    return a->source.text.length == b->source.text.length &&
           (!a->source.text.length || a->source.text.data == b->source.text.data ||
            !memcmp(a->source.text.data, b->source.text.data, a->source.text.length));
}

static QJSIntlStatus source_hints(const Build *build, const QJSIntlDateFormatSources *groups,
                                    size_t group, QJSIntlDateOrdinaryProgram *record)
{
    QJSIntlDateTimeComponents ignored;
    size_t count, variant, i;
    QJSIntlStatus status;
    if (!record->single.components.values[QJS_INTL_DATETIME_MONTH] &&
        !record->single.components.values[QJS_INTL_DATETIME_DAY_PERIOD])
        return QJS_INTL_OK;
    status = qjs_intl_date_format_source_group(groups, group, &ignored, &count);
    if (status)
        return status;
    for (variant = 0; variant < 2; variant++) {
        const QJSIntlDateSingleProgram *program = variant ? &record->single.twelve : &record->single.normal;
        const QJSIntlDateSourceCandidate *selected;
        int inside = 0;
        if (!program->pattern.count)
            continue;
        if (program->source_index >= build->pool->count)
            return QJS_INTL_DATA_ERROR;
        selected = &build->pool->records[program->source_index];
        /* Pairing can select a source outside this group. Seed its own hint
           before collecting aliases represented by the selected raw text. */
        include_hints(record, variant, selected, selected->info.period_symbol);
        for (i = 0; i < count; i++) {
            const QJSIntlDateSourceCandidate *alias;
            size_t index;
            status = qjs_intl_date_format_source_member(groups, group, i, &index);
            if (status || index >= build->pool->count)
                return status ? status : QJS_INTL_DATA_ERROR;
            inside |= index == program->source_index;
            alias = &build->pool->records[index];
            if (same_source_text(alias, selected))
                include_hints(record, variant, alias, selected->info.period_symbol);
        }
        if (!inside) {
            /* A representative with a different hour width belongs to another
               group. Include its exact-text aliases too, not only its own key. */
            for (i = 0; i < build->pool->count; i++)
                if (same_source_text(&build->pool->records[i], selected))
                    include_hints(record, variant, &build->pool->records[i], selected->info.period_symbol);
        }
    }
    return QJS_INTL_OK;
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
    if (normal.hour_symbol) {
        size_t parent = record->second != SIZE_MAX ? record->second : record->first;
        if (parent != SIZE_MAX && parent >= build->result.count) {
            status = QJS_INTL_DATA_ERROR;
            goto done;
        }
        /* Keep the actual clock source's padding through fractional insertion
           and date/clock composition, independently of the adapted metadata. */
        for (i = 0; i < 2; i++) {
            const QJSIntlDatePatternInfo *source = i ? &record->single.twelve.source_info :
                                                      &record->single.normal.source_info;
            record->source_hour_width[i] = source->hour_symbol ?
                source->components.values[QJS_INTL_DATETIME_HOUR] :
                parent != SIZE_MAX ? build->result.records[parent].source_hour_width[i] : 0;
        }
    }
    if (normal.components.values[QJS_INTL_DATETIME_DAY_PERIOD] && record->first != SIZE_MAX) {
        size_t parent = record->second != SIZE_MAX ? record->second : record->first;
        if (parent >= build->result.count) {
            status = QJS_INTL_DATA_ERROR;
            goto done;
        }
        /* Periods come from COMBINE's clock operand, unlike month hints. */
        for (i = 0; i < 2; i++)
            record->source_period_widths[i] = build->result.records[parent].source_period_widths[i];
    }
    if (normal.components.values[QJS_INTL_DATETIME_MONTH] && record->first != SIZE_MAX) {
        const QJSIntlDateOrdinaryProgram *parent;
        if (record->first >= build->result.count) {
            status = QJS_INTL_DATA_ERROR;
            goto done;
        }
        parent = &build->result.records[record->first];
        /* COMBINE's first operand is the date; its second is the clock.
           Date-only operands use normal even for a twelve-hour result. */
        for (i = 0; i < 2; i++)
            record->source_month_kinds[i] = parent->source_month_kinds[
                parent->single.components.values[QJS_INTL_DATETIME_HOUR] ? i : 0];
    }
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
    QJSIntlDatePatternInfo info;
    QJSIntlDateAppendLayout layout;
    QJSIntlStatus status = QJS_INTL_UNSUPPORTED;
    initialize(build, &record);
    field.tokens = &token;
    field.count = 1;
    status = qjs_intl_date_pattern_info(&field, &info);
    if (status)
        return status;
    if (qjs_intl_datetime_component_mask(&info.components) &
        qjs_intl_datetime_component_mask(&build->result.records[base].single.components))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_append_layout(build->append, (QJSIntlBytes){key, strlen(key)}, &layout);
    if (status)
        return status;
    status = qjs_intl_date_pattern_join(&build->result.allocator, &layout.pattern,
               &build->result.records[base].single.normal.pattern, &field, &record.single.normal.pattern);
    if (status)
        return status;
    record.append = layout.entry;
    record.origin = layout.origin;
    record.first = base;
    record.single.components = build->result.records[base].single.components;
    /* Derive metadata from the actual field token, including zone style,
       rather than from the append item's key or punctuation. */
    for (size_t component = 0; component < QJS_INTL_DATETIME_COMPONENT_COUNT; component++)
        if (info.components.values[component])
            record.single.components.values[component] = info.components.values[component];
    record.construction = QJS_INTL_DATE_ORDINARY_APPEND;
    return add(build, &record);
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
        unsigned int fields = qjs_intl_datetime_component_mask(&info->components);
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
        record.fraction_decimal = build->decimal;
        status = fractional_pattern(build, &build->result.records[base].single.normal.pattern,
                                     digits, &record.single.normal.pattern);
        if (!status && build->result.records[base].single.twelve.pattern.count)
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
        unsigned int date = wanted & (DATE_MASK | BIT(WEEKDAY) | BIT(ERA)), time = wanted & ~date;
        status = ensure(build, date);
        if (!status)
            status = ensure(build, time);
        if (status)
            return status;
        first = date_base(build, date);
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

static QJSIntlStatus day_period(Build *build, size_t width)
{
    QJSIntlDateOrdinaryProgram record;
    QJSIntlDatePatternInfo info;
    QJSIntlStatus status;
    initialize(build, &record);
    /* TR35 permits B-only skeletons. This native one-field program needs no
       invented layout; format-context names and rules are admitted later. */
    status = qjs_intl_date_pattern_compile(&build->result.allocator,
                 (QJSIntlBytes){"BBBBB", width}, &record.single.normal.pattern);
    if (!status)
        status = qjs_intl_date_pattern_info(&record.single.normal.pattern, &info);
    if (status) {
        qjs_intl_date_single_programs_clear(&record.single);
        return status;
    }
    record.single.components = info.components;
    record.single.normal.source_info = info;
    record.construction = QJS_INTL_DATE_ORDINARY_FIELD;
    return add(build, &record);
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
    static const unsigned int date_clocks[] = {
        DATE_MASK | BIT(HOUR) | BIT(MINUTE), DATE_MASK | CLOCK_MASK,
        DATE_MASK | CLOCK_MASK | BIT(FRACTION),
        BIT(ERA) | DATE_MASK | BIT(HOUR) | BIT(MINUTE), BIT(ERA) | DATE_MASK | CLOCK_MASK,
        BIT(ERA) | DATE_MASK | CLOCK_MASK | BIT(FRACTION)
    };
    static const struct { unsigned int symbol; size_t width; } date_zones[] = {
        {'z', 1}, {'z', 4}, {'O', 1}, {'O', 4}, {'v', 1}, {'v', 4}
    };
    static const size_t period_widths[] = {1, 4, 5};
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
    if (groups->count > SIZE_MAX / sizeof(*build.result.records) - 55)
        return QJS_INTL_OVERFLOW;
    /* Fourteen required targets, with up to two additional widths for each
       of the four fractional targets, fit within 32 extra records. Reserve
       three more for optional minute/second-only fractional patterns and
       eleven for whole date/clock combinations (including an optional era-date),
       six for date-only programs with an appended timezone name, and three
       for single flexible-day-period fields. */
    build.capacity = groups->count + 55;
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
            status = source_hints(&build, groups, i, &record);
        if (!status)
            status = add(&build, &record);
        if (status) {
            qjs_intl_date_single_programs_clear(&record.single);
            goto fail;
        }
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
    /* Required mixed subsets contain a weekday. Also join the genuine date
       and clock programs directly, so ordinary date/time requests need not
       acquire that extra field. Include whole era-bearing dates when available;
       calendars without them remain supported. Reuse the exact connector. */
    for (i = 0; i < sizeof(date_clocks) / sizeof(*date_clocks); i++) {
        status = ensure(&build, date_clocks[i]);
        if (status && status != QJS_INTL_UNSUPPORTED)
            goto fail;
    }
    /* TR35 uses Date-Timezone when the time side contains only a zone. Keep
       that real two-slot layout; do not borrow clock separators or strip fields. */
    for (i = 0; i < sizeof(date_zones) / sizeof(*date_zones); i++) {
        status = append_field(&build, date_base(&build, DATE_MASK), "Date-Timezone",
                                date_zones[i].symbol, date_zones[i].width);
        if (status && status != QJS_INTL_UNSUPPORTED)
            goto fail;
    }
    /* Extend a visible whole minute/second program; do not introduce an hour
       merely because the requested precision includes a fractional second. */
    i = find(&build, BIT(MINUTE) | BIT(SECOND));
    if (i != SIZE_MAX) {
        status = fraction(&build, i);
        if (status)
            goto fail;
    }
    for (i = 0; i < sizeof(period_widths) / sizeof(*period_widths); i++) {
        status = day_period(&build, period_widths[i]);
        if (status)
            goto fail;
    }
    *out = build.result;
    return QJS_INTL_OK;
fail:
    qjs_intl_date_ordinary_programs_clear(&build.result);
    return status;
}
