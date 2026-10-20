/* Sparse localized calendar names, without formatter fallback or chronology.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_NAME_DATA_H
#define QJS_INTL_DATETIME_NAME_DATA_H
#include "../native/data.h"

typedef enum QJSIntlCalendarNameFamily {
    QJS_INTL_CAL_NAME_CYCLIC,
    QJS_INTL_CAL_NAME_DAY_PERIOD,
    QJS_INTL_CAL_NAME_DAY,
    QJS_INTL_CAL_NAME_ERA,
    QJS_INTL_CAL_NAME_MONTH_PATTERN,
    QJS_INTL_CAL_NAME_MONTH,
    QJS_INTL_CAL_NAME_QUARTER,
    QJS_INTL_CAL_NAME_FAMILY_COUNT
} QJSIntlCalendarNameFamily;

typedef enum QJSIntlCalendarNameCycle {
    QJS_INTL_CAL_NAME_NO_CYCLE,
    QJS_INTL_CAL_NAME_CYCLE_YEAR,
    QJS_INTL_CAL_NAME_CYCLE_MONTH,
    QJS_INTL_CAL_NAME_CYCLE_DAY,
    QJS_INTL_CAL_NAME_CYCLE_DAY_PART,
    QJS_INTL_CAL_NAME_CYCLE_ZODIAC,
    QJS_INTL_CAL_NAME_CYCLE_SOLAR_TERM,
    QJS_INTL_CAL_NAME_CYCLE_COUNT
} QJSIntlCalendarNameCycle;

typedef enum QJSIntlCalendarNameContext {
    QJS_INTL_CAL_NAME_NO_CONTEXT,
    QJS_INTL_CAL_NAME_FORMAT,
    QJS_INTL_CAL_NAME_STAND_ALONE,
    QJS_INTL_CAL_NAME_NUMERIC,
    QJS_INTL_CAL_NAME_CONTEXT_COUNT
} QJSIntlCalendarNameContext;

typedef enum QJSIntlCalendarNameWidth {
    QJS_INTL_CAL_NAME_ABBREVIATED,
    QJS_INTL_CAL_NAME_NARROW,
    QJS_INTL_CAL_NAME_WIDE,
    QJS_INTL_CAL_NAME_SHORT,
    QJS_INTL_CAL_NAME_ALL,
    QJS_INTL_CAL_NAME_WIDTH_COUNT
} QJSIntlCalendarNameWidth;

typedef enum QJSIntlCalendarNameYear {
    QJS_INTL_CAL_NAME_NO_YEAR,
    QJS_INTL_CAL_NAME_STANDARD_YEAR,
    QJS_INTL_CAL_NAME_LEAP_YEAR,
    QJS_INTL_CAL_NAME_YEAR_COUNT
} QJSIntlCalendarNameYear;

typedef struct QJSIntlCalendarNameGroup {
    uint32_t calendar;
    QJSIntlCalendarNameFamily family;
    QJSIntlCalendarNameCycle cycle;
    QJSIntlCalendarNameContext context;
    QJSIntlCalendarNameWidth width;
} QJSIntlCalendarNameGroup;

typedef struct QJSIntlCalendarNameNode {
    QJSIntlBytes origin_locale;
    uint32_t group, map, parent; /* Zero-based group/map; one-based parent. */
} QJSIntlCalendarNameNode;

typedef struct QJSIntlCalendarNameEntry {
    QJSIntlBytes code, alt, text;
    QJSIntlCalendarNameYear year;
    unsigned int blocked;
} QJSIntlCalendarNameEntry;

typedef struct QJSIntlCalendarNameData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *calendars, *groups, *locale_rows, *rows, *maps, *nodes, *entries;
    uint32_t calendar_count, group_count, row_count, map_count, node_count, entry_count;
} QJSIntlCalendarNameData;

/* Borrow an unchanged validated shared owner. Require malloc/free; all scratch
   is freed before return. Follow pack.h's disjoint-output contract. Non-NULL
   output is reset on failure. No calendar arithmetic capability is inferred. */
QJSIntlStatus qjs_intl_calendar_name_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                               QJSIntlCalendarNameData *);
/* Queries borrow storage and allocate nothing. Calendar/group/map indexes are
   zero-based; heads/parents are one-based, with zero meaning no source layer. */
QJSIntlStatus qjs_intl_calendar_name_calendar_at(const QJSIntlCalendarNameData *, uint32_t,
                                                 QJSIntlBytes *);
QJSIntlStatus qjs_intl_calendar_name_group_at(const QJSIntlCalendarNameData *, uint32_t,
                                              QJSIntlCalendarNameGroup *);
QJSIntlStatus qjs_intl_calendar_name_find_group(const QJSIntlCalendarNameData *,
                                                const QJSIntlCalendarNameGroup *, uint32_t *);
QJSIntlStatus qjs_intl_calendar_name_head(const QJSIntlCalendarNameData *, uint32_t locale,
                                          uint32_t group, uint32_t *head);
QJSIntlStatus qjs_intl_calendar_name_node(const QJSIntlCalendarNameData *, uint32_t node,
                                          QJSIntlCalendarNameNode *);
/* Exact lookup in one source layer, without walking parents or selecting
   alternate/year/width/context fallbacks. Missing returns UNSUPPORTED with a
   zero output; BLOCK and explicit empty text return OK. Aliases may change the
   source group along the chain while retaining its semantic family. */
QJSIntlStatus qjs_intl_calendar_name_lookup(const QJSIntlCalendarNameData *, uint32_t node,
                                            QJSIntlBytes code, QJSIntlCalendarNameYear,
                                            QJSIntlBytes alt, QJSIntlCalendarNameEntry *);
/* Resolve that exact key along the prepared parent chain, retaining the first
   entry and its actual origin group. BLOCK and empty text stop inheritance.
   Choose no year/alternate/width/context/calendar fallback. Zero head means
   missing (UNSUPPORTED). Allocate nothing and borrow unchanged pack bytes;
   require disjoint outputs and reset all non-NULL outputs on failure. */
QJSIntlStatus qjs_intl_calendar_name_resolve(const QJSIntlCalendarNameData *, uint32_t head,
                                             QJSIntlBytes code, QJSIntlCalendarNameYear,
                                             QJSIntlBytes alt, QJSIntlCalendarNameEntry *,
                                             QJSIntlCalendarNameNode *origin);
#endif
