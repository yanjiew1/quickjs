/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "date-native-bank.h"
#include "temporal/civil.h"
#include <string.h>

#define FIELD(i) (1u << (i))
#define DATE_FIELDS (FIELD(QJS_DATE_WEEKDAY) | FIELD(QJS_DATE_YEAR) | \
                     FIELD(QJS_DATE_MONTH) | FIELD(QJS_DATE_DAY))
#define TIME_FIELDS (FIELD(QJS_DATE_DAY_PERIOD) | FIELD(QJS_DATE_HOUR) | \
                     FIELD(QJS_DATE_MINUTE) | FIELD(QJS_DATE_SECOND) | \
                     FIELD(QJS_DATE_FRACTION))
#define ANY_FIELDS (DATE_FIELDS | TIME_FIELDS)
#define ALL_FIELDS ((1u << QJS_DATE_FIELD_COUNT) - 1)
struct QJSIntlNativeDateBank {
    QJSIntlAllocator allocator;
    QJSIntlNativeDate *slots[QJS_DATE_VALUE_BANK_COUNT];
    QJSIntlBytes calendar; /* borrowed from the mandatory Number slot */
};
static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length ||
        (a.data && b.data && !memcmp(a.data, b.data, a.length)));
}
static unsigned int required_fields(QJSIntlDateRequired required)
{
    switch (required) {
    case QJS_DATE_REQUIRE_DATE: return DATE_FIELDS;
    case QJS_DATE_REQUIRE_TIME: return TIME_FIELDS;
    case QJS_DATE_REQUIRE_YEAR_MONTH: return FIELD(QJS_DATE_YEAR) | FIELD(QJS_DATE_MONTH);
    case QJS_DATE_REQUIRE_MONTH_DAY: return FIELD(QJS_DATE_MONTH) | FIELD(QJS_DATE_DAY);
    case QJS_DATE_REQUIRE_ANY: return ANY_FIELDS;
    default: return 0;
    }
}
static unsigned int default_fields(QJSIntlDateDefaults defaults)
{
    switch (defaults) {
    case QJS_DATE_DEFAULT_DATE: return FIELD(QJS_DATE_YEAR) | FIELD(QJS_DATE_MONTH) | FIELD(QJS_DATE_DAY);
    case QJS_DATE_DEFAULT_TIME: return FIELD(QJS_DATE_HOUR) | FIELD(QJS_DATE_MINUTE) | FIELD(QJS_DATE_SECOND);
    case QJS_DATE_DEFAULT_YEAR_MONTH: return FIELD(QJS_DATE_YEAR) | FIELD(QJS_DATE_MONTH);
    case QJS_DATE_DEFAULT_MONTH_DAY: return FIELD(QJS_DATE_MONTH) | FIELD(QJS_DATE_DAY);
    case QJS_DATE_DEFAULT_ALL: case QJS_DATE_DEFAULT_ZONED_DATE_TIME:
        return default_fields(QJS_DATE_DEFAULT_DATE) | default_fields(QJS_DATE_DEFAULT_TIME);
    default: return 0;
    }
}
/* Literal GetDateTimeFormat record preparation; no observable property read.
 * Era and timeZoneName alone do not make anyPresent true.
 */
static int format_options(QJSIntlDateOptions *out, const QJSIntlDateOptions *request,
    QJSIntlDateRequired required, QJSIntlDateDefaults defaults, int inherit_all)
{
    unsigned int needed = required_fields(required), add = default_fields(defaults);
    int i, any = 0, present = 0;
    *out = *request;
    out->date_style = out->time_style = -1;
    for (i = 0; i < QJS_DATE_FIELD_COUNT; i++) {
        if ((ANY_FIELDS & FIELD(i)) && request->fields[i] >= 0) any = 1;
        if ((needed & FIELD(i)) && request->fields[i] >= 0) present = 1;
        if (!inherit_all && !(needed & FIELD(i)) &&
            !(i == QJS_DATE_ERA && (required == QJS_DATE_REQUIRE_DATE ||
              required == QJS_DATE_REQUIRE_YEAR_MONTH || required == QJS_DATE_REQUIRE_ANY)))
            out->fields[i] = -1;
    }
    if (!present) {
        if (any && !inherit_all) return 0;
        for (i = 0; i < QJS_DATE_FIELD_COUNT; i++)
            if (add & FIELD(i)) out->fields[i] = QJS_DATE_NUMERIC;
        if (defaults == QJS_DATE_DEFAULT_ZONED_DATE_TIME && out->fields[QJS_DATE_ZONE] < 0)
            out->fields[QJS_DATE_ZONE] = QJS_DATE_ZONE_SHORT;
    }
    return 1;
}
static QJSIntlDateRequired kind_required(QJSIntlDateValueKind kind)
{
    switch (kind) {
    case QJS_DATE_VALUE_PLAIN_DATE: return QJS_DATE_REQUIRE_DATE;
    case QJS_DATE_VALUE_PLAIN_YEAR_MONTH: return QJS_DATE_REQUIRE_YEAR_MONTH;
    case QJS_DATE_VALUE_PLAIN_MONTH_DAY: return QJS_DATE_REQUIRE_MONTH_DAY;
    case QJS_DATE_VALUE_PLAIN_TIME: return QJS_DATE_REQUIRE_TIME;
    default: return QJS_DATE_REQUIRE_ANY;
    }
}
static QJSIntlDateDefaults kind_defaults(QJSIntlDateValueKind kind)
{
    switch (kind) {
    case QJS_DATE_VALUE_PLAIN_DATE: return QJS_DATE_DEFAULT_DATE;
    case QJS_DATE_VALUE_PLAIN_YEAR_MONTH: return QJS_DATE_DEFAULT_YEAR_MONTH;
    case QJS_DATE_VALUE_PLAIN_MONTH_DAY: return QJS_DATE_DEFAULT_MONTH_DAY;
    case QJS_DATE_VALUE_PLAIN_TIME: return QJS_DATE_DEFAULT_TIME;
    default: return QJS_DATE_DEFAULT_ALL;
    }
}
void qjs_intl_native_date_bank_close(QJSIntlNativeDateBank *bank)
{
    QJSIntlAllocator a;
    unsigned int i, j;
    if (!bank) return;
    a = bank->allocator;
    for (i = 0; i < QJS_DATE_VALUE_BANK_COUNT; i++) {
        QJSIntlNativeDate *handle = bank->slots[i];
        if (!handle) continue;
        /* Clear aliases before releasing the unique handle. */
        for (j = i + 1; j < QJS_DATE_VALUE_BANK_COUNT; j++)
            if (bank->slots[j] == handle) bank->slots[j] = NULL;
        bank->slots[i] = NULL;
        qjs_intl_native_date_close(handle);
    }
    a.free(a.opaque, bank);
}
QJSIntlStatus qjs_intl_native_date_bank_open(const QJSIntlAllocator *a,
    const QJSIntlDateData *data, const QJSIntlDateBankOptions *options,
    const QJSIntlDateEnvironment *environment, QJSIntlNativeDateBank **out)
{
    QJSIntlNativeDateBank *bank;
    QJSIntlDateOptions selected;
    QJSIntlStatus r;
    int base_fields[QJS_DATE_FIELD_COUNT], i, conflicting;
    unsigned int allowed;
    QJSIntlDateValueKind kind;
    int styles;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !a->malloc || !a->free || !data || !options ||
        options->requested.date_style < -1 || options->requested.date_style > 3 ||
        options->requested.time_style < -1 || options->requested.time_style > 3 ||
        !required_fields(options->number_required) || !default_fields(options->number_defaults) ||
        (options->instant_defaults != QJS_DATE_DEFAULT_ALL &&
         options->instant_defaults != QJS_DATE_DEFAULT_ZONED_DATE_TIME))
        return QJS_INTL_INVALID_ARGUMENT;
    bank = a->malloc(a->opaque, sizeof(*bank));
    if (!bank) return QJS_INTL_NO_MEMORY;
    memset(bank, 0, sizeof(*bank)); bank->allocator = *a;
    styles = options->requested.date_style >= 0 || options->requested.time_style >= 0;
    selected = options->requested;
    if (!styles) (void)format_options(&selected, &options->requested,
        options->number_required, options->number_defaults, 1);
    r = qjs_intl_native_date_open(a, data, &selected, environment,
                                 &bank->slots[QJS_DATE_VALUE_NUMBER]);
    if (r) goto fail;
    bank->calendar = qjs_intl_native_date_calendar(bank->slots[QJS_DATE_VALUE_NUMBER]);
    qjs_intl_native_date_resolved_fields(bank->slots[QJS_DATE_VALUE_NUMBER], base_fields);
    for (kind = QJS_DATE_VALUE_PLAIN_DATE; kind < QJS_DATE_VALUE_BANK_COUNT; kind++) {
        if (styles) {
            if ((kind <= QJS_DATE_VALUE_PLAIN_MONTH_DAY && options->requested.date_style < 0) ||
                (kind == QJS_DATE_VALUE_PLAIN_TIME && options->requested.time_style < 0)) continue;
            allowed = kind == QJS_DATE_VALUE_INSTANT ? ALL_FIELDS : required_fields(kind_required(kind));
            if (kind == QJS_DATE_VALUE_PLAIN_DATE || kind == QJS_DATE_VALUE_PLAIN_YEAR_MONTH ||
                kind == QJS_DATE_VALUE_PLAIN_DATE_TIME) allowed |= FIELD(QJS_DATE_ERA);
            conflicting = 0;
            for (i = 0; i < QJS_DATE_FIELD_COUNT; i++)
                if (base_fields[i] >= 0 && !(allowed & FIELD(i))) conflicting = 1;
            if (!conflicting) {
                /* AdjustDateTimeStyleFormat returns the exact base record. */
                bank->slots[kind] = bank->slots[QJS_DATE_VALUE_NUMBER]; continue;
            }
            selected = options->requested;
            selected.date_style = selected.time_style = -1;
            for (i = 0; i < QJS_DATE_FIELD_COUNT; i++)
                selected.fields[i] = allowed & FIELD(i) ? base_fields[i] : -1;
        } else if (!format_options(&selected, &options->requested, kind_required(kind),
                    kind == QJS_DATE_VALUE_INSTANT ? options->instant_defaults : kind_defaults(kind),
                    kind == QJS_DATE_VALUE_INSTANT)) continue;
        r = qjs_intl_native_date_open(a, data, &selected, environment, &bank->slots[kind]);
        if (r) goto fail;
    }
    *out = bank; return QJS_INTL_OK;
 fail:
    qjs_intl_native_date_bank_close(bank); return r;
}
const QJSIntlNativeDate *qjs_intl_native_date_bank_slot(
    const QJSIntlNativeDateBank *bank, QJSIntlDateValueKind kind)
{
    return bank && (unsigned int)kind < QJS_DATE_VALUE_BANK_COUNT ? bank->slots[kind] : NULL;
}
static QJSIntlStatus fault(QJSIntlDateValueFault *out, QJSIntlDateValueFault value)
{
    *out = value; return QJS_INTL_INVALID_ARGUMENT;
}
QJSIntlStatus qjs_intl_native_date_bank_handle(const QJSIntlNativeDateBank *bank,
    const QJSIntlDateValue *value, QJSIntlDateValueFormat *out, QJSIntlDateValueFault *error)
{
    QJSIntlDateValueFormat result;
    QJSTemporalISODateTime iso;
    QJSIntlBytes iso_calendar = { "iso8601", 7 };
    QJSIntlDateValueKind kind;
    if (out) memset(out, 0, sizeof(*out));
    if (error) *error = QJS_DATE_FAULT_NONE;
    if (!bank || !value || !out || !error ||
        (unsigned int)value->kind > QJS_DATE_VALUE_ZONED_DATE_TIME)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(&result, 0, sizeof(result)); kind = value->kind;
    if (kind == QJS_DATE_VALUE_ZONED_DATE_TIME)
        return fault(error, QJS_DATE_FAULT_INAPPLICABLE_TYPE);
    if (kind == QJS_DATE_VALUE_NUMBER) {
        if (value->value.number_ms < -INT64_C(8640000000000000) ||
            value->value.number_ms > INT64_C(8640000000000000)) return QJS_INTL_INVALID_ARGUMENT;
        result.epoch = qjs_temporal_epoch_ns_from_int64(value->value.number_ms);
        if (qjs_temporal_epoch_ns_multiply(&result.epoch, result.epoch, 1000000))
            return QJS_INTL_OVERFLOW;
    } else if (kind == QJS_DATE_VALUE_INSTANT) {
        if (!qjs_temporal_epoch_ns_is_valid(value->value.instant)) return QJS_INTL_INVALID_ARGUMENT;
        result.epoch = value->value.instant;
    } else {
        if (kind != QJS_DATE_VALUE_PLAIN_TIME && !equal(value->calendar, bank->calendar) &&
            !((kind == QJS_DATE_VALUE_PLAIN_DATE || kind == QJS_DATE_VALUE_PLAIN_DATE_TIME) &&
              equal(value->calendar, iso_calendar)))
            return fault(error, QJS_DATE_FAULT_CALENDAR_MISMATCH);
        iso.time = (QJSTemporalISOTime){ 12, 0, 0, 0, 0, 0 };
        if (kind == QJS_DATE_VALUE_PLAIN_DATE_TIME) {
            iso = value->value.datetime;
            if (!qjs_temporal_iso_datetime_within_limits(iso)) return QJS_INTL_INVALID_ARGUMENT;
        } else if (kind == QJS_DATE_VALUE_PLAIN_TIME) {
            iso.date = (QJSTemporalISODate){ 1970, 1, 1 }; iso.time = value->value.time;
        } else {
            iso.date = value->value.date;
            if (kind == QJS_DATE_VALUE_PLAIN_YEAR_MONTH ?
                !qjs_temporal_iso_year_month_within_limits(iso.date) :
                !qjs_temporal_iso_date_within_limits(iso.date)) return QJS_INTL_INVALID_ARGUMENT;
        }
        if (qjs_temporal_iso_datetime_to_epoch_ns(&result.epoch, iso)) return QJS_INTL_INVALID_ARGUMENT;
        result.is_plain = 1;
    }
    result.format = bank->slots[kind];
    if (!result.format) return fault(error, QJS_DATE_FAULT_INAPPLICABLE_TYPE);
    *out = result; return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_date_bank_format(const QJSIntlNativeDateBank *bank,
    const QJSIntlDateValue *value, QJSIntlFormatted *out, QJSIntlDateValueFault *error)
{
    QJSIntlDateValueFormat record;
    QJSIntlStatus r;
    if (out) memset(out, 0, sizeof(*out));
    if (error) *error = QJS_DATE_FAULT_NONE;
    if (!out || !error) return QJS_INTL_INVALID_ARGUMENT;
    r = qjs_intl_native_date_bank_handle(bank, value, &record, error);
    if (r) return r;
    return qjs_intl_native_date_format_ns(record.format, record.epoch, record.is_plain, out);
}
QJSIntlStatus qjs_intl_native_date_bank_range(const QJSIntlNativeDateBank *bank,
    const QJSIntlDateValue *start, const QJSIntlDateValue *end,
    QJSIntlFormatted *out, QJSIntlDateValueFault *error)
{
    QJSIntlDateValueFormat first, second;
    QJSIntlStatus r;
    if (out) memset(out, 0, sizeof(*out));
    if (error) *error = QJS_DATE_FAULT_NONE;
    if (!bank || !start || !end || !out || !error ||
        (unsigned int)start->kind > QJS_DATE_VALUE_ZONED_DATE_TIME ||
        (unsigned int)end->kind > QJS_DATE_VALUE_ZONED_DATE_TIME)
        return QJS_INTL_INVALID_ARGUMENT;
    if (start->kind != end->kind) return fault(error, QJS_DATE_FAULT_MIXED_TYPE);
    if ((r = qjs_intl_native_date_bank_handle(bank, start, &first, error)) ||
        (r = qjs_intl_native_date_bank_handle(bank, end, &second, error))) return r;
    if (first.format != second.format || first.is_plain != second.is_plain)
        return QJS_INTL_DATA_ERROR;
    return qjs_intl_native_date_range_ns(first.format, first.epoch, second.epoch,
                                        first.is_plain, out);
}
