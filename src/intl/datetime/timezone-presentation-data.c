/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "timezone-presentation-data.h"
#include "timezone-syntax.h"
#include "../native/wire.h"
#include <stdlib.h>
#include <string.h>

#define HEADER_WORDS 5
#define ID_WIDTH 2
#define ZONE_WIDTH 3
#define PERIOD_WIDTH 8
#define SHORT_USED 1
#define ZONE_CHECKED 2
#define METAZONE_CHECKED 4

typedef struct Membership {
    uint32_t zone, metazone;
} Membership;

static uint32_t word(const uint8_t *bytes, size_t index)
{
    return qjs_intl_read_u32(bytes + index * 4);
}

static int64_t signed64(uint32_t lo, uint32_t hi)
{
    uint64_t bits = (uint64_t)hi << 32 | lo;
    return bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)~bits;
}

static int32_t signed32(uint32_t bits)
{
    return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)~bits;
}

static int take(const QJSIntlSection *section, size_t *at, uint32_t count, uint32_t width,
                const uint8_t **out)
{
    if (*at > section->length / 4 || count > (section->length / 4 - *at) / width)
        return 0;
    *out = section->bytes + *at * 4;
    *at += (size_t)count * width;
    return 1;
}

static int region_valid(QJSIntlBytes text)
{
    size_t i;
    if (text.length != 2 && text.length != 3)
        return 0;
    for (i = 0; i < text.length; i++) {
        unsigned int c = (uint8_t)text.data[i];
        if (text.length == 2 ? c < 'A' || c > 'Z' : c < '0' || c > '9')
            return 0;
    }
    return 1;
}

static int identity_valid(const QJSIntlData *data, uint32_t ref, uint8_t *checked, int metazone)
{
    QJSIntlBytes text;
    uint8_t mask = metazone ? METAZONE_CHECKED : ZONE_CHECKED;
    if (ref >= data->strings.count)
        return 0;
    if (!(checked[ref] & mask)) {
        if (qjs_intl_data_string(data, ref, &text) || !qjs_intl_timezone_source_identity_valid(text, metazone))
            return 0;
        checked[ref] |= mask;
    }
    return 1;
}

static void read_period(const uint8_t *record, QJSIntlTimeZonePeriod *out)
{
    out->start = signed64(word(record, 0), word(record, 1));
    out->end = signed64(word(record, 2), word(record, 3));
    out->metazone = word(record, 4);
    out->flags = word(record, 5);
    out->standard_offset = signed32(word(record, 6));
    out->daylight_offset = signed32(word(record, 7));
}

static int period_valid(const uint8_t *record, uint32_t identity_count)
{
    QJSIntlTimeZonePeriod period;
    read_period(record, &period);
    if (period.metazone >= identity_count || period.flags & ~15U ||
        (!(period.flags & QJS_INTL_TIMEZONE_PERIOD_START) && period.start) ||
        (!(period.flags & QJS_INTL_TIMEZONE_PERIOD_END) && period.end) ||
        (!(period.flags & QJS_INTL_TIMEZONE_PERIOD_STANDARD) && period.standard_offset) ||
        (!(period.flags & QJS_INTL_TIMEZONE_PERIOD_DAYLIGHT) && period.daylight_offset) ||
        ((period.flags & 3) == 3 && period.start >= period.end) ||
        period.standard_offset < -86340 || period.standard_offset > 86340 || period.standard_offset % 60 ||
        period.daylight_offset < -86340 || period.daylight_offset > 86340 || period.daylight_offset % 60)
        return 0;
    return 1;
}

static int membership_compare(const void *left, const void *right)
{
    const Membership *a = left, *b = right;
    if (a->zone != b->zone)
        return a->zone < b->zone ? -1 : 1;
    return a->metazone < b->metazone ? -1 : a->metazone != b->metazone;
}

QJSIntlStatus qjs_intl_timezone_presentation_data_open(const QJSIntlData *data,
                                                       const QJSIntlAllocator *allocator,
                                                       QJSIntlTimeZonePresentationData *out)
{
    QJSIntlTimeZonePresentationData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    uint8_t *checked = NULL;
    Membership *membership = NULL;
    size_t at = HEADER_WORDS, i, j, next = 0, membership_count = 0;
    QJSIntlBytes text;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes || !data->locales.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_TIMEZONE_PRESENTATION, &result.section) ||
        result.section.length < HEADER_WORDS * 4 || result.section.length % 4 ||
        word(result.section.bytes, 0) != 1 || qjs_intl_data_string(data, 0, &text) || text.length)
        return status;
    result.zone_count = result.section.count;
    result.period_count = word(result.section.bytes, 1);
    result.preferred_count = word(result.section.bytes, 2);
    result.primary_count = word(result.section.bytes, 3);
    result.identity_count = word(result.section.bytes, 4);
    if (!take(&result.section, &at, result.identity_count, ID_WIDTH, &result.identities) ||
        !take(&result.section, &at, result.zone_count, ZONE_WIDTH, &result.zones) ||
        !take(&result.section, &at, result.period_count, PERIOD_WIDTH, &result.periods) ||
        !take(&result.section, &at, result.preferred_count, 3, &result.preferred) ||
        !take(&result.section, &at, result.primary_count, 2, &result.primary) || at != result.section.length / 4)
        return status;
    checked = allocator->malloc(allocator->opaque, data->strings.count);
    if (!checked)
        return QJS_INTL_NO_MEMORY;
    memset(checked, 0, data->strings.count);
    if (result.period_count) {
        /* The validated physical period width also bounds this smaller scratch
           allocation on 32-bit size_t. Never allocate an implicit zone cube. */
        membership = allocator->malloc(allocator->opaque, (size_t)result.period_count * sizeof(*membership));
        if (!membership) {
            status = QJS_INTL_NO_MEMORY;
            goto done;
        }
    }
    for (i = 0; i < result.identity_count; i++) {
        uint32_t long_ref = word(result.identities, i * ID_WIDTH), short_ref = word(result.identities, i * ID_WIDTH + 1);
        if ((i && word(result.identities, (i - 1) * ID_WIDTH) >= long_ref) ||
            !identity_valid(data, long_ref, checked, 1) || short_ref >= data->strings.count ||
            checked[short_ref] & SHORT_USED || qjs_intl_data_string(data, short_ref, &text) || text.length != 4)
            goto done;
        for (j = 0; j < text.length; j++) {
            if (text.data[j] < 'a' || text.data[j] > 'z')
                goto done;
        }
        checked[short_ref] |= SHORT_USED;
    }
    for (i = 0; i < result.zone_count; i++) {
        uint32_t ref = word(result.zones, i * ZONE_WIDTH);
        uint32_t start = word(result.zones, i * ZONE_WIDTH + 1), count = word(result.zones, i * ZONE_WIDTH + 2);
        QJSIntlTimeZonePeriod previous = {0};
        if ((i && word(result.zones, (i - 1) * ZONE_WIDTH) >= ref) ||
            !identity_valid(data, ref, checked, 0) || start != next || count > result.period_count - next)
            goto done;
        for (j = 0; j < count; j++) {
            const uint8_t *record = result.periods + (next + j) * PERIOD_WIDTH * 4;
            QJSIntlTimeZonePeriod current;
            if (!period_valid(record, result.identity_count))
                goto done;
            read_period(record, &current);
            if (j && (!(previous.flags & QJS_INTL_TIMEZONE_PERIOD_END) ||
                      !(current.flags & QJS_INTL_TIMEZONE_PERIOD_START) || previous.end > current.start))
                goto done;
            previous = current;
            membership[next + j] = (Membership){(uint32_t)i, current.metazone};
        }
        next += count;
    }
    if (next != result.period_count)
        goto done;
    if (result.period_count) {
        qsort(membership, result.period_count, sizeof(*membership), membership_compare);
        for (i = 0; i < result.period_count; i++) {
            if (!membership_count || membership_compare(&membership[membership_count - 1], &membership[i]))
                membership[membership_count++] = membership[i];
        }
    }
    for (i = 0; i < result.preferred_count; i++) {
        uint32_t metazone = word(result.preferred, i * 3), region = word(result.preferred, i * 3 + 1);
        Membership key = {word(result.preferred, i * 3 + 2), metazone};
        if (metazone >= result.identity_count || key.zone >= result.zone_count ||
            qjs_intl_data_string(data, region, &text) || !region_valid(text) ||
            (i && (word(result.preferred, (i - 1) * 3) > metazone ||
                   (word(result.preferred, (i - 1) * 3) == metazone &&
                    word(result.preferred, (i - 1) * 3 + 1) >= region))) ||
            !membership_count || !bsearch(&key, membership, membership_count, sizeof(*membership), membership_compare))
            goto done;
    }
    for (i = 0; i < result.primary_count; i++) {
        uint32_t region = word(result.primary, i * 2), zone = word(result.primary, i * 2 + 1);
        if ((i && word(result.primary, (i - 1) * 2) >= region) ||
            qjs_intl_data_string(data, region, &text) || !region_valid(text) ||
            !identity_valid(data, zone, checked, 0))
            goto done;
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    if (membership)
        allocator->free(allocator->opaque, membership);
    allocator->free(allocator->opaque, checked);
    return status;
}

static int compare_bytes(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = length ? memcmp(a.data, b.data, length) : 0;
    return order ? order : a.length < b.length ? -1 : a.length != b.length;
}

QJSIntlStatus qjs_intl_timezone_presentation_identity(const QJSIntlTimeZonePresentationData *data,
                                                      uint32_t index, QJSIntlBytes *long_id,
                                                      QJSIntlBytes *short_id)
{
    if (long_id)
        *long_id = (QJSIntlBytes){0};
    if (short_id)
        *short_id = (QJSIntlBytes){0};
    if (!data || !long_id || !short_id || index >= data->identity_count)
        return QJS_INTL_INVALID_ARGUMENT;
    qjs_intl_data_string(&data->data, word(data->identities, (size_t)index * ID_WIDTH), long_id);
    qjs_intl_data_string(&data->data, word(data->identities, (size_t)index * ID_WIDTH + 1), short_id);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_timezone_presentation_zone(const QJSIntlTimeZonePresentationData *data,
                                                  uint32_t index, QJSIntlBytes *name,
                                                  uint32_t *start, uint32_t *count)
{
    if (name)
        *name = (QJSIntlBytes){0};
    if (start)
        *start = 0;
    if (count)
        *count = 0;
    if (!data || !name || !start || !count || index >= data->zone_count)
        return QJS_INTL_INVALID_ARGUMENT;
    qjs_intl_data_string(&data->data, word(data->zones, (size_t)index * ZONE_WIDTH), name);
    *start = word(data->zones, (size_t)index * ZONE_WIDTH + 1);
    *count = word(data->zones, (size_t)index * ZONE_WIDTH + 2);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_timezone_presentation_find_zone(const QJSIntlTimeZonePresentationData *data,
                                                       QJSIntlBytes name, uint32_t *out)
{
    size_t lo = 0, hi;
    if (out)
        *out = 0;
    if (!data || !out || (!name.data && name.length))
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->zone_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        QJSIntlBytes current;
        int order;
        qjs_intl_data_string(&data->data, word(data->zones, mid * ZONE_WIDTH), &current);
        order = compare_bytes(current, name);
        if (!order) {
            *out = (uint32_t)mid;
            return QJS_INTL_OK;
        }
        if (order < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_timezone_presentation_period(const QJSIntlTimeZonePresentationData *data,
                                                    uint32_t index, QJSIntlTimeZonePeriod *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!data || !out || index >= data->period_count)
        return QJS_INTL_INVALID_ARGUMENT;
    read_period(data->periods + (size_t)index * PERIOD_WIDTH * 4, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_timezone_presentation_at(const QJSIntlTimeZonePresentationData *data,
                                                uint32_t zone, int64_t instant,
                                                QJSIntlTimeZonePeriod *out)
{
    size_t lo, hi, start;
    QJSIntlTimeZonePeriod current;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!data || !out || zone >= data->zone_count)
        return QJS_INTL_INVALID_ARGUMENT;
    start = word(data->zones, (size_t)zone * ZONE_WIDTH + 1);
    lo = start;
    hi = start + word(data->zones, (size_t)zone * ZONE_WIDTH + 2);
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        read_period(data->periods + mid * PERIOD_WIDTH * 4, &current);
        if (!(current.flags & QJS_INTL_TIMEZONE_PERIOD_START) || current.start <= instant)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == start)
        return QJS_INTL_UNSUPPORTED;
    read_period(data->periods + (lo - 1) * PERIOD_WIDTH * 4, &current);
    if ((current.flags & QJS_INTL_TIMEZONE_PERIOD_END) && current.end <= instant)
        return QJS_INTL_UNSUPPORTED;
    *out = current;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_timezone_presentation_preferred(const QJSIntlTimeZonePresentationData *data,
                                                       uint32_t metazone, QJSIntlBytes territory,
                                                       QJSIntlBytes *out)
{
    size_t lo = 0, hi;
    if (out)
        *out = (QJSIntlBytes){0};
    if (!data || !out || metazone >= data->identity_count || (!territory.data && territory.length))
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->preferred_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const uint8_t *record = data->preferred + mid * 12;
        uint32_t key = word(record, 0);
        QJSIntlBytes region;
        int order;
        qjs_intl_data_string(&data->data, word(record, 1), &region);
        order = key != metazone ? (key < metazone ? -1 : 1) : compare_bytes(region, territory);
        if (!order) {
            qjs_intl_data_string(&data->data, word(data->zones, (size_t)word(record, 2) * ZONE_WIDTH), out);
            return QJS_INTL_OK;
        }
        if (order < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_timezone_presentation_primary(const QJSIntlTimeZonePresentationData *data,
                                                     QJSIntlBytes territory, QJSIntlBytes *out)
{
    size_t lo = 0, hi;
    if (out)
        *out = (QJSIntlBytes){0};
    if (!data || !out || (!territory.data && territory.length))
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->primary_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const uint8_t *record = data->primary + mid * 8;
        QJSIntlBytes region;
        int order;
        qjs_intl_data_string(&data->data, word(record, 0), &region);
        order = compare_bytes(region, territory);
        if (!order) {
            qjs_intl_data_string(&data->data, word(record, 1), out);
            return QJS_INTL_OK;
        }
        if (order < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return QJS_INTL_UNSUPPORTED;
}
