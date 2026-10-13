/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "date-native-calendar.h"
#include <string.h>

static int era_is(const QJSCalendarDate *date, const char *era)
{
    size_t n = strlen(era);
    return n < sizeof(date->era) && !memcmp(date->era, era, n) && !date->era[n];
}
QJSIntlStatus qjs_intl_native_date_calendar_names(void *opaque, QJSCalendarId id,
    const QJSCalendarDate *date, unsigned int *era, unsigned int *month)
{
    unsigned int e = 0, m, code;
    size_t length;
    (void)opaque;
    if (!date || !era || !month || id < QJS_CAL_ISO8601 || id >= QJS_CAL_COUNT ||
        date->month < 1 || date->month > 13 ||
        (date->has_era != 0 && date->has_era != 1) ||
        (date->in_leap_year != 0 && date->in_leap_year != 1)) return QJS_INTL_INVALID_ARGUMENT;
    for (length = 0; length < sizeof(date->month_code) && date->month_code[length]; length++) {}
    if ((length != 3 && length != 4) || date->month_code[0] != 'M' ||
        date->month_code[1] < '0' || date->month_code[1] > '1' ||
        date->month_code[2] < '0' || date->month_code[2] > '9' ||
        (length == 4 && date->month_code[3] != 'L')) return QJS_INTL_DATA_ERROR;
    code = (unsigned int)(date->month_code[1] - '0') * 10 + (unsigned int)(date->month_code[2] - '0');
    if (!code || code > 13) return QJS_INTL_DATA_ERROR;
    m = code;
    if (id == QJS_CAL_HEBREW) {
        if (code > 12 || (length == 4 && (code != 5 || !date->in_leap_year))) return QJS_INTL_DATA_ERROR;
        if (length == 4) m = 6;
        else if (code == 6) m = date->in_leap_year ? 14 : 7;
        else if (code > 6) m = code + 1;
    } else if (id == QJS_CAL_CHINESE || id == QJS_CAL_DANGI) {
        if (code > 12 || date->has_era) return QJS_INTL_DATA_ERROR;
    } else if (length == 4 || code != (unsigned int)date->month) return QJS_INTL_DATA_ERROR;
    if (id != QJS_CAL_ISO8601 && id != QJS_CAL_CHINESE && id != QJS_CAL_DANGI && !date->has_era)
        return QJS_INTL_DATA_ERROR;
    switch (id) {
    case QJS_CAL_ISO8601: e = date->year > 0; break;
    case QJS_CAL_GREGORY:
        if (!era_is(date, "bce") && !era_is(date, "ce")) return QJS_INTL_DATA_ERROR;
        e = era_is(date, "ce"); break;
    case QJS_CAL_BUDDHIST: if (!era_is(date, "be")) return QJS_INTL_DATA_ERROR; break;
    case QJS_CAL_COPTIC:
        if (!era_is(date, "am")) return QJS_INTL_DATA_ERROR;
        e = 1; break;
    case QJS_CAL_ETHIOAA: if (!era_is(date, "aa")) return QJS_INTL_DATA_ERROR; break;
    case QJS_CAL_ETHIOPIC:
        if (!era_is(date, "aa") && !era_is(date, "am")) return QJS_INTL_DATA_ERROR;
        e = era_is(date, "am"); break;
    case QJS_CAL_HEBREW: if (!era_is(date, "am")) return QJS_INTL_DATA_ERROR; break;
    case QJS_CAL_INDIAN: if (!era_is(date, "shaka")) return QJS_INTL_DATA_ERROR; break;
    case QJS_CAL_ISLAMIC_CIVIL: case QJS_CAL_ISLAMIC_TBLA: case QJS_CAL_ISLAMIC_UMALQURA:
        if (!era_is(date, "ah") && !era_is(date, "bh")) return QJS_INTL_DATA_ERROR;
        e = era_is(date, "bh"); break;
    case QJS_CAL_PERSIAN: if (!era_is(date, "ap")) return QJS_INTL_DATA_ERROR; break;
    case QJS_CAL_ROC:
        if (!era_is(date, "broc") && !era_is(date, "roc")) return QJS_INTL_DATA_ERROR;
        e = era_is(date, "roc"); break;
    case QJS_CAL_JAPANESE:
        if (era_is(date, "meiji")) e = 232;
        else if (era_is(date, "taisho")) e = 233;
        else if (era_is(date, "showa")) e = 234;
        else if (era_is(date, "heisei")) e = 235;
        else if (era_is(date, "reiwa")) e = 236;
        else if (era_is(date, "bce")) e = 237;
        else if (era_is(date, "ce")) e = 238;
        else return QJS_INTL_DATA_ERROR;
        break;
    case QJS_CAL_CHINESE: case QJS_CAL_DANGI: break;
    default: return QJS_INTL_INVALID_ARGUMENT;
    }
    *era = e; *month = m; return QJS_INTL_OK;
}

static int supported(void *opaque, QJSIntlBytes identifier)
{
    QJSIntlDateCalendarBridge *bridge = opaque;
    QJSCalendarId id;
    return !qjs_calendar_from_identifier(&id, identifier.data, identifier.length) &&
        qjs_calendar_is_supported(id) &&
        bridge->supports_data(bridge->names_opaque, id);
}
static QJSIntlStatus zone(void *opaque, QJSIntlBytes identifier, int64_t seconds,
                          QJSIntlDateZoneInfo *out)
{
    QJSIntlDateCalendarBridge *bridge = opaque;
    return bridge->upstream.zone ? bridge->upstream.zone(bridge->upstream.opaque,
        identifier, seconds, out) : QJS_INTL_UNSUPPORTED;
}
static QJSIntlStatus calendar(void *opaque, QJSIntlBytes identifier,
    const QJSTemporalISODateTime *iso, QJSIntlDateFields *out)
{
    QJSIntlDateCalendarBridge *bridge = opaque;
    QJSCalendarId id;
    QJSCalendarDate date;
    QJSIntlDateFields fields;
    int64_t epoch_day, year;
    int status;
    QJSIntlStatus result;
    if (!iso || !out || !supported(opaque, identifier)) return QJS_INTL_UNSUPPORTED;
    if (qjs_calendar_from_identifier(&id, identifier.data, identifier.length) ||
        qjs_temporal_iso_date_to_days(&epoch_day, iso->date)) return QJS_INTL_DATA_ERROR;
    status = qjs_calendar_from_epoch_day_for_intl(id, epoch_day, &date);
    if (status == QJS_CAL_RANGE) return QJS_INTL_OVERFLOW;
    if (status == QJS_CAL_UNSUPPORTED) return QJS_INTL_UNSUPPORTED;
    if (status) return QJS_INTL_DATA_ERROR;
    memset(&fields, 0, sizeof(fields));
    result = bridge->resolve_names(bridge->names_opaque, id, &date,
                                   &fields.era, &fields.month_name_index);
    if (result) return result;
    /* Shared arithmetic owns era boundaries and eraYear. Only the ECMA402
     * numeric year presentation transform belongs to this formatter bridge.
     */
    year = date.has_era ? date.era_year : date.year;
    fields.year = year > 0 ? year : 1 - year;
    fields.calendar_year = date.year; fields.has_calendar_year = 1;
    fields.month = (unsigned int)date.month; fields.day = (unsigned int)date.day;
    if (id == QJS_CAL_CHINESE || id == QJS_CAL_DANGI) {
        int64_t cycle = ((int64_t)date.year - 4) % 60;
        if (cycle < 0) cycle += 60;
        fields.year_name_index = (unsigned int)cycle + 1;
        fields.year = fields.year_name_index;
        fields.related_year = date.year; fields.has_related_year = 1;
        fields.month = fields.month_name_index;
        fields.leap_month = date.month_code[3] == 'L';
    }
    fields.hour = (unsigned int)iso->time.hour; fields.minute = (unsigned int)iso->time.minute;
    fields.second = (unsigned int)iso->time.second;
    fields.millisecond = (unsigned int)iso->time.millisecond;
    *out = fields; return QJS_INTL_OK;
}
static QJSIntlStatus zone_name_stable(void *opaque, QJSIntlBytes name,
    int64_t from, int64_t through, int *proven)
{
    QJSIntlDateCalendarBridge *bridge = opaque;
    *proven = 0;
    return bridge->upstream.zone_name_stable ?
        bridge->upstream.zone_name_stable(bridge->upstream.opaque, name, from, through, proven) : QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_date_calendar_environment(
    QJSIntlDateCalendarBridge *bridge, QJSIntlDateEnvironment *out)
{
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!bridge || !bridge->supports_data || !bridge->resolve_names)
        return QJS_INTL_INVALID_ARGUMENT;
    out->opaque = bridge; out->zone = zone;
    out->zone_name_stable = zone_name_stable;
    out->calendar_supported = supported; out->calendar = calendar;
    return QJS_INTL_OK;
}
