/* Native DateTimeFormat locale admission. Copyright 2026 Yan-Jie Wang.
 * Complete localized calendars and shared arithmetic are proved per row. */
#include "provider-native-date.h"
#include "metadata-data.h"
#include "native-locale-info.h"
#include <string.h>

static QJSIntlBytes bytes(const char *s)
{
    QJSIntlBytes b = { s, s ? strlen(s) : 0 }; return b;
}
static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length ||
        (a.data && b.data && !memcmp(a.data, b.data, a.length)));
}
static QJSIntlStatus text(const QJSIntlDataView *v,
    const QJSIntlDataSection *s, uint32_t row, uint32_t offset, QJSIntlBytes *out)
{
    QJSIntlDataSlice b;
    if (qjs_intl_data_record_string(v, s, row, offset, &b)) return QJS_INTL_DATA_ERROR;
    out->data = (const char *)b.data; out->length = b.length;
    return QJS_INTL_OK;
}
static QJSIntlStatus numbering_row(QJSIntlProvider *p, QJSIntlBytes value,
                                   uint32_t *out)
{
    const QJSIntlDataView *v = qjs_intl_native_provider_view(p);
    QJSIntlDataSection s; QJSIntlBytes tag; QJSIntlDataSlice record; uint32_t row;
    if (!v || !out) return QJS_INTL_INVALID_ARGUMENT;
    if (qjs_intl_data_section(v, QJS_INTL_DATA_NUMBERING, &s)) return QJS_INTL_DATA_ERROR;
    for (row = 0; row < s.record_count; row++) {
        if (text(v, &s, row, 0, &tag)) return QJS_INTL_DATA_ERROR;
        if (!equal(value, tag)) continue;
        if (qjs_intl_data_record(&s, row, &record)) return QJS_INTL_DATA_ERROR;
        if (record.data[8] != 10 || record.data[9]) return QJS_INTL_UNSUPPORTED;
        *out = row; return QJS_INTL_OK;
    }
    return QJS_INTL_UNSUPPORTED;
}
static QJSIntlStatus append(QJSIntlProvider *p, QJSIntlTagList *out, QJSIntlBytes value)
{
    const QJSIntlAllocator *a = qjs_intl_native_provider_allocator(p);
    QJSIntlBytes *items; char *copy; size_t i;
    for (i = 0; i < out->count; i++) if (equal(out->items[i], value)) return QJS_INTL_OK;
    if (out->count >= SIZE_MAX / sizeof(*items) || value.length == SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    copy = NULL;
    if (value.data) {
        copy = a->malloc(a->opaque, value.length + 1);
        if (!copy) return QJS_INTL_NO_MEMORY;
        if (value.length) memcpy(copy, value.data, value.length);
        copy[value.length] = 0;
    }
    items = a->realloc(a->opaque, out->items, (out->count + 1) * sizeof(*items));
    if (!items) { if (copy) a->free(a->opaque, copy); return QJS_INTL_NO_MEMORY; }
    out->items = items; out->items[out->count++] = (QJSIntlBytes){copy, value.length}; return QJS_INTL_OK;
}
static QJSIntlStatus date_defaults(QJSIntlProvider *, QJSIntlBytes,
    uint32_t, QJSIntlNativeDateDefaults *);
QJSIntlStatus qjs_intl_native_provider_date_hour_cycle(QJSIntlProvider *p,
    QJSIntlBytes locale, int hour12, unsigned int *out)
{
    QJSIntlNativeDateDefaults defaults;
    QJSIntlStatus status;
    uint32_t row;
    if (!out || hour12 < -1 || hour12 > 1) return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_native_provider_locale_row(p, locale, &row);
    if (status) return status;
    status = date_defaults(p, locale, row, &defaults);
    if (status) return status;
    *out = defaults.hour_cycles[hour12 < 0 ? 0 : hour12 ? 1 : 2];
    return QJS_INTL_OK;
}
static int calendar_data_supported(void *opaque, QJSCalendarId id)
{
    QJSIntlNativeDateProviderEnvironment *state = opaque;
    return (unsigned int)id < QJS_CAL_COUNT && qjs_calendar_is_supported(id) &&
        (state->calendar_mask & (UINT32_C(1) << (unsigned int)id));
}
QJSIntlStatus qjs_intl_native_provider_date_open(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlBytes numbering, const QJSIntlDateBankOptions *o,
    QJSTzProvider *tz, QJSIntlNativeDateProviderEnvironment *state,
    QJSIntlNativeDateBank **out)
{
    QJSIntlDateEnvironment env; QJSIntlStatus status; uint32_t l, n; QJSCalendarId calendar;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!p || !o || !state) return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_native_provider_locale_row(p, locale, &l); if (status) return status;
    status = numbering_row(p, numbering, &n); if (status) return status;
    if (qjs_calendar_from_identifier(&calendar, o->requested.calendar.data,
        o->requested.calendar.length)) return QJS_INTL_UNSUPPORTED;
    memset(state, 0, sizeof(*state));
    status = qjs_intl_native_provider_date_calendar_mask(p, l, &state->calendar_mask);
    if (status) return status;
    if (!calendar_data_supported(state, calendar)) return QJS_INTL_UNSUPPORTED;
    state->bridge.upstream.opaque = tz;
    state->bridge.upstream.zone = qjs_intl_native_date_iana_zone;
    state->bridge.upstream.zone_name_stable = qjs_intl_native_date_iana_zone_stable;
    state->bridge.names_opaque = state;
    state->bridge.supports_data = calendar_data_supported;
    state->bridge.resolve_names = qjs_intl_native_date_calendar_names;
    status = qjs_intl_native_date_calendar_environment(&state->bridge, &env);
    if (status) return status;
    return qjs_intl_native_date_bank_open_data(qjs_intl_native_provider_allocator(p),
        qjs_intl_native_provider_view(p), l, n, o, &env, out);
}
static QJSIntlStatus probe_cycle(QJSIntlProvider *p, QJSIntlBytes locale,
                                 QJSIntlBytes nu, int requested_cycle, QJSIntlBytes calendar)
{
    QJSIntlDateBankOptions o; QJSIntlNativeDateBank *bank = NULL;
    QJSIntlNativeDateProviderEnvironment state;
    QJSIntlStatus status; unsigned int hc; size_t i; uint32_t l, n;
    QJSCalendarId id;
    /* Authenticate every identity and resolve defaults before proof lookup.
     * Algorithmic numbering is rejected without allocating. */
    status = qjs_intl_native_provider_locale_row(p, locale, &l); if (status) return status;
    status = numbering_row(p, nu, &n); if (status) return status;
    if (qjs_calendar_from_identifier(&id, calendar.data, calendar.length))
        return QJS_INTL_UNSUPPORTED;
    status = qjs_intl_native_provider_date_hour_cycle(p, locale, -1, &hc);
    if (status) return status;
    if (requested_cycle >= 0) hc = (unsigned int)requested_cycle;
    if (qjs_intl_native_provider_date_probe_success(p, l, n, (uint32_t)id, hc))
        return QJS_INTL_OK;
    memset(&o, 0, sizeof(o)); o.requested.calendar = calendar;
    o.requested.time_zone = bytes("UTC");
    o.requested.date_style = o.requested.time_style = -1;
    o.requested.hour_cycle = hc; o.requested.format_matcher = QJS_DATE_BEST_FIT;
    for (i = 0; i < QJS_DATE_FIELD_COUNT; i++) o.requested.fields[i] = -1;
    o.number_required = QJS_DATE_REQUIRE_ANY; o.number_defaults = QJS_DATE_DEFAULT_DATE;
    o.instant_defaults = QJS_DATE_DEFAULT_ALL;
    status = qjs_intl_native_provider_date_open(p, locale, nu, &o, NULL, &state, &bank);
    qjs_intl_native_date_bank_close(bank);
    if (status) return status;
    /* Failed storage must leave the proof absent and report allocator failure.
     * Every retry then performs the complete probe again. */
    return qjs_intl_native_provider_date_probe_success_store(p, l, n,
        (uint32_t)id, hc);
}
static QJSIntlStatus default_calendar_uncached(QJSIntlProvider *p, QJSIntlBytes locale,
    uint32_t row, QJSIntlBytes *out)
{
    QJSIntlNativeLocaleInfoResult preferences = {0};
    QJSIntlStatus status;
    uint32_t mask;
    size_t i;
    status = qjs_intl_native_provider_date_calendar_mask(p, row, &mask);
    if (status) return status;
    status = qjs_intl_native_locale_date_calendars(qjs_intl_native_provider_allocator(p),
        qjs_intl_native_provider_view(p), locale, &preferences);
    if (status) return status;
    status = QJS_INTL_UNSUPPORTED;
    for (i = 0; i < preferences.list.count; i++) {
        QJSCalendarId id;
        QJSIntlBytes candidate = preferences.list.items[i];
        if (qjs_calendar_from_identifier(&id, candidate.data, candidate.length) ||
            !(mask & (UINT32_C(1) << id)) || !qjs_calendar_is_supported(id)) continue;
        *out = bytes(qjs_calendar_identifier(id)); status = QJS_INTL_OK; break;
    }
    qjs_intl_native_locale_info_result_clear(qjs_intl_native_provider_allocator(p), &preferences);
    /* A missing preferred bank may use another independently proved calendar. */
    if (status == QJS_INTL_UNSUPPORTED) {
        for (i = 0; i < QJS_CAL_COUNT; i++) {
            if (!(mask & (UINT32_C(1) << i)) || !qjs_calendar_is_supported((QJSCalendarId)i)) continue;
            *out = bytes(qjs_calendar_identifier((QJSCalendarId)i)); return QJS_INTL_OK;
        }
    }
    return status;
}
static QJSIntlStatus date_defaults(QJSIntlProvider *p, QJSIntlBytes locale,
    uint32_t row, QJSIntlNativeDateDefaults *out)
{
    QJSIntlNativeDateDefaults defaults;
    QJSIntlNativeDateHourPreferences hours;
    QJSIntlBytes calendar;
    QJSCalendarId id;
    QJSIntlStatus status;
    const char *names[3];
    static const char *const cycles[] = {"h11", "h12", "h23", "h24"};
    unsigned int field, i;
    if (qjs_intl_native_provider_date_defaults_get(p, row, out)) return QJS_INTL_OK;
    status = default_calendar_uncached(p, locale, row, &calendar);
    if (status) return status;
    if (qjs_calendar_from_identifier(&id, calendar.data, calendar.length)) return QJS_INTL_DATA_ERROR;
    defaults.calendar = (uint32_t)id;
    status = qjs_intl_native_locale_date_hour_preferences(
        qjs_intl_native_provider_allocator(p), qjs_intl_native_provider_view(p), locale, &hours);
    if (status) return status;
    names[0] = hours.preferred; names[1] = hours.twelve; names[2] = hours.twenty_four;
    for (field = 0; field < 3; field++) {
        for (i = 0; i < 4; i++) if (!strcmp(names[field], cycles[i])) break;
        if (i == 4) return QJS_INTL_DATA_ERROR;
        defaults.hour_cycles[field] = i;
    }
    qjs_intl_native_provider_date_defaults_store(p, row, &defaults);
    *out = defaults;
    return QJS_INTL_OK;
}
static QJSIntlStatus default_calendar(QJSIntlProvider *p, QJSIntlBytes locale,
    uint32_t row, QJSIntlBytes *out)
{
    QJSIntlNativeDateDefaults defaults;
    QJSIntlStatus status = date_defaults(p, locale, row, &defaults);
    if (!status) *out = bytes(qjs_calendar_identifier((QJSCalendarId)defaults.calendar));
    return status;
}
QJSIntlStatus qjs_intl_native_provider_date_locale_available(QJSIntlProvider *p,
    QJSIntlBytes locale, uint32_t row)
{
    QJSIntlBytes numbering, calendar;
    QJSIntlStatus status = qjs_intl_native_provider_default_numbering(p, row, &numbering);
    if (status) return status;
    status = default_calendar(p, locale, row, &calendar);
    if (status) return status;
    return probe_cycle(p, locale, numbering, -1, calendar);
}
QJSIntlStatus qjs_intl_native_provider_date_key_values(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlBytes key, QJSIntlTagList *out)
{
    const QJSIntlDataView *v; QJSIntlDataSection s;
    QJSIntlBytes nu, candidate, calendar; QJSIntlStatus status; uint32_t row, i;
    static const char *const cycles[] = { "h11", "h12", "h23", "h24" };
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!p) return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_native_provider_locale_row(p, locale, &row); if (status) return status;
    status = qjs_intl_native_provider_default_numbering(p, row, &nu); if (status) return status;
    status = default_calendar(p, locale, row, &calendar); if (status) return status;
    status = probe_cycle(p, locale, nu, -1, calendar); if (status) return status;
    if (equal(key, bytes("ca"))) {
        uint32_t mask;
        status = qjs_intl_native_provider_date_calendar_mask(p, row, &mask);
        if (!status) status = append(p, out, calendar);
        for (i = 0; !status && i < QJS_CAL_COUNT; i++) {
            QJSCalendarId id = (QJSCalendarId)i;
            if (!(mask & (UINT32_C(1) << i)) || !qjs_calendar_is_supported(id)) continue;
            candidate = bytes(qjs_calendar_identifier(id));
            status = probe_cycle(p, locale, nu, -1, candidate);
            if (status == QJS_INTL_UNSUPPORTED) { status = QJS_INTL_OK; continue; }
            if (!status) status = append(p, out, candidate);
        }
    } else if (equal(key, bytes("hc"))) {
        /* ResolveLocale represents the default cycle with null, not a string. */
        status = append(p, out, (QJSIntlBytes){NULL, 0});
        for (i = 0; !status && i < 4; i++) status = append(p, out, bytes(cycles[i]));
    } else if (equal(key, bytes("nu"))) {
        status = append(p, out, nu); v = qjs_intl_native_provider_view(p);
        if (!status && qjs_intl_data_section(v, QJS_INTL_DATA_NUMBERING, &s)) status = QJS_INTL_DATA_ERROR;
        for (i = 0; !status && i < s.record_count; i++) {
            status = text(v, &s, i, 0, &candidate); if (status) break;
            status = probe_cycle(p, locale, candidate, -1, calendar);
            if (status == QJS_INTL_UNSUPPORTED) { status = QJS_INTL_OK; continue; }
            if (!status) status = append(p, out, candidate);
        }
    } else status = QJS_INTL_UNSUPPORTED;
    if (status) qjs_intl_tag_list_clear(p, out);
    return status;
}
