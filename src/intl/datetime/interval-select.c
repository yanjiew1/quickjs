/* Select actual interval source programs; no JS coercion or endpoint rendering.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "interval-select.h"
#include "basic-match.h"
#include <limits.h>
#include <string.h>

QJSIntlDateRangeField qjs_intl_date_range_last_field(const QJSIntlDateTimeComponents *components)
{
    static const int levels[QJS_INTL_DATETIME_COMPONENT_COUNT] = {
        QJS_INTL_DATE_RANGE_DAY, QJS_INTL_DATE_RANGE_ERA, QJS_INTL_DATE_RANGE_YEAR,
        QJS_INTL_DATE_RANGE_MONTH, QJS_INTL_DATE_RANGE_DAY, QJS_INTL_DATE_RANGE_DAY_PERIOD,
        QJS_INTL_DATE_RANGE_HOUR, QJS_INTL_DATE_RANGE_MINUTE, QJS_INTL_DATE_RANGE_SECOND,
        QJS_INTL_DATE_RANGE_FRACTION, -1,
    };
    int finest = -1;
    size_t i;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if (components->values[i] && levels[i] > finest)
            finest = levels[i];
    return finest < 0 ? QJS_INTL_DATE_RANGE_FRACTION : finest;
}

static QJSIntlStatus skeleton_info(QJSIntlBytes source, QJSIntlDatePatternInfo *out)
{
    QJSIntlDatePatternToken tokens[QJS_INTL_DATETIME_COMPONENT_COUNT + 2] = {0};
    QJSIntlDatePattern view = {0};
    size_t at = 0;
    view.tokens = tokens;
    while (at < source.length) {
        unsigned int symbol = (unsigned char)source.data[at];
        size_t first = at++;
        if (view.count == sizeof(tokens) / sizeof(*tokens) ||
            !((symbol >= 'A' && symbol <= 'Z') || (symbol >= 'a' && symbol <= 'z')))
            return QJS_INTL_UNSUPPORTED;
        while (at < source.length && (unsigned char)source.data[at] == symbol)
            at++;
        tokens[view.count].symbol = symbol;
        tokens[view.count++].width = at - first;
    }
    return view.count ? qjs_intl_date_pattern_info(&view, out) : QJS_INTL_UNSUPPORTED;
}

static unsigned int component_mask(const QJSIntlDateTimeComponents *components)
{
    unsigned int result = 0;
    size_t i;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if (components->values[i])
            result |= 1U << i;
    return result;
}

static QJSIntlStatus resolve(const QJSIntlDateIntervalData *data, uint32_t head,
                             char symbol, QJSIntlDateIntervalEntry *out)
{
    QJSIntlBytes code = {&symbol, 1};
    while (head) {
        QJSIntlDateIntervalNode node;
        QJSIntlStatus status = qjs_intl_date_interval_lookup(data, head, code, (QJSIntlBytes){0}, out);
        if (!status)
            return out->blocked || !out->text.length ? QJS_INTL_UNSUPPORTED : QJS_INTL_OK;
        if (status != QJS_INTL_UNSUPPORTED)
            return status;
        status = qjs_intl_date_interval_node(data, head, &node);
        if (status)
            return status;
        head = node.parent;
    }
    return QJS_INTL_UNSUPPORTED;
}

static QJSIntlStatus admit(const QJSIntlDateIntervalProgram *program, QJSCalendarId calendar,
                           const QJSIntlDateTextData *names, QJSIntlDateNameAdmission *admission)
{
    QJSIntlDatePattern view = {0};
    QJSIntlStatus status;
    view.tokens = program->pattern.tokens;
    view.count = program->split;
    status = qjs_intl_date_names_admit(&view, calendar, names, admission);
    if (!status) {
        view.tokens += program->split;
        view.count = program->pattern.count - program->split;
        status = qjs_intl_date_names_admit(&view, calendar, names, admission);
    }
    return status;
}

static int compatible_periods(const QJSIntlDateIntervalProgram *program,
                               const QJSIntlDatePatternInfo *ordinary)
{
    size_t i;
    for (i = 0; i < program->pattern.count; i++) {
        unsigned int symbol = program->pattern.tokens[i].symbol;
        if ((symbol == 'a' || symbol == 'b' || symbol == 'B') && symbol != ordinary->period_symbol)
            return 0;
    }
    return 1;
}

static int represents_difference(const QJSIntlDateIntervalProgram *program, char code)
{
    unsigned int seen = 0, years = 0, first_years = 0, last_years = 0;
    size_t i;
    for (i = 0; i < program->pattern.count; i++) {
        unsigned int symbol = program->pattern.tokens[i].symbol;
        unsigned int year = symbol == 'y' ? QJS_INTL_DATE_YEAR_CALENDAR :
                            symbol == 'r' ? QJS_INTL_DATE_YEAR_RELATED :
                            symbol == 'U' ? QJS_INTL_DATE_YEAR_CYCLIC : 0;
        int matches;
        years |= year;
        if (program->sources[i] == QJS_INTL_RANGE_START)
            first_years |= year;
        else if (program->sources[i] == QJS_INTL_RANGE_END)
            last_years |= year;
        switch (code) {
        case 'y': matches = symbol == 'y' || symbol == 'r' || symbol == 'U'; break;
        case 'M': matches = symbol == 'M' || symbol == 'L'; break;
        case 'H': case 'h':
            matches = symbol == 'H' || symbol == 'h' || symbol == 'K' || symbol == 'k';
            break;
        case 'a': case 'B': matches = symbol == 'a' || symbol == 'b' || symbol == 'B'; break;
        default: matches = symbol == (unsigned char)code; break;
        }
        if (matches)
            seen |= 1U << program->sources[i];
    }
    /* A repeated cyclic name does not prove that a singly occurring related
       year is shared. On a year difference expose each raw year at both ends. */
    if (code == 'y' && (years & ~(first_years & last_years)))
        return 0;
    return (seen & ((1U << QJS_INTL_RANGE_START) | (1U << QJS_INTL_RANGE_END))) ==
                  ((1U << QJS_INTL_RANGE_START) | (1U << QJS_INTL_RANGE_END));
}

void qjs_intl_date_interval_set_clear(QJSIntlDateIntervalSet *set)
{
    size_t i;
    if (!set)
        return;
    for (i = 0; i < set->count; i++)
        qjs_intl_date_interval_program_clear(&set->records[i]);
    if (set->records)
        set->allocator.free(set->allocator.opaque, set->records);
    memset(set, 0, sizeof(*set));
}

const QJSIntlDateIntervalProgram *qjs_intl_date_interval_set_get(const QJSIntlDateIntervalSet *set,
                                                               QJSIntlDateRangeField field)
{
    size_t index;
    if (!set || (unsigned int)field >= QJS_INTL_DATE_RANGE_FIELD_COUNT)
        return NULL;
    index = set->indexes[field];
    return index && index <= set->count ? &set->records[index - 1] : NULL;
}

QJSIntlStatus qjs_intl_date_interval_set_open(const QJSIntlDateIntervalData *data,
                                             uint32_t locale, uint32_t calendar,
                                             const QJSIntlDatePattern *ordinary,
                                             QJSIntlDateHourCycle cycle,
                                             QJSCalendarId arithmetic_calendar,
                                             const QJSIntlDateTextData *names,
                                             QJSIntlDateNameAdmission *admission,
                                             const QJSIntlAllocator *allocator,
                                             QJSIntlDateIntervalSet *out)
{
    static const struct { char symbol; QJSIntlDateRangeField field; } codes[] = {
        {'G', QJS_INTL_DATE_RANGE_ERA}, {'y', QJS_INTL_DATE_RANGE_YEAR},
        {'M', QJS_INTL_DATE_RANGE_MONTH}, {'d', QJS_INTL_DATE_RANGE_DAY},
        {'a', QJS_INTL_DATE_RANGE_AM_PM}, {'B', QJS_INTL_DATE_RANGE_DAY_PERIOD},
        {'H', QJS_INTL_DATE_RANGE_HOUR}, {'h', QJS_INTL_DATE_RANGE_HOUR},
        {'m', QJS_INTL_DATE_RANGE_MINUTE},
    };
    QJSIntlDateIntervalProgram selected[QJS_INTL_DATE_RANGE_FIELD_COUNT] = {0};
    QJSIntlDateIntervalSet result = {0};
    QJSIntlDatePatternInfo info;
    QJSIntlDateRangeField last_field;
    QJSIntlStatus status;
    int scores[QJS_INTL_DATE_RANGE_FIELD_COUNT];
    unsigned int wanted;
    uint32_t i;
    size_t j;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !names || !admission || !allocator || !allocator->malloc || !allocator->free ||
        locale >= data->section.count || calendar >= data->calendar_count ||
        (unsigned int)cycle > QJS_INTL_DATE_H24 || (unsigned int)arithmetic_calendar >= QJS_CAL_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_pattern_info(ordinary, &info);
    if (status)
        return status;
    wanted = component_mask(&info.components);
    last_field = qjs_intl_date_range_last_field(&info.components);
    result.allocator = *allocator;
    for (j = 0; j < QJS_INTL_DATE_RANGE_FIELD_COUNT; j++)
        scores[j] = INT_MIN;
    /* The suffix catalog is shared by calendars; do not scan other calendars'
       copies, and project skeletons without allocating temporary patterns. */
    for (i = 0; i < data->suffix_count; i++) {
        QJSIntlDateIntervalGroup group;
        QJSIntlDatePatternInfo key;
        uint32_t index = calendar * data->suffix_count + i, head;
        int score;
        status = qjs_intl_date_interval_group_at(data, index, &group);
        if (status)
            goto fail;
        if (group.family != QJS_INTL_DATE_INTERVAL_ITEM || group.alt.length || group.item_alt.length)
            continue;
        status = skeleton_info(group.skeleton, &key);
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            goto fail;
        if (key.hour_symbol && !key.period_symbol &&
            ((key.hour_symbol == 'h' || key.hour_symbol == 'K') != (cycle <= QJS_INTL_DATE_H12)))
            continue;
        status = qjs_intl_datetime_basic_score(&info.components, &key.components, &score);
        if (!status)
            status = qjs_intl_date_interval_head(data, locale, index, &head);
        if (status)
            goto fail;
        if (!head)
            continue;
        for (j = 0; j < sizeof(codes) / sizeof(*codes); j++) {
            QJSIntlDateRangeField field = codes[j].field;
            QJSIntlDateIntervalEntry entry;
            QJSIntlDateIntervalProgram program = {0};
            if (field > last_field || score <= scores[field])
                continue;
            status = resolve(data, head, codes[j].symbol, &entry);
            if (status == QJS_INTL_UNSUPPORTED)
                continue;
            if (!status)
                status = qjs_intl_date_interval_program_open(allocator, entry.text, &info, cycle, &program);
            if (!status && (wanted & ~component_mask(&program.components)))
                status = QJS_INTL_UNSUPPORTED;
            /* A changed field must actually expose both endpoints. Keep the
               ordinary period semantics used by the comparison kernel; adding
               a different flexible-period system could invalidate sharing. */
            if (!status && (!represents_difference(&program, codes[j].symbol) ||
                            !compatible_periods(&program, &info)))
                status = QJS_INTL_UNSUPPORTED;
            if (!status)
                status = admit(&program, arithmetic_calendar, names, admission);
            if (status) {
                qjs_intl_date_interval_program_clear(&program);
                if (status == QJS_INTL_UNSUPPORTED)
                    continue;
                goto fail;
            }
            qjs_intl_date_interval_program_clear(&selected[field]);
            selected[field] = program;
            scores[field] = score;
        }
    }
    for (j = 0; j < QJS_INTL_DATE_RANGE_FIELD_COUNT; j++)
        result.count += selected[j].pattern.count != 0;
    if (result.count) {
        size_t at = 0;
        result.records = allocator->malloc(allocator->opaque, result.count * sizeof(*result.records));
        if (!result.records) {
            status = QJS_INTL_NO_MEMORY;
            goto fail;
        }
        for (j = 0; j < QJS_INTL_DATE_RANGE_FIELD_COUNT; j++) {
            if (!selected[j].pattern.count)
                continue;
            result.records[at] = selected[j];
            result.indexes[j] = ++at;
        }
    }
    *out = result;
    return QJS_INTL_OK;
fail:
    for (j = 0; j < QJS_INTL_DATE_RANGE_FIELD_COUNT; j++)
        qjs_intl_date_interval_program_clear(&selected[j]);
    return status;
}
