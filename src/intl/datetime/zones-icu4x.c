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
#include "provider.h"
#include "../../icu4x/zone-names.h"
#include "../icu4x/common.h"
#include <stdlib.h>
#include <string.h>

typedef struct Source {
    QJSIntlBytes locale;
    QJSICU4XZoneNames *handle;
} Source;

typedef struct Choice {
    QJSIntlBytes locale;
    uint32_t group;
    QJSIntlBytes names[3];
} Choice;

typedef struct Witness {
    QJSIntlBytes zone;
    int64_t instant;
} Witness;

typedef struct ZoneOwner {
    QJSIntlAllocator allocator;
    Source *sources;
    Choice *choices;
    size_t source_count, count;
} ZoneOwner;

static int compare_text(QJSIntlBytes a, QJSIntlBytes b)
{
    int result = memcmp(a.data, b.data, a.length < b.length ? a.length : b.length);
    return result ? result : a.length < b.length ? -1 : a.length != b.length;
}

static int compare_choice(const void *left, const void *right)
{
    const Choice *a = left, *b = right;
    return a->group < b->group ? -1 : a->group > b->group ? 1 : compare_text(a->locale, b->locale);
}

void qjs_intl_datetime_provider_zone_close(void *opaque)
{
    ZoneOwner *owner = opaque;
    size_t i;
    if (!owner)
        return;
    for (i = 0; i < owner->source_count; i++)
        qjs_icu4x_zone_names_close(owner->sources[i].handle);
    if (owner->choices)
        owner->allocator.free(owner->allocator.opaque, owner->choices);
    if (owner->sources)
        owner->allocator.free(owner->allocator.opaque, owner->sources);
    owner->allocator.free(owner->allocator.opaque, owner);
}

/* Each witness lies safely inside a post-horizon CLDR membership. Public ICU
   periods recover their own opaque metazone ID; no numeric ID crosses datasets.
   Old metazones without such a witness remain an explicit companion gap. */
static QJSIntlStatus witnesses(const QJSIntlTimeZonePresentationData *data, Witness *out)
{
    uint32_t zone;
    for (zone = 0; zone < data->zone_count; zone++) {
        QJSIntlBytes name;
        uint32_t start, count, i;
        QJSIntlStatus status =
            qjs_intl_timezone_presentation_zone(data, zone, &name, &start, &count);
        if (status)
            return status;
        for (i = 0; i < count; i++) {
            QJSIntlTimeZonePeriod period;
            int64_t instant = INT64_C(1451606400); /* 2016-01-01, after ICU's 2015 horizon. */
            status = qjs_intl_timezone_presentation_period(data, start + i, &period);
            if (status)
                return status;
            if (out[period.metazone].zone.length)
                continue;
            if ((period.flags & QJS_INTL_TIMEZONE_PERIOD_START) && period.start >= instant) {
                if (period.start > INT64_C(4102444800)) /* No far-future ID extrapolation. */
                    continue;
                instant = period.start + 86400;
            }
            if ((period.flags & QJS_INTL_TIMEZONE_PERIOD_END) && period.end <= instant + 86400)
                continue;
            out[period.metazone] = (Witness){name, instant};
        }
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus source(ZoneOwner *owner, QJSIntlBytes locale, QJSICU4XZoneNames **out)
{
    size_t i;
    QJSIntlStatus status;
    for (i = 0; i < owner->source_count; i++)
        if (!compare_text(owner->sources[i].locale, locale)) {
            *out = owner->sources[i].handle;
            return QJS_INTL_OK;
        }
    status = qjs_icu4x_status(
        qjs_icu4x_zone_names_open((const uint8_t *)locale.data, locale.length, out));
    if (status == QJS_INTL_UNSUPPORTED) {
        status = QJS_INTL_OK;
        *out = NULL;
    }
    if (!status)
        owner->sources[owner->source_count++] = (Source){locale, *out};
    return status;
}

static QJSIntlStatus prepare(ZoneOwner *owner, const QJSIntlDateZoneData *binding,
                             const Witness *witness, const QJSIntlTimeZoneNameNode *node)
{
    QJSIntlTimeZoneNameGroup group;
    Choice *choice;
    QJSICU4XZoneNames *handle = NULL;
    QJSIntlBytes zone;
    QJSIntlStatus status;
    int64_t instant = 0;
    unsigned metazone, kind;
    uint32_t i;
    size_t previous;
    for (previous = 0; previous < owner->count; previous++)
        if (owner->choices[previous].group == node->group &&
            !compare_text(owner->choices[previous].locale, node->origin_locale))
            return QJS_INTL_OK;
    status = qjs_intl_timezone_name_group_at(binding->names, node->group, &group);
    if (status)
        return status;
    if (group.family < QJS_INTL_TIMEZONE_NAME_ZONE || !group.width || group.entity_alt.length ||
        group.width_alt.length)
        return QJS_INTL_OK;
    zone = group.entity;
    metazone = group.family == QJS_INTL_TIMEZONE_NAME_METAZONE;
    if (metazone) {
        for (i = 0; i < binding->presentation->identity_count; i++) {
            QJSIntlBytes name, short_id;
            status =
                qjs_intl_timezone_presentation_identity(binding->presentation, i, &name, &short_id);
            if (status)
                return status;
            if (!compare_text(name, group.entity))
                break;
        }
        if (i == binding->presentation->identity_count || !witness[i].zone.length)
            return QJS_INTL_OK;
        zone = witness[i].zone;
        instant = witness[i].instant;
    }
    status = source(owner, node->origin_locale, &handle);
    if (status || !handle)
        return status;
    choice = &owner->choices[owner->count++];
    choice->locale = node->origin_locale;
    choice->group = node->group;
    for (kind = 0; kind < 3; kind++) {
        QJSICU4XDateText text;
        status = qjs_icu4x_status(qjs_icu4x_zone_names_text(handle, (const uint8_t *)zone.data,
                                                            zone.length, instant, metazone,
                                                            group.width, kind, &text));
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            return status;
        choice->names[kind] = (QJSIntlBytes){(const char *)text.data, text.length};
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus materialize(void *opaque, const QJSIntlTimeZoneNameNode *node,
                                 const QJSIntlTimeZoneNameEntry *entry, QJSIntlBytes *out)
{
    static const char *const kinds[] = {"generic", "standard", "daylight"};
    ZoneOwner *owner = opaque;
    Choice key = {0}, *choice;
    unsigned kind;
    *out = (QJSIntlBytes){0};
    if (entry->type || entry->alt.length)
        return QJS_INTL_UNSUPPORTED;
    for (kind = 0; kind < 3; kind++)
        if (entry->code.length == strlen(kinds[kind]) &&
            !memcmp(entry->code.data, kinds[kind], entry->code.length))
            break;
    if (kind == 3)
        return QJS_INTL_UNSUPPORTED;
    key.locale = node->origin_locale;
    key.group = node->group;
    choice = bsearch(&key, owner->choices, owner->count, sizeof(*owner->choices), compare_choice);
    /* ICU compaction loses some leaf provenance and generic/standard choices.
       Keep selected CLDR text unless the public span proves equivalence. */
    if (!choice || !choice->names[kind].length || compare_text(choice->names[kind], entry->text))
        return QJS_INTL_UNSUPPORTED;
    *out = choice->names[kind];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_provider_zone_open(const QJSIntlAllocator *allocator,
                                                   QJSIntlDateZoneData *binding, void **out)
{
    ZoneOwner *owner;
    Witness *witness = NULL;
    QJSIntlStatus status;
    uint32_t group;
    size_t capacity = 0;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || !binding ||
        !binding->names || !binding->presentation ||
        binding->names_locale >= binding->names->data.locales.count)
        return QJS_INTL_INVALID_ARGUMENT;
    for (group = 0; group < binding->names->group_count; group++) {
        QJSIntlTimeZoneNameGroup key;
        uint32_t head;
        status = qjs_intl_timezone_name_group_at(binding->names, group, &key);
        if (status)
            return status;
        if (key.family < QJS_INTL_TIMEZONE_NAME_ZONE)
            continue;
        status = qjs_intl_timezone_name_head(binding->names, binding->names_locale, group, &head);
        while (!status && head) {
            QJSIntlTimeZoneNameNode node;
            status = qjs_intl_timezone_name_node(binding->names, head, &node);
            if (!status) {
                if (capacity == SIZE_MAX)
                    return QJS_INTL_OVERFLOW;
                capacity++;
                head = node.parent;
            }
        }
        if (status)
            return status;
    }
    if (!capacity)
        return QJS_INTL_OK;
    if (capacity > SIZE_MAX / sizeof(Choice) || capacity > SIZE_MAX / sizeof(Source) ||
        (binding->presentation->identity_count &&
         sizeof(Witness) > SIZE_MAX / binding->presentation->identity_count))
        return QJS_INTL_OVERFLOW;
    owner = allocator->malloc(allocator->opaque, sizeof(*owner));
    if (!owner)
        return QJS_INTL_NO_MEMORY;
    *owner = (ZoneOwner){.allocator = *allocator};
    owner->choices = allocator->malloc(allocator->opaque, capacity * sizeof(*owner->choices));
    owner->sources = allocator->malloc(allocator->opaque, capacity * sizeof(*owner->sources));
    witness = allocator->malloc(allocator->opaque,
                                binding->presentation->identity_count * sizeof(*witness));
    if (!owner->choices || !owner->sources || !witness) {
        status = QJS_INTL_NO_MEMORY;
        goto done;
    }
    memset(owner->choices, 0, capacity * sizeof(*owner->choices));
    memset(witness, 0, binding->presentation->identity_count * sizeof(*witness));
    status = witnesses(binding->presentation, witness);
    for (group = 0; !status && group < binding->names->group_count; group++) {
        QJSIntlTimeZoneNameGroup key;
        uint32_t head;
        status = qjs_intl_timezone_name_group_at(binding->names, group, &key);
        if (status)
            break;
        if (key.family < QJS_INTL_TIMEZONE_NAME_ZONE)
            continue;
        status = qjs_intl_timezone_name_head(binding->names, binding->names_locale, group, &head);
        while (!status && head) {
            QJSIntlTimeZoneNameNode node;
            status = qjs_intl_timezone_name_node(binding->names, head, &node);
            if (!status)
                status = prepare(owner, binding, witness, &node);
            if (!status)
                head = node.parent;
        }
    }
    if (!status) {
        qsort(owner->choices, owner->count, sizeof(*owner->choices), compare_choice);
        binding->materialize = materialize;
        binding->materialize_opaque = owner;
        *out = owner;
    }
done:
    if (witness)
        allocator->free(allocator->opaque, witness);
    if (status)
        qjs_intl_datetime_provider_zone_close(owner);
    return status;
}
