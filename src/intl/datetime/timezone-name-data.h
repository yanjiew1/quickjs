/* Localized timezone presentation source, without zone or formatting policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_INTL_DATETIME_TIMEZONE_NAME_DATA_H
#define QJS_INTL_DATETIME_TIMEZONE_NAME_DATA_H
#include "../native/data.h"

typedef enum QJSIntlTimeZoneNameFamily {
    QJS_INTL_TIMEZONE_NAME_FORMAT,
    QJS_INTL_TIMEZONE_NAME_CITY,
    QJS_INTL_TIMEZONE_NAME_ZONE,
    QJS_INTL_TIMEZONE_NAME_METAZONE,
    QJS_INTL_TIMEZONE_NAME_FAMILY_COUNT
} QJSIntlTimeZoneNameFamily;

typedef enum QJSIntlTimeZoneNameWidth {
    QJS_INTL_TIMEZONE_NAME_NO_WIDTH,
    QJS_INTL_TIMEZONE_NAME_LONG,
    QJS_INTL_TIMEZONE_NAME_SHORT,
    QJS_INTL_TIMEZONE_NAME_WIDTH_COUNT
} QJSIntlTimeZoneNameWidth;

typedef enum QJSIntlTimeZoneNameType {
    QJS_INTL_TIMEZONE_NAME_NO_TYPE,
    QJS_INTL_TIMEZONE_NAME_STANDARD,
    QJS_INTL_TIMEZONE_NAME_DAYLIGHT,
    QJS_INTL_TIMEZONE_NAME_TYPE_COUNT
} QJSIntlTimeZoneNameType;

typedef struct QJSIntlTimeZoneNameGroup {
    QJSIntlTimeZoneNameFamily family;
    QJSIntlBytes entity, entity_alt;
    QJSIntlTimeZoneNameWidth width;
    QJSIntlBytes width_alt;
} QJSIntlTimeZoneNameGroup;

typedef struct QJSIntlTimeZoneNameNode {
    QJSIntlBytes origin_locale;
    uint32_t group, map, parent; /* Zero-based group/map; one-based parent. */
} QJSIntlTimeZoneNameNode;

typedef struct QJSIntlTimeZoneNameEntry {
    QJSIntlBytes code, alt, text;
    QJSIntlTimeZoneNameType type;
    unsigned int blocked;
} QJSIntlTimeZoneNameEntry;

typedef struct QJSIntlTimeZoneNameRows {
    const uint8_t *locale_rows, *rows;
    uint32_t group_start, group_count, row_count;
} QJSIntlTimeZoneNameRows;

typedef struct QJSIntlTimeZoneNameData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *groups, *maps, *nodes, *entries;
    uint32_t group_count, map_count, node_count, entry_count;
    QJSIntlTimeZoneNameRows families[QJS_INTL_TIMEZONE_NAME_FAMILY_COUNT];
} QJSIntlTimeZoneNameData;

/* Borrow an unchanged validated shared owner. Require malloc/free and release
   all validation scratch before return. Follow pack.h's disjoint-output
   contract; reset non-NULL output on failure. Validate physical source records,
   not format/hour-template grammar or formatter capability. Source collection
   owns grammar checks; consumers must compile templates before rendering. */
QJSIntlStatus qjs_intl_timezone_name_data_open(const QJSIntlData *data,
                                               const QJSIntlAllocator *allocator,
                                               QJSIntlTimeZoneNameData *out);
/* Queries allocate nothing. Returned bytes borrow pack storage. Group/map
   indexes are zero-based; heads/parents are one-based, zero meaning missing. */
QJSIntlStatus qjs_intl_timezone_name_group_at(const QJSIntlTimeZoneNameData *data,
                                              uint32_t index, QJSIntlTimeZoneNameGroup *out);
QJSIntlStatus qjs_intl_timezone_name_find_group(const QJSIntlTimeZoneNameData *data,
                                                const QJSIntlTimeZoneNameGroup *key, uint32_t *out);
QJSIntlStatus qjs_intl_timezone_name_head(const QJSIntlTimeZoneNameData *data,
                                          uint32_t locale, uint32_t group, uint32_t *out);
QJSIntlStatus qjs_intl_timezone_name_node(const QJSIntlTimeZoneNameData *data,
                                          uint32_t index, QJSIntlTimeZoneNameNode *out);
/* Exact lookup in one source layer, without parent/alternate fallback. Missing
   returns UNSUPPORTED and zero output; blocked/empty return OK. Zone-name and
   metazone-name aliases are compatible, but format/city classes are separate. */
QJSIntlStatus qjs_intl_timezone_name_lookup(const QJSIntlTimeZoneNameData *data,
                                            uint32_t index, QJSIntlBytes code,
                                            QJSIntlTimeZoneNameType type, QJSIntlBytes alt,
                                            QJSIntlTimeZoneNameEntry *out);
/* Resolve one exact key along the prepared chain; aliases retain their source
   identity. BLOCK/empty stop inheritance. Zero head means missing. No alternate,
   width, type or zone fallback. Allocate nothing, borrow unchanged pack text,
   reset disjoint outputs on failure. */
QJSIntlStatus qjs_intl_timezone_name_resolve(const QJSIntlTimeZoneNameData *data, uint32_t head,
                                             QJSIntlBytes code, QJSIntlTimeZoneNameType type,
                                             QJSIntlBytes alt, QJSIntlTimeZoneNameEntry *out,
                                             QJSIntlTimeZoneNameNode *origin);
#endif
