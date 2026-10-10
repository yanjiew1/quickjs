/* ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd, 2026-10-09:
 * sec-regionpreference, sec-calendarsoflocale, sec-collationsoflocale,
 * sec-hourcyclesoflocale, sec-numberingsystemsoflocale, sec-timezonesoflocale,
 * sec-textdirectionoflocale, sec-weekinfooflocale. CLDR49 / Unicode18.
 * No service coverage is inferred from CLDR identities or BCP47 types. */
#include "intl/native-locale-info.h"
#include <stdlib.h>
#include <string.h>

typedef struct InfoContext {
    const QJSIntlAllocator *a;
    const QJSIntlDataView *view;
    QJSIntlStatus status;
} InfoContext;
typedef struct TagParts {
    QJSIntlBytes base, language, script, region;
} TagParts;

static QJSIntlBytes bytes(const char *s)
{
    QJSIntlBytes b = {s, strlen(s)};
    return b;
}
static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}
static int alpha(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int digit(int c) { return c >= '0' && c <= '9'; }
static int all(QJSIntlBytes b, int (*predicate)(int))
{
    size_t i;
    for (i = 0; i < b.length; i++)
        if (!predicate((unsigned char)b.data[i])) return 0;
    return 1;
}
static int is_region(QJSIntlBytes b)
{
    return (b.length == 2 && all(b, alpha)) || (b.length == 3 && all(b, digit));
}
/* Only called after native locale parsing/canonicalization succeeded. */
static QJSIntlBytes token(const char **cursor)
{
    QJSIntlBytes b;
    const char *s = *cursor, *end = strchr(s, '-');
    b.data = s; b.length = end ? (size_t)(end - s) : strlen(s);
    *cursor = end ? end + 1 : s + b.length;
    return b;
}
static TagParts parts(const char *tag)
{
    TagParts p;
    QJSIntlBytes b;
    const char *cursor = tag;
    memset(&p, 0, sizeof(p));
    p.language = token(&cursor);
    p.base.data = tag; p.base.length = p.language.length;
    if (*cursor) {
        b = token(&cursor);
        if (b.length == 4 && all(b, alpha)) {
            p.script = b;
            if (*cursor) b = token(&cursor);
            else b.length = 0;
        }
        if (is_region(b)) p.region = b;
    }
    cursor = tag;
    (void)token(&cursor);
    while (*cursor) {
        b = token(&cursor);
        if (b.length == 1) break;
        p.base.length = (size_t)(b.data + b.length - tag);
    }
    return p;
}
static QJSIntlBytes uvalue(const char *tag, const char *key)
{
    const char *cursor = tag;
    QJSIntlBytes b, result = {0};
    int in_u = 0, found = 0;
    while (*cursor) {
        b = token(&cursor);
        if (b.length == 1) {
            if (in_u || b.data[0] == 'x') break;
            in_u = b.data[0] == 'u';
        } else if (in_u) {
            if (b.length == 2) {
                if (found) break;
                found = b.data[0] == key[0] && b.data[1] == key[1];
                if (found) { result.data = b.data + 2; result.length = 0; }
            } else if (found) {
                if (!result.length) result.data = b.data;
                result.length = (size_t)(b.data + b.length - result.data);
            }
        }
    }
    return result;
}
static void release(InfoContext *c, void *p)
{
    if (p) c->a->free(c->a->opaque, p);
}
static char *copy(InfoContext *c, QJSIntlBytes b)
{
    char *p;
    if (b.length == SIZE_MAX) { c->status = QJS_INTL_OVERFLOW; return NULL; }
    p = c->a->malloc(c->a->opaque, b.length + 1);
    if (!p) { c->status = QJS_INTL_NO_MEMORY; return NULL; }
    if (b.length) memcpy(p, b.data, b.length);
    p[b.length] = 0;
    return p;
}
void qjs_intl_native_locale_info_result_clear(const QJSIntlAllocator *a,
                                             QJSIntlNativeLocaleInfoResult *r)
{
    size_t i;
    if (!r) return;
    if (a && a->free) {
        for (i = 0; i < r->list.count; i++)
            if (r->list.items[i].data) a->free(a->opaque, (void *)r->list.items[i].data);
        if (r->list.items) a->free(a->opaque, r->list.items);
        if (r->value) a->free(a->opaque, r->value);
    }
    memset(r, 0, sizeof(*r));
}
static int append(InfoContext *c, QJSIntlTagList *list, QJSIntlBytes b)
{
    char *s;
    QJSIntlBytes *items;
    size_t i;
    for (i = 0; i < list->count; i++) if (equal(list->items[i], b)) return 1;
    if (list->count >= SIZE_MAX / sizeof(*items)) {
        c->status = QJS_INTL_OVERFLOW; return 0;
    }
    s = copy(c, b);
    if (!s) return 0;
    items = c->a->realloc(c->a->opaque, list->items,
                         (list->count + 1) * sizeof(*items));
    if (!items) {
        release(c, s); c->status = QJS_INTL_NO_MEMORY; return 0;
    }
    list->items = items;
    items[list->count].data = s; items[list->count].length = b.length;
    list->count++;
    return 1;
}
static int section(InfoContext *c, uint32_t id, QJSIntlDataSection *s)
{
    QJSIntlDataStatus st = qjs_intl_data_section(c->view, id, s);
    if (st == QJS_INTL_DATA_OK) return 1;
    c->status = st == QJS_INTL_DATA_NOT_FOUND ? QJS_INTL_UNSUPPORTED : QJS_INTL_DATA_ERROR;
    return 0;
}
static uint32_t number(InfoContext *c, const QJSIntlDataSection *s,
                        uint32_t i, uint32_t at)
{
    uint32_t n = 0;
    if (qjs_intl_data_record_u32(s, i, at, &n) != QJS_INTL_DATA_OK)
        c->status = QJS_INTL_DATA_ERROR;
    return n;
}
static QJSIntlBytes text(InfoContext *c, const QJSIntlDataSection *s,
                         uint32_t i, uint32_t at)
{
    QJSIntlDataSlice slice = {0};
    QJSIntlBytes b;
    if (qjs_intl_data_record_string(c->view, s, i, at, &slice) != QJS_INTL_DATA_OK)
        c->status = QJS_INTL_DATA_ERROR;
    b.data = (const char *)slice.data; b.length = slice.length;
    return b;
}
static int list_span(InfoContext *c, const QJSIntlDataSection *owner,
                      uint32_t row, uint32_t at, QJSIntlDataSection *list,
                      uint32_t *first, uint32_t *count)
{
    *first = number(c, owner, row, at); *count = number(c, owner, row, at + 4);
    if (c->status != QJS_INTL_OK) return 0;
    if (!*count) return 1;
    if (!section(c, QJS_INTL_DATA_LIST, list)) return 0;
    if (*first > list->record_count || *count > list->record_count - *first) {
        c->status = QJS_INTL_DATA_ERROR; return 0;
    }
    return 1;
}
/* unicode_subdivision_id: region prefix plus1..4 ASCII alnum suffix.
 * Canonicalize und-region separately: this is region alias resolution, not
 * likely-subtag inference and not a hardcoded zzzz test. */
static int subdivision_region(InfoContext *c, const char *tag, const char *key,
                               char region[4])
{
    QJSIntlBytes v = uvalue(tag, key), prefix;
    char input[8], *canonical = NULL;
    size_t n, i;
    TagParts p;
    region[0] = 0;
    if (!v.data || !v.length) return 1;
    n = v.length >= 3 && digit((unsigned char)v.data[0]) &&
        digit((unsigned char)v.data[1]) && digit((unsigned char)v.data[2]) ? 3 : 2;
    if (v.length < n + 1 || v.length > n + 4) return 1;
    prefix.data = v.data; prefix.length = n;
    if (!is_region(prefix)) return 1;
    for (i = n; i < v.length; i++)
        if (!alpha((unsigned char)v.data[i]) && !digit((unsigned char)v.data[i])) return 1;
    memcpy(input, "und-", 4); memcpy(input + 4, v.data, n); input[4 + n] = 0;
    c->status = qjs_intl_native_locale_canonicalize(c->a, c->view, bytes(input), &canonical);
    if (c->status != QJS_INTL_OK) return 0;
    p = parts(canonical);
    if (p.region.length) { memcpy(region, p.region.data, p.region.length); region[p.region.length] = 0; }
    release(c, canonical);
    return 1;
}
static int preference_regions(InfoContext *c, const char *tag, TagParts p,
                               char region[4], char override[4])
{
    char *maximal = NULL;
    TagParts maxparts;
    region[0] = override[0] = 0;
    if (p.region.length) {
        memcpy(region, p.region.data, p.region.length); region[p.region.length] = 0;
    } else if (!subdivision_region(c, tag, "sd", region)) return 0;
    if (!region[0]) {
        c->status = qjs_intl_native_locale_maximize(c->a, c->view, bytes(tag), &maximal);
        if (c->status != QJS_INTL_OK) return 0; /* OOM/data errors are not no-match. */
        maxparts = parts(maximal);
        if (maxparts.region.length) {
            memcpy(region, maxparts.region.data, maxparts.region.length);
            region[maxparts.region.length] = 0;
        }
        release(c, maximal);
        if (!region[0]) memcpy(region, "001", 4);
    }
    return subdivision_region(c, tag, "rg", override);
}
/* LookupMatchingLocaleByPrefix: remove all extensions; truncate the base
 * through complete subtags; root is never a language tag match. */
static int service_match(InfoContext *c, TagParts p, uint32_t service,
                         QJSIntlDataSection *info, uint32_t *row)
{
    QJSIntlDataSection locales;
    QJSIntlBytes candidate = p.base;
    uint32_t i;
    if (!section(c, QJS_INTL_DATA_LOCALE_SERVICE_INFO, info) ||
        !section(c, QJS_INTL_DATA_LOCALE, &locales)) return 0;
    while (candidate.length) {
        for (i = 0; i < info->record_count; i++) {
            uint32_t index;
            if (number(c, info, i, 4) != service) continue;
            index = number(c, info, i, 0);
            if (equal(candidate, text(c, &locales, index, 0))) {
                *row = i; return c->status == QJS_INTL_OK;
            }
            if (c->status != QJS_INTL_OK) return 0;
        }
        while (candidate.length && candidate.data[candidate.length - 1] != '-') candidate.length--;
        if (candidate.length) candidate.length--;
    }
    *row = QJS_INTL_DATA_INDEX_NONE;
    return c->status == QJS_INTL_OK;
}
static int compare(const void *a, const void *b)
{
    const QJSIntlBytes *x = a, *y = b;
    size_t n = x->length < y->length ? x->length : y->length;
    int d = n ? memcmp(x->data, y->data, n) : 0;
    return d ? d : x->length < y->length ? -1 : x->length > y->length ? 1 : 0;
}
static int service_values(InfoContext *c, TagParts p, uint32_t service,
                          QJSIntlNativeLocaleInfoResult *out)
{
    QJSIntlDataSection info, list;
    uint32_t row, first = 0, count = 0, i;
    if (!service_match(c, p, service, &info, &row)) return 0;
    if (row == QJS_INTL_DATA_INDEX_NONE) {
        if (service == QJS_INTL_LOCALE_INFO_NUMBER)
            return append(c, &out->list, bytes("latn"));
        return append(c, &out->list, bytes("emoji")) && append(c, &out->list, bytes("eor"));
    }
    if (service == QJS_INTL_LOCALE_INFO_NUMBER)
        return append(c, &out->list, text(c, &info, row, 8));
    if (!list_span(c, &info, row, 16, &list, &first, &count)) return 0;
    for (i = 0; i < count; i++) {
        QJSIntlBytes value = text(c, &list, first + i, 0);
        if (c->status != QJS_INTL_OK || !append(c, &out->list, value)) return 0;
    }
    if (out->list.count > 1) qsort(out->list.items, out->list.count, sizeof(*out->list.items), compare);
    return 1;
}
static int available_calendar(InfoContext *c, const QJSIntlDataSection *available,
                               QJSIntlBytes value)
{
    uint32_t i;
    for (i = 0; i < available->record_count; i++)
        if (equal(value, text(c, available, i, 0))) return 1;
    return 0;
}
static int preferred_values(InfoContext *c, const char *tag, TagParts p,
                            int calendars, QJSIntlNativeLocaleInfoResult *out)
{
    QJSIntlDataSection pref, list, available;
    char region[4], override[4], language_region[13];
    const char *regions[2];
    uint32_t first = 0, count = 0, row, i, scope;
    size_t r, regions_count;
    if (!section(c, QJS_INTL_DATA_PREFERENCE, &pref)) return 0;
    if (calendars && !section(c, QJS_INTL_DATA_AVAILABLE_CALENDAR, &available)) return 0;
    if (!preference_regions(c, tag, p, region, override)) return 0;
    regions[0] = override[0] ? override : region; regions[1] = region;
    regions_count = override[0] ? 2 : 1;
    for (r = 0; r < regions_count && !count; r++) {
        memcpy(language_region, p.language.data, p.language.length);
        language_region[p.language.length] = '-';
        memcpy(language_region + p.language.length + 1, regions[r], strlen(regions[r]) + 1);
        for (scope = 1; scope <= 2 && !count; scope++) {
            /* language-region before region; do not silently inherit001.
             * The specification's missing preference fallback is below. */
            uint32_t wire_scope = scope == 1 ? 1 : 0;
            QJSIntlBytes key = bytes(wire_scope ? language_region : regions[r]);
            for (row = 0; row < pref.record_count; row++) {
                if (number(c, &pref, row, 8) == wire_scope &&
                    equal(key, text(c, &pref, row, 0))) {
                    if (!list_span(c, &pref, row, calendars ? 12 : 20, &list, &first, &count)) return 0;
                    break;
                }
            }
            if (c->status != QJS_INTL_OK) return 0;
        }
    }
    for (i = 0; i < count; i++) {
        QJSIntlBytes value = text(c, &list, first + i, 0);
        if (c->status != QJS_INTL_OK) return 0;
        if (calendars) {
            char *canonical = NULL;
            c->status = qjs_intl_native_locale_canonicalize_uvalue(c->a, c->view, bytes("ca"), value, &canonical);
            if (c->status != QJS_INTL_OK) return 0;
            value = bytes(canonical);
            if (available_calendar(c, &available, value) && !append(c, &out->list, value)) {
                release(c, canonical); return 0;
            }
            release(c, canonical);
            if (c->status != QJS_INTL_OK) return 0;
        } else {
            if (!equal(value, bytes("h11")) && !equal(value, bytes("h12")) &&
                !equal(value, bytes("h23")) && !equal(value, bytes("h24"))) {
                c->status = QJS_INTL_DATA_ERROR; return 0;
            }
            if (!append(c, &out->list, value)) return 0;
        }
    }
    if (!out->list.count) return append(c, &out->list, bytes(calendars ? "gregory" : "h23"));
    return 1;
}
static int week_info(InfoContext *c, const char *tag, TagParts p,
                     QJSIntlBytes fw, QJSIntlNativeLocaleInfoResult *out)
{
    static const char *const days[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    QJSIntlDataSection weeks;
    QJSIntlDataSlice record;
    char region[4], override[4];
    const char *regions[3];
    uint32_t i, row = QJS_INTL_DATA_INDEX_NONE;
    size_t r;
    if (!section(c, QJS_INTL_DATA_WEEK, &weeks) ||
        !preference_regions(c, tag, p, region, override)) return 0;
    regions[0] = override; regions[1] = region; regions[2] = "001";
    for (r = 0; r < 3 && row == QJS_INTL_DATA_INDEX_NONE; r++) {
        if (!regions[r][0]) continue;
        for (i = 0; i < weeks.record_count; i++)
            if (equal(bytes(regions[r]), text(c, &weeks, i, 0))) { row = i; break; }
        if (c->status != QJS_INTL_OK) return 0;
    }
    if (row == QJS_INTL_DATA_INDEX_NONE ||
        qjs_intl_data_record(&weeks, row, &record) != QJS_INTL_DATA_OK) {
        c->status = QJS_INTL_DATA_ERROR; return 0;
    }
    out->first_day = record.data[8]; out->weekend_mask = record.data[9];
    if (!out->weekend_mask) { c->status = QJS_INTL_DATA_ERROR; return 0; }
    /* WEEK minimalDays at10 is retained wire metadata, never exposed here. */
    for (i = 0; i < 7; i++) if (equal(fw, bytes(days[i]))) out->first_day = (uint8_t)(i + 1);
    return 1;
}
static int time_zones(InfoContext *c, TagParts p,
                      QJSIntlNativeLocaleInfoResult *out)
{
    QJSIntlDataSection zones, list;
    uint32_t row, first = 0, count = 0, i;
    if (!section(c, QJS_INTL_DATA_REGION_TIME_ZONE, &zones)) return 0;
    for (row = 0; row < zones.record_count; row++) {
        if (equal(p.region, text(c, &zones, row, 0))) {
            if (!list_span(c, &zones, row, 8, &list, &first, &count)) return 0;
            break;
        }
    }
    if (c->status != QJS_INTL_OK) return 0;
    for (i = 0; i < count; i++) {
        QJSIntlBytes value = text(c, &list, first + i, 0);
        if (c->status != QJS_INTL_OK || !append(c, &out->list, value)) return 0;
    }
    if (out->list.count > 1) qsort(out->list.items, out->list.count, sizeof(*out->list.items), compare);
    return 1;
}
static int text_info(InfoContext *c, const char *tag, TagParts p,
                     QJSIntlNativeLocaleInfoResult *out)
{
    QJSIntlDataSection scripts;
    char *maximal = NULL;
    QJSIntlBytes script = p.script;
    uint32_t i;
    if (!section(c, QJS_INTL_DATA_SCRIPT_DIRECTION, &scripts)) return 0;
    if (!script.length) {
        c->status = qjs_intl_native_locale_maximize(c->a, c->view, bytes(tag), &maximal);
        if (c->status != QJS_INTL_OK) return 0;
        script = parts(maximal).script;
    }
    for (i = 0; script.length && i < scripts.record_count; i++) {
        if (equal(script, text(c, &scripts, i, 0))) {
            out->direction = (QJSIntlLocaleDirection)number(c, &scripts, i, 8);
            break;
        }
    }
    out->defined = out->direction != QJS_INTL_LOCALE_DIRECTION_UNDEFINED;
    release(c, maximal);
    return c->status == QJS_INTL_OK;
}
static QJSIntlBytes slot(const QJSIntlLocaleInfoRequest *r, QJSIntlLocaleInfoField f)
{
    QJSIntlBytes absent = {0};
    switch (f) {
    case QJS_INTL_LOCALE_CALENDARS: return r->calendar;
    case QJS_INTL_LOCALE_COLLATIONS: return r->collation;
    case QJS_INTL_LOCALE_HOUR_CYCLES: return r->hour_cycle;
    case QJS_INTL_LOCALE_NUMBERING_SYSTEMS: return r->numbering_system;
    case QJS_INTL_LOCALE_FIRST_DAY_OF_WEEK: return r->first_day;
    default: return absent;
    }
}
/* Structural slot checks admit the empty keyword type. Values are actual
 * canonical internal slots; this API does not silently override them. */
static int valid_slot(InfoContext *c, QJSIntlBytes value, const char *key, int hour)
{
    char *canonical = NULL;
    int valid;
    if (!value.data) {
        if (value.length) { c->status = QJS_INTL_INVALID_ARGUMENT; return 0; }
        return 1;
    }
    c->status = qjs_intl_native_locale_canonicalize_uvalue(c->a, c->view, bytes(key), value, &canonical);
    if (c->status != QJS_INTL_OK) return 0;
    valid = equal(value, bytes(canonical));
    release(c, canonical);
    /* MakeLocaleRecord retains inherited unknown hc values. Options were
     * checked by the frontend; HourCyclesOfLocale returns the actual slot. */
    (void)hour;
    if (!valid) c->status = QJS_INTL_INVALID_ARGUMENT;
    return valid;
}
QJSIntlStatus qjs_intl_native_locale_info_get(const QJSIntlAllocator *a,
    const QJSIntlDataView *view, const QJSIntlLocaleInfoCapabilities *cap,
    const QJSIntlLocaleInfoRequest *request, QJSIntlLocaleInfoField field,
    QJSIntlNativeLocaleInfoResult *out)
{
    InfoContext c;
    QJSIntlNativeLocaleInfoResult result;
    QJSIntlBytes selected;
    TagParts p;
    char *tag = NULL;
    int success = 0;
    if (out) memset(out, 0, sizeof(*out));
    if (!out || !a || !a->malloc || !a->realloc || !a->free || !view || !cap || !request ||
        (unsigned int)field > QJS_INTL_LOCALE_FIRST_DAY_OF_WEEK)
        return QJS_INTL_INVALID_ARGUMENT;
    c.a = a; c.view = view; c.status = QJS_INTL_OK;
    memset(&result, 0, sizeof(result));
    c.status = qjs_intl_native_locale_canonicalize(a, view, request->locale, &tag);
    if (c.status != QJS_INTL_OK) return c.status;
    p = parts(tag);
    selected = slot(request, field);
    if (!valid_slot(&c, selected,
        field == QJS_INTL_LOCALE_CALENDARS ? "ca" : field == QJS_INTL_LOCALE_COLLATIONS ? "co" :
        field == QJS_INTL_LOCALE_HOUR_CYCLES ? "hc" : field == QJS_INTL_LOCALE_NUMBERING_SYSTEMS ? "nu" : "fw",
        field == QJS_INTL_LOCALE_HOUR_CYCLES)) goto done;
    result.defined = 1;
    if (selected.data && field <= QJS_INTL_LOCALE_NUMBERING_SYSTEMS) {
        success = append(&c, &result.list, selected);
    } else switch (field) {
    case QJS_INTL_LOCALE_CALENDARS:
        if (!cap->calendars_ready) c.status = QJS_INTL_UNSUPPORTED;
        else success = preferred_values(&c, tag, p, 1, &result);
        break;
    case QJS_INTL_LOCALE_COLLATIONS:
        if (!cap->collator_ready) c.status = QJS_INTL_UNSUPPORTED;
        else success = service_values(&c, p, QJS_INTL_LOCALE_INFO_COLLATOR, &result);
        break;
    case QJS_INTL_LOCALE_HOUR_CYCLES:
        success = preferred_values(&c, tag, p, 0, &result); break;
    case QJS_INTL_LOCALE_NUMBERING_SYSTEMS:
        if (!cap->number_format_ready) c.status = QJS_INTL_UNSUPPORTED;
        else success = service_values(&c, p, QJS_INTL_LOCALE_INFO_NUMBER, &result);
        break;
    case QJS_INTL_LOCALE_TIME_ZONES:
        /* No region means undefined even when the timezone service is absent.
         * rg/sd and likely region have no role in TimeZonesOfLocale. */
        if (!p.region.length) { result.defined = 0; success = 1; }
        else if (!cap->time_zones_ready) c.status = QJS_INTL_UNSUPPORTED;
        else success = time_zones(&c, p, &result);
        break;
    case QJS_INTL_LOCALE_TEXT_INFO:
        success = text_info(&c, tag, p, &result); break;
    case QJS_INTL_LOCALE_WEEK_INFO:
        if (!valid_slot(&c, request->first_day, "fw", 0)) break;
        success = week_info(&c, tag, p, request->first_day, &result); break;
    case QJS_INTL_LOCALE_FIRST_DAY_OF_WEEK:
        if (selected.data) { result.value = copy(&c, selected); success = result.value != NULL; }
        else { result.defined = 0; success = 1; }
        break;
    }
done:
    release(&c, tag);
    if (!success || c.status != QJS_INTL_OK) {
        qjs_intl_native_locale_info_result_clear(a, &result);
        return c.status == QJS_INTL_OK ? QJS_INTL_DATA_ERROR : c.status;
    }
    *out = result;
    return QJS_INTL_OK;
}
