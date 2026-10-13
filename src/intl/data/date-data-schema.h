/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATE_DATA_SCHEMA_H
#define QJS_INTL_DATE_DATA_SCHEMA_H
/* Additive wire1.3 reservation. Existing IDs/widths are unchanged.
 * This header does not cause the sealed generic reader to admit these IDs.
 */
enum {
    QJS_INTL_DATA_DATE_PATTERN = 100,
    QJS_INTL_DATA_DATE_NAME = 101,
    QJS_INTL_DATA_DATE_SYMBOL = 102,
    QJS_INTL_DATA_DATE_PERIOD_RULE = 103,
    QJS_INTL_DATA_DATE_ZONE_NAME = 104,
    QJS_INTL_DATA_DATE_META_PERIOD = 105,
    QJS_INTL_DATA_DATE_ZONE_ALIAS = 106,
    QJS_INTL_DATA_DATE_RANGE_FALLBACK = 107,
    QJS_INTL_DATA_DATE_ZONE_FORMAT = 108,
    QJS_INTL_DATA_DATE_NAME_OFFSETS = 109
};
/* 100+101+102 form an atomic group;103..109 optional, require the group.
 * Section109 additionally requires its exact owning105 period.
 * Group presence confers no service coverage or calendar capability.
 * UTF8 strings are shared pool1 references; digits are shared section17.
 */
#define QJS_INTL_DATE_PATTERN_WIDTH 32u
#define QJS_INTL_DATE_NAME_WIDTH 28u
#define QJS_INTL_DATE_SYMBOL_WIDTH 48u
#define QJS_INTL_DATE_PERIOD_RULE_WIDTH 16u
#define QJS_INTL_DATE_ZONE_NAME_WIDTH 72u
#define QJS_INTL_DATE_META_PERIOD_WIDTH 32u
#define QJS_INTL_DATE_ZONE_ALIAS_WIDTH 16u
#define QJS_INTL_DATE_RANGE_FALLBACK_WIDTH 20u
#define QJS_INTL_DATE_ZONE_FORMAT_WIDTH 36u
#define QJS_INTL_DATE_NAME_OFFSETS_WIDTH 32u
/* 108: locale u32; zone, metazone, location, name_pattern pool refs at4/12/20/28.
 * Sorted by(locale,zone,metazone). Metazone may be empty for fallback rows.
 * Pattern may be empty, otherwise exactly one literal {0}. Optional108
 * requires the100..102 group; old blobs without108 remain readable.
 */
/* Additive section101 semantics; record widths/IDs and wire1.3 unchanged.
 * field4: cyclic year1..60, format context0, width0..2.
 * field5: exact leap-month template, one literal {0}; named index1 with
 * context0/1,width0..2; numeric index0 with context0,width0 only.
 * Month index14 exists only for Hebrew month7@yeartype=leap.
 * Japanese era237/238 source exact Gregorian BCE/CE labels for the proposal's
 * pre-1873 Gregorian era rule. Other era indices retain the CLDR numeric key.
 * Presence never establishes arithmetic support or service coverage.
 */
#define QJS_INTL_DATE_HEBREW_ADAR_II 14u
#define QJS_INTL_DATE_JAPANESE_BCE 237u
#define QJS_INTL_DATE_JAPANESE_CE 238u
/* 109: zone pool ref0; UTC from_ms/before_ms i64 at8/16; signed absolute
 * CLDR standard/daylight name offsets i32 at24/28. Optional and locale-free;
 * must exactly match one105 period. It never changes the QJTZ schema. */
#endif
