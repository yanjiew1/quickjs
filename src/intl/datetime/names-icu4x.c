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
#include "calendar-identity.h"
#include "../../icu4x/datetime-names.h"
#include "../icu4x/common.h"
#include "../../calendar/calendar.h"
#include <string.h>

typedef struct NameSource {
    QJSIntlBytes locale;
    uint32_t group;
    QJSCalendarId calendar;
    QJSIntlCalendarNameFamily family;
    QJSICU4XDateNames *handle;
    QJSICU4XDateNamesMeta meta;
    size_t start;
    QJSIntlBytes adar[2];
    char *owned[3];
    QJSIntlBytes leap_template;
} NameSource;

typedef struct EraChoice {
    QJSCalendarId calendar;
    QJSIntlBytes code;
    size_t index;
} EraChoice;

typedef struct NameOwner {
    QJSIntlAllocator allocator;
    const QJSIntlCalendarNameData *data;
    const QJSIntlLocaleMetadata *metadata;
    const QJSIntlCalendarEraData *eras;
    NameSource *sources;
    QJSIntlBytes *choices;
    EraChoice *era_choices;
    size_t count, era_count;
} NameOwner;

static QJSIntlBytes bytes(const char *s)
{
    return (QJSIntlBytes){s, strlen(s)};
}

static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}

void qjs_intl_datetime_provider_text_close(void *opaque)
{
    NameOwner *owner = opaque;
    size_t i;
    unsigned j;
    if (!owner)
        return;
    for (i = 0; i < owner->count; i++) {
        qjs_icu4x_datetime_names_close(owner->sources[i].handle);
        for (j = 0; j < 3; j++)
            if (owner->sources[i].owned[j])
                owner->allocator.free(owner->allocator.opaque, owner->sources[i].owned[j]);
    }
    if (owner->era_choices)
        owner->allocator.free(owner->allocator.opaque, owner->era_choices);
    if (owner->choices)
        owner->allocator.free(owner->allocator.opaque, owner->choices);
    if (owner->sources)
        owner->allocator.free(owner->allocator.opaque, owner->sources);
    owner->allocator.free(owner->allocator.opaque, owner);
}

static QJSIntlStatus calendar_id(const NameOwner *owner, QJSIntlBytes name, QJSCalendarId *out)
{
    QJSIntlLocaleType type;
    QJSIntlStatus status;
    uint32_t index;
    unsigned id;
    status = qjs_intl_locale_type_identity(owner->metadata, bytes("u"), bytes("ca"), name, &index);
    if (!status)
        status = qjs_intl_locale_type_at(owner->metadata, index, &type);
    if (status)
        return status;
    /* All Hijri name markers share a payload. Keep the deprecated source
       spelling aligned with the core's tabular fallback, without doing dates. */
    if (equal(type.name, bytes("islamic"))) {
        *out = QJS_CAL_ISLAMIC_TBLA;
        return QJS_INTL_OK;
    }
    for (id = 0; id < QJS_CAL_COUNT; id++) {
        if (equal(type.name, bytes(qjs_calendar_identifier((QJSCalendarId)id)))) {
            *out = (QJSCalendarId)id;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

static QJSIntlStatus source_calendar(const NameOwner *owner, uint32_t source, QJSCalendarId *out)
{
    QJSIntlBytes name;
    QJSIntlStatus status = qjs_intl_calendar_name_calendar_at(owner->data, source, &name);
    return status ? status : calendar_id(owner, name, out);
}

static QJSCalendarId era_calendar(QJSCalendarId calendar)
{
    if (calendar == QJS_CAL_ISO8601)
        return QJS_CAL_GREGORY;
    if (calendar == QJS_CAL_ETHIOAA)
        return QJS_CAL_ETHIOPIC;
    if (calendar >= QJS_CAL_ISLAMIC_CIVIL && calendar <= QJS_CAL_ISLAMIC_UMALQURA)
        return QJS_CAL_ISLAMIC_TBLA;
    return calendar;
}

static QJSIntlStatus prepare_eras(NameOwner *owner)
{
    uint32_t i;
    size_t capacity = owner->eras->section.count;
    if (capacity > SIZE_MAX / sizeof(*owner->era_choices))
        return QJS_INTL_OVERFLOW;
    if (!owner->eras->section.count)
        return QJS_INTL_OK;
    for (i = 0; i < owner->count; i++)
        if (owner->sources[i].handle && owner->sources[i].family == QJS_INTL_CAL_NAME_ERA &&
            owner->sources[i].meta.kind == QJS_ICU4X_DATE_ERAS)
            break;
    if (i == owner->count)
        return QJS_INTL_OK;
    owner->era_choices = owner->allocator.malloc(
        owner->allocator.opaque, owner->eras->section.count * sizeof(*owner->era_choices));
    if (!owner->era_choices)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i < owner->eras->section.count; i++) {
        QJSIntlCalendarEraEntry entry;
        QJSCalendarId calendar;
        size_t index = 0, j;
        QJSIntlStatus status = qjs_intl_calendar_era_at(owner->eras, i, &entry);
        if (!status)
            status = calendar_id(owner, entry.calendar, &calendar);
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            return status;
        calendar = era_calendar(calendar);
        for (j = 0; j < owner->count; j++)
            if (owner->sources[j].handle && owner->sources[j].family == QJS_INTL_CAL_NAME_ERA &&
                owner->sources[j].meta.kind == QJS_ICU4X_DATE_ERAS &&
                era_calendar(owner->sources[j].calendar) == calendar)
                break;
        if (j == owner->count)
            continue;
        for (j = 0; j < owner->era_count; j++)
            if (owner->era_choices[j].calendar == calendar &&
                equal(owner->era_choices[j].code, entry.code))
                break;
        if (j < owner->era_count)
            continue;
        status = qjs_icu4x_status(qjs_icu4x_datetime_era_index(
            calendar, (const uint8_t *)entry.code.data, entry.code.length, &index));
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            return status;
        owner->era_choices[owner->era_count++] = (EraChoice){calendar, entry.code, index};
    }
    return QJS_INTL_OK;
}

static int relevant(const QJSIntlCalendarNameGroup *group, uint32_t primary, uint32_t gregorian)
{
    if (group->calendar != primary &&
        !(group->calendar == gregorian && group->family == QJS_INTL_CAL_NAME_ERA))
        return 0;
    return group->family == QJS_INTL_CAL_NAME_MONTH_PATTERN ||
           group->family == QJS_INTL_CAL_NAME_ERA || group->family == QJS_INTL_CAL_NAME_MONTH ||
           group->family == QJS_INTL_CAL_NAME_DAY ||
           group->family == QJS_INTL_CAL_NAME_DAY_PERIOD ||
           (group->family == QJS_INTL_CAL_NAME_CYCLIC &&
            group->cycle == QJS_INTL_CAL_NAME_CYCLE_YEAR);
}

static QJSIntlStatus prepare_source(NameOwner *owner, const QJSIntlCalendarNameNode *node)
{
    static const uint32_t widths[] = {3, 5, 4, 6, 1};
    QJSIntlCalendarNameGroup group;
    NameSource *source;
    QJSIntlStatus status;
    uint32_t family;
    size_t i;
    for (i = 0; i < owner->count; i++)
        if (owner->sources[i].group == node->group &&
            equal(owner->sources[i].locale, node->origin_locale))
            return QJS_INTL_OK;
    source = &owner->sources[owner->count++];
    source->group = node->group;
    source->locale = node->origin_locale;
    status = qjs_intl_calendar_name_group_at(owner->data, node->group, &group);
    if (status)
        return status;
    source->family = group.family;
    if (group.family == QJS_INTL_CAL_NAME_DAY || group.family == QJS_INTL_CAL_NAME_DAY_PERIOD) {
        source->calendar = QJS_CAL_ISO8601; /* These public markers are calendar-independent. */
        family = group.family == QJS_INTL_CAL_NAME_DAY ? 2 : 3;
    } else {
        status = source_calendar(owner, group.calendar, &source->calendar);
        if (status == QJS_INTL_UNSUPPORTED)
            return QJS_INTL_OK;
        if (status)
            return status;
        family = group.family == QJS_INTL_CAL_NAME_MONTH ||
                         group.family == QJS_INTL_CAL_NAME_MONTH_PATTERN
                     ? 1
                     : 0;
    }
    status = qjs_icu4x_status(qjs_icu4x_datetime_names_open(
        (const uint8_t *)source->locale.data, source->locale.length, source->calendar, family,
        widths[group.width], group.context == QJS_INTL_CAL_NAME_STAND_ALONE, &source->handle));
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (!status)
        status = qjs_icu4x_status(qjs_icu4x_datetime_names_meta(source->handle, &source->meta));
    return status;
}

static QJSIntlBytes raw_text(QJSICU4XDateText value)
{
    return (QJSIntlBytes){(const char *)value.data, value.length};
}

static QJSIntlStatus decorate(NameOwner *owner, NameSource *source, unsigned phase,
                              QJSIntlBytes base, QJSIntlBytes *out, char **owned)
{
    QJSICU4XDateMonthPattern pattern = {0};
    QJSIntlBytes prefix, suffix;
    QJSIntlStatus status =
        qjs_icu4x_status(qjs_icu4x_datetime_month_pattern(source->handle, phase, &pattern));
    size_t length;
    char *storage;
    if (status)
        return status;
    /* This path decorates lexical names, not numeric month coefficients. */
    if (pattern.offset)
        return QJS_INTL_UNSUPPORTED;
    prefix = raw_text(pattern.pattern.prefix);
    suffix = raw_text(pattern.pattern.suffix);
    if (!pattern.pattern.has_value) {
        *out = prefix;
        return QJS_INTL_OK;
    }
    if (!base.length)
        return QJS_INTL_UNSUPPORTED;
    if (!prefix.length && !suffix.length) {
        *out = base;
        return QJS_INTL_OK;
    }
    if (prefix.length > SIZE_MAX - base.length ||
        prefix.length + base.length > SIZE_MAX - suffix.length)
        return QJS_INTL_OVERFLOW;
    length = prefix.length + base.length + suffix.length;
    storage = owner->allocator.malloc(owner->allocator.opaque, length);
    if (!storage)
        return QJS_INTL_NO_MEMORY;
    if (prefix.length)
        memcpy(storage, prefix.data, prefix.length);
    if (base.length)
        memcpy(storage + prefix.length, base.data, base.length);
    if (suffix.length)
        memcpy(storage + prefix.length + base.length, suffix.data, suffix.length);
    *owned = storage;
    *out = (QJSIntlBytes){storage, length};
    return QJS_INTL_OK;
}

/* The C template grammar has one {0} and literal affixes. Re-encode only
   exactly representable public patterns; offsets and literal braces keep
   their explicit source form instead of changing nominal-month semantics. */
static QJSIntlStatus prepare_template(NameOwner *owner, NameSource *source)
{
    QJSICU4XDateMonthPattern pattern = {0};
    QJSIntlBytes prefix, suffix;
    QJSIntlStatus status =
        qjs_icu4x_status(qjs_icu4x_datetime_month_pattern(source->handle, 1, &pattern));
    size_t length;
    char *storage;
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (status)
        return status;
    if (pattern.offset || !pattern.pattern.has_value)
        return QJS_INTL_OK;
    prefix = raw_text(pattern.pattern.prefix);
    suffix = raw_text(pattern.pattern.suffix);
    if ((prefix.length &&
         (memchr(prefix.data, '{', prefix.length) || memchr(prefix.data, '}', prefix.length))) ||
        (suffix.length &&
         (memchr(suffix.data, '{', suffix.length) || memchr(suffix.data, '}', suffix.length))))
        return QJS_INTL_OK;
    if (!prefix.length && !suffix.length) {
        source->leap_template = bytes("{0}");
        return QJS_INTL_OK;
    }
    if (prefix.length > SIZE_MAX - suffix.length || prefix.length + suffix.length > SIZE_MAX - 3)
        return QJS_INTL_OVERFLOW;
    length = prefix.length + suffix.length + 3;
    storage = owner->allocator.malloc(owner->allocator.opaque, length);
    if (!storage)
        return QJS_INTL_NO_MEMORY;
    if (prefix.length)
        memcpy(storage, prefix.data, prefix.length);
    memcpy(storage + prefix.length, "{0}", 3);
    if (suffix.length)
        memcpy(storage + prefix.length + 3, suffix.data, suffix.length);
    source->owned[2] = storage;
    source->leap_template = (QJSIntlBytes){storage, length};
    return QJS_INTL_OK;
}

static QJSIntlStatus snapshot(NameOwner *owner)
{
    size_t i, j, count = 0;
    QJSIntlStatus status;
    for (i = 0; i < owner->count; i++) {
        NameSource *source = &owner->sources[i];
        source->start = count;
        if (source->meta.count > SIZE_MAX - count)
            return QJS_INTL_OVERFLOW;
        count += source->meta.count;
    }
    if (count > SIZE_MAX / sizeof(*owner->choices))
        return QJS_INTL_OVERFLOW;
    if (count) {
        owner->choices =
            owner->allocator.malloc(owner->allocator.opaque, count * sizeof(*owner->choices));
        if (!owner->choices)
            return QJS_INTL_NO_MEMORY;
    }
    for (i = 0; i < owner->count; i++) {
        NameSource *source = &owner->sources[i];
        for (j = 0; j < source->meta.count; j++) {
            QJSICU4XDateText text = {0};
            status = qjs_icu4x_status(qjs_icu4x_datetime_names_text(source->handle, j, &text));
            if (status)
                return status;
            owner->choices[source->start + j] = raw_text(text);
        }
        if (source->handle && source->family == QJS_INTL_CAL_NAME_MONTH_PATTERN) {
            status = prepare_template(owner, source);
            if (status)
                return status;
        }
        if (source->calendar == QJS_CAL_HEBREW && source->family == QJS_INTL_CAL_NAME_MONTH &&
            source->meta.kind == QJS_ICU4X_DATE_MONTH_LEAP && source->meta.count >= 6) {
            for (j = 0; j < 2; j++) {
                status =
                    decorate(owner, source, (unsigned)j + 1, owner->choices[source->start + 4 + j],
                             &source->adar[j], &source->owned[j]);
                if (status && status != QJS_INTL_UNSUPPORTED)
                    return status;
            }
        }
    }
    return QJS_INTL_OK;
}

static int number(QJSIntlBytes key, size_t *out)
{
    size_t i, value = 0;
    if (!key.length || key.length > 3)
        return 0;
    for (i = 0; i < key.length; i++) {
        if (key.data[i] < '0' || key.data[i] > '9')
            return 0;
        value = value * 10 + key.data[i] - '0';
    }
    if (!value)
        return 0;
    *out = value;
    return 1;
}

static QJSIntlStatus materialize(void *opaque, const QJSIntlDateCalendarFields *local,
                                 const QJSIntlDatePatternToken *token,
                                 const QJSIntlDateNameSelection *selected, QJSIntlBytes *out)
{
    static const char *weekdays[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
    static const char *periods[] = {"am", "pm", "noon", "midnight"};
    const NameOwner *owner = opaque;
    const NameSource *source = NULL;
    QJSIntlBytes choice = {0};
    size_t i, index = SIZE_MAX;
    (void)token;
    for (i = 0; i < owner->count; i++) {
        if (owner->sources[i].group == selected->origin.group &&
            equal(owner->sources[i].locale, selected->origin.origin_locale)) {
            source = &owner->sources[i];
            break;
        }
    }
    if (!source || !source->handle)
        return QJS_INTL_UNSUPPORTED;
    switch (source->family) {
    case QJS_INTL_CAL_NAME_ERA:
        if (source->meta.kind != QJS_ICU4X_DATE_ERAS)
            return QJS_INTL_UNSUPPORTED;
        for (i = 0; i < owner->era_count; i++) {
            if (owner->era_choices[i].calendar == era_calendar(source->calendar) &&
                equal(owner->era_choices[i].code, bytes(local->era_code))) {
                index = owner->era_choices[i].index;
                break;
            }
        }
        break;
    case QJS_INTL_CAL_NAME_CYCLIC:
        if (source->meta.kind != QJS_ICU4X_DATE_CYCLIC || !number(selected->entry.code, &index))
            return QJS_INTL_UNSUPPORTED;
        index--;
        break;
    case QJS_INTL_CAL_NAME_MONTH:
        if (!number(selected->entry.code, &index))
            return QJS_INTL_UNSUPPORTED;
        if (source->calendar == QJS_CAL_HEBREW) {
            if (index == 6) {
                index = SIZE_MAX;
                choice = source->adar[0];
                break;
            }
            if (index == 7 && local->in_leap_year) {
                index = SIZE_MAX;
                choice = source->adar[1];
                break;
            }
            if (index >= 7)
                index--;
        }
        index--;
        break;
    case QJS_INTL_CAL_NAME_MONTH_PATTERN:
        if (!equal(selected->entry.code, bytes("leap")))
            return QJS_INTL_UNSUPPORTED;
        choice = source->leap_template;
        break;
    case QJS_INTL_CAL_NAME_DAY:
        for (i = 0; i < sizeof(weekdays) / sizeof(weekdays[0]); i++)
            if (equal(selected->entry.code, bytes(weekdays[i])))
                index = i;
        break;
    case QJS_INTL_CAL_NAME_DAY_PERIOD:
        for (i = 0; i < sizeof(periods) / sizeof(periods[0]); i++)
            if (equal(selected->entry.code, bytes(periods[i])))
                index = i;
        break;
    default:
        return QJS_INTL_UNSUPPORTED;
    }
    if (!choice.length && index < source->meta.count)
        choice = owner->choices[source->start + index];
    /* Exact-source payloads still lose individual leaf/case/alias provenance.
       Only a matching selected leaf can replace its backing representation.
       Missing public data, transformed Hebrew numeric labels and flexible
       periods retain the explicit matching-CLDR source, never another parent. */
    if (!choice.length || !equal(choice, selected->entry.text))
        return QJS_INTL_UNSUPPORTED;
    *out = choice;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_provider_text_open(const QJSIntlAllocator *allocator,
                                                   QJSIntlDateTextData *binding,
                                                   QJSCalendarId calendar, void **out)
{
    NameOwner *owner;
    QJSIntlStatus status;
    uint32_t group, gregorian = UINT32_MAX;
    size_t capacity = 0;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || !binding ||
        !binding->names || !binding->metadata || !binding->eras ||
        binding->calendar >= binding->names->calendar_count ||
        binding->locale >= binding->names->data.locales.count ||
        (unsigned)calendar >= QJS_CAL_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    if (calendar == QJS_CAL_JAPANESE) {
        status = qjs_intl_date_name_calendar_index(binding->metadata, binding->names,
                                                   bytes("gregory"), &gregorian);
        if (status && status != QJS_INTL_UNSUPPORTED)
            return status;
    }
    for (group = 0; group < binding->names->group_count; group++) {
        QJSIntlCalendarNameGroup key;
        uint32_t head;
        status = qjs_intl_calendar_name_group_at(binding->names, group, &key);
        if (status)
            return status;
        if (!relevant(&key, binding->calendar, gregorian))
            continue;
        status = qjs_intl_calendar_name_head(binding->names, binding->locale, group, &head);
        while (!status && head) {
            QJSIntlCalendarNameNode node;
            status = qjs_intl_calendar_name_node(binding->names, head, &node);
            if (status)
                break;
            if (capacity == SIZE_MAX)
                return QJS_INTL_OVERFLOW;
            capacity++;
            head = node.parent;
        }
        if (status)
            return status;
    }
    if (!capacity)
        return QJS_INTL_OK;
    if (capacity > SIZE_MAX / sizeof(*owner->sources))
        return QJS_INTL_OVERFLOW;
    owner = allocator->malloc(allocator->opaque, sizeof(*owner));
    if (!owner)
        return QJS_INTL_NO_MEMORY;
    memset(owner, 0, sizeof(*owner));
    owner->allocator = *allocator;
    owner->data = binding->names;
    owner->metadata = binding->metadata;
    owner->eras = binding->eras;
    owner->sources = allocator->malloc(allocator->opaque, capacity * sizeof(*owner->sources));
    if (!owner->sources) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    memset(owner->sources, 0, capacity * sizeof(*owner->sources));
    for (group = 0; group < binding->names->group_count; group++) {
        QJSIntlCalendarNameGroup key;
        uint32_t head;
        status = qjs_intl_calendar_name_group_at(binding->names, group, &key);
        if (status)
            goto fail;
        if (!relevant(&key, binding->calendar, gregorian))
            continue;
        status = qjs_intl_calendar_name_head(binding->names, binding->locale, group, &head);
        while (!status && head) {
            QJSIntlCalendarNameNode node;
            status = qjs_intl_calendar_name_node(binding->names, head, &node);
            if (!status)
                status = prepare_source(owner, &node);
            if (status)
                break;
            head = node.parent;
        }
        if (status)
            goto fail;
    }
    status = snapshot(owner);
    if (!status)
        status = prepare_eras(owner);
    if (status)
        goto fail;
    binding->materialize = materialize;
    binding->materialize_opaque = owner;
    *out = owner;
    return QJS_INTL_OK;
fail:
    qjs_intl_datetime_provider_text_close(owner);
    return status;
}
