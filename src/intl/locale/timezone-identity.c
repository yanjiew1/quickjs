/* CLDR TR35 Time Zone Identifiers: first alias and short-ID region rules.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "timezone-identity.h"
#include "../common/timezone-syntax.h"
#include <string.h>

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static QJSIntlStatus project(const QJSIntlLocaleMetadata *metadata, uint32_t index,
                             QJSIntlTimeZoneIdentity *out)
{
    QJSIntlTimeZoneIdentity result = {0};
    QJSIntlLocaleType type;
    QJSIntlStatus status = qjs_intl_locale_type_at(metadata, index, &type);
    size_t length;
    if (status)
        return status;
    if (!equal(type.extension, "u") || !equal(type.key, "tz") || type.variable ||
        type.preferred.length || !type.aliases.length)
        return QJS_INTL_UNSUPPORTED;
    for (length = 0; length < type.aliases.length && type.aliases.data[length] != ' '; length++)
        continue;
    result.identifier = (QJSIntlBytes){type.aliases.data, length};
    if (!qjs_intl_timezone_source_identity_valid(result.identifier, 0))
        return QJS_INTL_DATA_ERROR;
    if (type.region.length) {
        if (type.region.length != 2)
            return QJS_INTL_DATA_ERROR;
        memcpy(result.territory, type.region.data, 2);
    } else if (type.name.length == 5) {
        unsigned int i;
        for (i = 0; i < 2; i++) {
            unsigned int c = (uint8_t)type.name.data[i];
            if (c < 'a' || c > 'z')
                return QJS_INTL_DATA_ERROR;
            result.territory[i] = c - 'a' + 'A';
        }
    }
    result.short_id = type.name;
    result.metadata_index = index;
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_timezone_identity(const QJSIntlLocaleMetadata *metadata,
                                        QJSIntlBytes spelling, QJSIntlTimeZoneIdentity *out)
{
    QJSIntlStatus status;
    uint32_t index;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_locale_type_identity(metadata, (QJSIntlBytes){"u", 1},
                                          (QJSIntlBytes){"tz", 2}, spelling, &index);
    return status ? status : project(metadata, index, out);
}

QJSIntlStatus qjs_intl_timezone_region_count(const QJSIntlLocaleMetadata *metadata,
                                            QJSIntlBytes territory, uint32_t *out)
{
    uint32_t i, count = 0;
    size_t j;
    if (out)
        *out = 0;
    if (!out || !metadata || !metadata->types.bytes || !territory.data ||
        (territory.length != 2 && territory.length != 3))
        return QJS_INTL_INVALID_ARGUMENT;
    for (j = 0; j < territory.length; j++) {
        unsigned int c = (uint8_t)territory.data[j];
        if (territory.length == 2 ? c < 'A' || c > 'Z' : c < '0' || c > '9')
            return QJS_INTL_INVALID_ARGUMENT;
    }
    for (i = 0; i < metadata->types.count; i++) {
        QJSIntlTimeZoneIdentity identity;
        QJSIntlStatus status = project(metadata, i, &identity);
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            return status;
        if (territory.length == 2 && !memcmp(identity.territory, territory.data, 2))
            count++;
    }
    *out = count;
    return QJS_INTL_OK;
}
