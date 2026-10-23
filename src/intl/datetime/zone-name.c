/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "zone-name.h"
#include "../display/select.h"
#include "../common/format-builder.h"
#include "../common/pair-pattern.h"
#include "../common/locale-grammar.h"
#include <string.h>

#define SEASON_WINDOW INT64_C(15897600) /* 184 * 86400. */

typedef struct NameSet { QJSIntlBytes generic, standard, daylight; } NameSet;
typedef struct Text { QJSIntlBytes bytes; unsigned int underscores; } Text;
typedef struct Layout { Text items[5]; size_t count; } Layout;
typedef struct Zone {
    QJSIntlTimeZoneIdentity identity;
    QJSIntlTimeZonePeriod period;
    QJSTimeZoneInfo info;
    const QJSTimeZone *named;
    uint32_t index;
    unsigned int has_zone, has_period;
    int64_t instant;
} Zone;

enum { GENERIC, STANDARD, DAYLIGHT, UNKNOWN };

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int same(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}

static QJSIntlStatus rule_error(int error)
{
    return error == QJS_TZ_MEMORY ? QJS_INTL_NO_MEMORY :
           error == QJS_TZ_RANGE ? QJS_INTL_INVALID_ARGUMENT :
           error == QJS_TZ_UNSUPPORTED ? QJS_INTL_UNSUPPORTED : QJS_INTL_DATA_ERROR;
}

static QJSIntlStatus head(const QJSIntlDateZoneData *data, QJSIntlTimeZoneNameFamily family,
                          QJSIntlBytes entity, QJSIntlTimeZoneNameWidth width, uint32_t *out)
{
    QJSIntlTimeZoneNameGroup group = {0};
    uint32_t index;
    QJSIntlStatus status;
    *out = 0;
    group.family = family;
    group.entity = entity;
    group.width = width;
    status = qjs_intl_timezone_name_find_group(data->names, &group, &index);
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    return status ? status : qjs_intl_timezone_name_head(data->names, data->names_locale, index, out);
}

static QJSIntlStatus text(const QJSIntlDateZoneData *data, uint32_t source, const char *code,
                          QJSIntlTimeZoneNameType type, QJSIntlBytes *out)
{
    QJSIntlTimeZoneNameEntry entry;
    QJSIntlTimeZoneNameNode origin;
    QJSIntlStatus status;
    *out = (QJSIntlBytes){0};
    status = qjs_intl_timezone_name_resolve(data->names, source, bytes(code), type,
                                           (QJSIntlBytes){0}, &entry, &origin);
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (!status && !entry.blocked && entry.text.length)
        *out = entry.text;
    return status;
}

static QJSIntlStatus names(const QJSIntlDateZoneData *data, QJSIntlTimeZoneNameFamily family,
                           QJSIntlBytes entity, QJSIntlTimeZoneNameWidth width, NameSet *out)
{
    uint32_t source;
    QJSIntlStatus status = head(data, family, entity, width, &source);
    memset(out, 0, sizeof(*out));
    if (!status)
        status = text(data, source, "generic", QJS_INTL_TIMEZONE_NAME_NO_TYPE, &out->generic);
    if (!status)
        status = text(data, source, "standard", QJS_INTL_TIMEZONE_NAME_NO_TYPE, &out->standard);
    if (!status)
        status = text(data, source, "daylight", QJS_INTL_TIMEZONE_NAME_NO_TYPE, &out->daylight);
    return status;
}

/* A zero custom offset has GMT's names without changing its public identifier.
   This is a name-style lookup; explicit O/offset styles still use hourFormat.
   Use locale data, including masks and empty records, rather than a GMT literal. */
static QJSIntlStatus fixed_zero_name(const QJSIntlDateZoneData *data,
                                     QJSIntlTimeZoneNameWidth width, int generic,
                                     QJSIntlBytes *out)
{
    static const struct {
        QJSIntlTimeZoneNameFamily family;
        const char *entity;
    } sources[] = {
        {QJS_INTL_TIMEZONE_NAME_ZONE, "Etc/GMT"},
        {QJS_INTL_TIMEZONE_NAME_METAZONE, "GMT"},
    };
    QJSIntlStatus status;
    NameSet set;
    size_t i;

    *out = (QJSIntlBytes){0};
    for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        status = names(data, sources[i].family, bytes(sources[i].entity), width, &set);
        if (status)
            return status;
        /* A fixed zero offset is permanently standard. Generic styles may
           therefore use its standard name without a seasonal rule query. */
        *out = generic && set.generic.length ? set.generic :
               set.standard.length ? set.standard : set.generic;
        if (out->length)
            break;
    }
    return QJS_INTL_OK;
}

static int effective_type(QJSTimeZoneInfo info, const QJSIntlTimeZonePeriod *period)
{
    int standard, daylight;
    if (!period || !(period->flags & (QJS_INTL_TIMEZONE_PERIOD_STANDARD | QJS_INTL_TIMEZONE_PERIOD_DAYLIGHT)))
        return info.is_dst ? DAYLIGHT : STANDARD;
    standard = (period->flags & QJS_INTL_TIMEZONE_PERIOD_STANDARD) && info.offset_seconds == period->standard_offset;
    daylight = (period->flags & QJS_INTL_TIMEZONE_PERIOD_DAYLIGHT) && info.offset_seconds == period->daylight_offset;
    return standard != daylight ? (standard ? STANDARD : DAYLIGHT) : UNKNOWN;
}

static QJSIntlStatus standard_window(const QJSIntlDateZoneData *data, const Zone *zone, int *out)
{
    QJSIntlBytes name;
    uint32_t start, count, i;
    int64_t first, last, cursor;
    int constant, error;
    QJSIntlStatus status;
    *out = 0;
    if (!zone->has_zone || zone->instant < INT64_MIN + SEASON_WINDOW || zone->instant > INT64_MAX - SEASON_WINDOW)
        return QJS_INTL_OK;
    first = cursor = zone->instant - SEASON_WINDOW;
    last = zone->instant + SEASON_WINDOW;
    error = qjs_timezone_info_is_constant(zone->named, first, last, &constant);
    if (error == QJS_TZ_RANGE || error == QJS_TZ_UNSUPPORTED)
        return QJS_INTL_OK;
    if (error)
        return rule_error(error);
    if (!constant)
        return QJS_INTL_OK;
    status = qjs_intl_timezone_presentation_zone(data->presentation, zone->index, &name, &start, &count);
    if (status)
        return status;
    for (i = 0; i < count; i++) {
        QJSIntlTimeZonePeriod period;
        unsigned int overrides;
        status = qjs_intl_timezone_presentation_period(data->presentation, start + i, &period);
        if (status)
            return status;
        if ((period.flags & QJS_INTL_TIMEZONE_PERIOD_END) && period.end <= cursor)
            continue;
        if ((period.flags & QJS_INTL_TIMEZONE_PERIOD_START) && period.start > cursor)
            return QJS_INTL_OK;
        overrides = period.flags & (QJS_INTL_TIMEZONE_PERIOD_STANDARD | QJS_INTL_TIMEZONE_PERIOD_DAYLIGHT);
        if (overrides) {
            if (!(overrides & QJS_INTL_TIMEZONE_PERIOD_STANDARD) ||
                zone->info.offset_seconds != period.standard_offset ||
                ((overrides & QJS_INTL_TIMEZONE_PERIOD_DAYLIGHT) && period.daylight_offset == period.standard_offset))
                return QJS_INTL_OK;
            if (zone->info.is_dst && overrides != (QJS_INTL_TIMEZONE_PERIOD_STANDARD | QJS_INTL_TIMEZONE_PERIOD_DAYLIGHT))
                return QJS_INTL_OK;
        } else if (zone->info.is_dst) {
            return QJS_INTL_OK;
        }
        if (!(period.flags & QJS_INTL_TIMEZONE_PERIOD_END) || period.end > last) {
            *out = 1;
            return QJS_INTL_OK;
        }
        cursor = period.end;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus choose(const QJSIntlDateZoneData *data, const Zone *zone,
                            const NameSet *set, int type, QJSIntlBytes *out)
{
    QJSIntlStatus status;
    int proven;
    *out = type == GENERIC ? set->generic : type == STANDARD ? set->standard :
           type == DAYLIGHT ? set->daylight : (QJSIntlBytes){0};
    if (out->length)
        return QJS_INTL_OK;
    if (!set->daylight.length) {
        *out = set->generic.length ? set->generic : set->standard;
    } else if (type == GENERIC && set->standard.length) {
        status = standard_window(data, zone, &proven);
        if (status)
            return status;
        if (proven)
            *out = set->standard;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus country(const QJSIntlDateZoneData *data, QJSIntlBytes code, Text *out)
{
    QJSIntlNameLabel label;
    QJSIntlStatus status;
    char normalized[3];
    size_t i;
    *out = (Text){code, 0};
    if (!data->regions)
        return QJS_INTL_OK;
    if (code.length > sizeof(normalized))
        return QJS_INTL_DATA_ERROR;
    /* Shared name catalogs use folded keys. Keep the uppercase raw code for
       the CLDR unlocalized-country fallback. */
    for (i = 0; i < code.length; i++) {
        unsigned int c = (uint8_t)code.data[i];
        normalized[i] = c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
    }
    status = qjs_intl_name_select(data->regions, data->regions_locale, QJS_INTL_NAME_REGION,
                                  QJS_INTL_NAME_SHORT, 0, (QJSIntlBytes){normalized, code.length}, &label);
    if (status)
        return status;
    if (label.found && label.entry.text.length)
        out->bytes = label.entry.text;
    return QJS_INTL_OK;
}

static QJSIntlStatus city(const QJSIntlDateZoneData *data, const Zone *zone, Text *out)
{
    uint32_t source;
    QJSIntlBytes selected;
    QJSIntlStatus status = head(data, QJS_INTL_TIMEZONE_NAME_CITY, zone->identity.identifier,
                                QJS_INTL_TIMEZONE_NAME_NO_WIDTH, &source);
    size_t start;
    *out = (Text){0};
    if (!status)
        status = text(data, source, "exemplarCity", QJS_INTL_TIMEZONE_NAME_NO_TYPE, &selected);
    if (status)
        return status;
    if (selected.length) {
        out->bytes = selected;
    } else if (zone->identity.territory[0]) {
        start = zone->identity.identifier.length;
        while (start && zone->identity.identifier.data[start - 1] != '/')
            start--;
        out->bytes = (QJSIntlBytes){zone->identity.identifier.data + start, zone->identity.identifier.length - start};
        out->underscores = 1;
    } else {
        status = head(data, QJS_INTL_TIMEZONE_NAME_CITY, bytes("Etc/Unknown"),
                      QJS_INTL_TIMEZONE_NAME_NO_WIDTH, &source);
        if (!status)
            status = text(data, source, "exemplarCity", QJS_INTL_TIMEZONE_NAME_NO_TYPE, &out->bytes);
    }
    return status;
}

static QJSIntlStatus same_identity(const QJSIntlDateZoneData *data, QJSIntlBytes candidate,
                                   const Zone *zone, int *out)
{
    QJSIntlTimeZoneIdentity identity;
    QJSIntlStatus status = qjs_intl_timezone_identity(data->metadata, candidate, &identity);
    *out = 0;
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_DATA_ERROR; /* A CLDR record must reference its metadata. */
    if (!status)
        *out = identity.metadata_index == zone->identity.metadata_index;
    return status;
}

static QJSIntlStatus format_source(const QJSIntlDateZoneData *data, const char *code, QJSIntlBytes *out)
{
    uint32_t source;
    QJSIntlStatus status = head(data, QJS_INTL_TIMEZONE_NAME_FORMAT, (QJSIntlBytes){0},
                                QJS_INTL_TIMEZONE_NAME_NO_WIDTH, &source);
    return status ? status : text(data, source, code, QJS_INTL_TIMEZONE_NAME_NO_TYPE, out);
}

static QJSIntlStatus location(const QJSIntlDateZoneData *data, const Zone *zone, Layout *out)
{
    QJSIntlBytes region = bytes(zone->identity.territory), dominant, pattern;
    QJSIntlUnaryPattern wrapper;
    QJSIntlStatus status;
    uint32_t count;
    int selected = 0;
    Text body;
    /* Etc zones describe offsets rather than places. POSIX-style named zones
       without a region can still use CLDR's localized unknown-city label. */
    if (!region.length && zone->identity.identifier.length >= 4 &&
        !memcmp(zone->identity.identifier.data, "Etc/", 4))
        return QJS_INTL_OK;
    if (region.length) {
        status = qjs_intl_timezone_region_count(data->metadata, region, &count);
        if (status)
            return status;
        if (count == 1) {
            selected = 1;
        } else {
            status = qjs_intl_timezone_presentation_primary(data->presentation, region, &dominant);
            if (!status)
                status = same_identity(data, dominant, zone, &selected);
            if (status != QJS_INTL_OK && status != QJS_INTL_UNSUPPORTED)
                return status;
        }
    }
    status = selected ? country(data, region, &body) : city(data, zone, &body);
    if (status || !body.bytes.length)
        return status;
    status = format_source(data, "regionFormat", &pattern);
    if (status || !pattern.length)
        return status;
    status = qjs_intl_unary_pattern_parse(pattern, &wrapper);
    if (status)
        return status == QJS_INTL_UNSUPPORTED ? QJS_INTL_DATA_ERROR : status;
    *out = (Layout){{{wrapper.prefix, 0}, body, {wrapper.suffix, 0}}, 3};
    return QJS_INTL_OK;
}

static QJSIntlStatus qualified(const QJSIntlDateZoneData *data, const Zone *zone,
                               QJSIntlBytes label, Layout *out)
{
    QJSIntlBytes preferred, region = data->territory.length ? data->territory : bytes("001"), pattern;
    QJSIntlPairPattern wrapper;
    QJSIntlStatus status;
    int selected = 0;
    Text qualifier, meta = {label, 0};
    status = qjs_intl_timezone_presentation_preferred(data->presentation, zone->period.metazone, region, &preferred);
    if (status == QJS_INTL_UNSUPPORTED && !same(region, bytes("001")))
        status = qjs_intl_timezone_presentation_preferred(data->presentation, zone->period.metazone, bytes("001"), &preferred);
    if (!status)
        status = same_identity(data, preferred, zone, &selected);
    if (status != QJS_INTL_OK && status != QJS_INTL_UNSUPPORTED)
        return status;
    if (selected) {
        *out = (Layout){{meta}, 1};
        return QJS_INTL_OK;
    }
    region = bytes(zone->identity.territory);
    if (region.length) {
        status = qjs_intl_timezone_presentation_preferred(data->presentation, zone->period.metazone, region, &preferred);
        if (!status)
            status = same_identity(data, preferred, zone, &selected);
        if (status != QJS_INTL_OK && status != QJS_INTL_UNSUPPORTED)
            return status;
    }
    status = selected ? country(data, region, &qualifier) : city(data, zone, &qualifier);
    if (status || !qualifier.bytes.length)
        return status;
    status = format_source(data, "fallbackFormat", &pattern);
    if (status || !pattern.length)
        return status;
    status = qjs_intl_pair_pattern_parse(pattern, &wrapper);
    if (status)
        return status == QJS_INTL_UNSUPPORTED ? QJS_INTL_DATA_ERROR : status;
    *out = (Layout){{{wrapper.prefix, 0}, wrapper.first ? meta : qualifier,
                     {wrapper.separator, 0}, wrapper.first ? qualifier : meta, {wrapper.suffix, 0}}, 5};
    return QJS_INTL_OK;
}

static QJSIntlStatus emit(QJSIntlFormatBuilder *builder, const Layout *layout)
{
    size_t i;
    for (i = 0; i < layout->count; i++) {
        const Text *item = &layout->items[i];
        size_t start = 0, at;
        QJSIntlStatus status;
        if (!item->underscores) {
            status = qjs_intl_format_builder_text(builder, item->bytes, NULL, NULL);
            if (status)
                return status;
            continue;
        }
        for (at = 0; at < item->bytes.length; at++) {
            if (item->bytes.data[at] != '_')
                continue;
            status = qjs_intl_format_builder_text(builder, (QJSIntlBytes){item->bytes.data + start, at - start}, NULL, NULL);
            if (!status)
                status = qjs_intl_format_builder_text(builder, bytes(" "), NULL, NULL);
            if (status)
                return status;
            start = at + 1;
        }
        status = qjs_intl_format_builder_text(builder,
                  (QJSIntlBytes){item->bytes.data + start, item->bytes.length - start}, NULL, NULL);
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_zone_format(const QJSIntlAllocator *allocator,
                                       const QJSIntlDateZoneData *data,
                                       const QJSIntlDateOffset *offset,
                                       const QJSIntlDateNumberFormats *numbers,
                                       const QJSTimeZone *named, int32_t offset_minutes,
                                       int64_t epoch_seconds, QJSIntlDateTimeZoneStyle style,
                                       size_t maximum_output_length, QJSIntlFormatted *out)
{
    Zone zone = {0};
    Layout layout = {0};
    NameSet set;
    QJSIntlBytes label, metazone, short_id;
    QJSIntlTimeZoneNameWidth width;
    QJSIntlFormatBuilder builder = {0};
    QJSIntlStatus status;
    int error, type;
    unsigned int long_format = !((unsigned int)style & 1);
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !offset || !numbers ||
        !maximum_output_length || (unsigned int)style - 1 >= QJS_INTL_DATETIME_ZONE_LONG_GENERIC ||
        (named ? offset_minutes != 0 : offset_minutes <= -1440 || offset_minutes >= 1440))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!offset->storage || offset->allocator.opaque != allocator->opaque ||
        offset->allocator.malloc != allocator->malloc || offset->allocator.free != allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    zone.named = named;
    zone.instant = epoch_seconds;
    if (named) {
        error = qjs_timezone_info(named, epoch_seconds, &zone.info);
        if (error)
            return rule_error(error);
        if (zone.info.offset_seconds <= -86400 || zone.info.offset_seconds >= 86400 ||
            (unsigned int)zone.info.is_dst > 1)
            return QJS_INTL_DATA_ERROR;
    } else {
        zone.info.offset_seconds = offset_minutes * 60;
    }
    if (style == QJS_INTL_DATETIME_ZONE_SHORT_OFFSET || style == QJS_INTL_DATETIME_ZONE_LONG_OFFSET)
        goto offset_fallback;
    width = long_format ? QJS_INTL_TIMEZONE_NAME_LONG : QJS_INTL_TIMEZONE_NAME_SHORT;
    if (!named) {
        if (offset_minutes || !data || !data->names || !data->names->section.bytes)
            goto offset_fallback;
        if (data->names_locale >= data->names->section.count)
            return QJS_INTL_INVALID_ARGUMENT;
        status = fixed_zero_name(data, width, style >= QJS_INTL_DATETIME_ZONE_SHORT_GENERIC, &label);
        if (status)
            return status;
        if (!label.length)
            goto offset_fallback;
        layout = (Layout){{{label, 0}}, 1};
        goto render_layout;
    }
    if (!data || !data->names || !data->names->section.bytes || !data->metadata ||
        !data->presentation || !data->presentation->section.bytes ||
        data->names_locale >= data->names->section.count ||
        (data->regions && (!data->regions->section.bytes || data->regions_locale >= data->regions->section.count)))
        return QJS_INTL_INVALID_ARGUMENT;
    if (data->territory.length) {
        size_t i;
        if (!qjs_intl_unicode_subtag_well_formed(QJS_INTL_REGION_SUBTAG,
                                                data->territory.data, data->territory.length))
            return QJS_INTL_INVALID_ARGUMENT;
        for (i = 0; i < data->territory.length; i++) {
            unsigned int c = (uint8_t)data->territory.data[i];
            if (data->territory.length == 2 && (c < 'A' || c > 'Z'))
                return QJS_INTL_INVALID_ARGUMENT;
        }
    }
    status = qjs_intl_timezone_identity(data->metadata, bytes(qjs_timezone_identifier(named)), &zone.identity);
    if (status == QJS_INTL_UNSUPPORTED)
        goto offset_fallback; /* IANA can contain names absent from this CLDR release. */
    if (status)
        return status;
    status = qjs_intl_timezone_presentation_find_zone(data->presentation, zone.identity.identifier, &zone.index);
    if (!status) {
        zone.has_zone = 1;
        status = qjs_intl_timezone_presentation_at(data->presentation, zone.index, epoch_seconds, &zone.period);
        if (!status)
            zone.has_period = 1;
    }
    if (status != QJS_INTL_OK && status != QJS_INTL_UNSUPPORTED)
        return status;
    type = style >= QJS_INTL_DATETIME_ZONE_SHORT_GENERIC ? GENERIC :
           effective_type(zone.info, zone.has_period ? &zone.period : NULL);
    status = names(data, QJS_INTL_TIMEZONE_NAME_ZONE, zone.identity.identifier, width, &set);
    if (!status)
        status = choose(data, &zone, &set, type, &label);
    if (status)
        return status;
    if (label.length) {
        layout = (Layout){{{label, 0}}, 1};
    } else if (zone.has_period) {
        status = qjs_intl_timezone_presentation_identity(data->presentation, zone.period.metazone, &metazone, &short_id);
        if (!status)
            status = names(data, QJS_INTL_TIMEZONE_NAME_METAZONE, metazone, width, &set);
        if (!status)
            status = choose(data, &zone, &set, type, &label);
        if (status)
            return status;
        if (label.length) {
            if (type == GENERIC)
                status = qualified(data, &zone, label, &layout);
            else
                layout = (Layout){{{label, 0}}, 1};
            if (status)
                return status;
        }
    }
    if (!layout.count && type == GENERIC) {
        status = location(data, &zone, &layout);
        if (status)
            return status;
    }
    if (!layout.count)
        goto offset_fallback;
render_layout:
    builder.limit = maximum_output_length;
    status = emit(&builder, &layout);
    if (status)
        return status;
    if (!builder.length || builder.length > SIZE_MAX / sizeof(uint16_t))
        return builder.length ? QJS_INTL_OVERFLOW : QJS_INTL_UNSUPPORTED;
    builder.result.text = allocator->malloc(allocator->opaque, builder.length * sizeof(uint16_t));
    builder.result.parts = allocator->malloc(allocator->opaque, sizeof(QJSIntlPart));
    if (!builder.result.text || !builder.result.parts) {
        qjs_intl_formatted_clear(allocator, &builder.result);
        return QJS_INTL_NO_MEMORY;
    }
    builder.result.length = builder.length;
    builder.result.part_count = 1;
    builder.length = 0;
    status = emit(&builder, &layout);
    if (status) {
        qjs_intl_formatted_clear(allocator, &builder.result);
        return status;
    }
    builder.result.parts[0] = (QJSIntlPart){0, builder.length, QJS_INTL_PART_TIME_ZONE_NAME};
    *out = builder.result;
    return QJS_INTL_OK;
offset_fallback:
    return qjs_intl_date_offset_format(offset, zone.info.offset_seconds, long_format,
                                       numbers->numeric, numbers->two_digit, maximum_output_length, out);
}
