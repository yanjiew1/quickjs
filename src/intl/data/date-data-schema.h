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
    QJS_INTL_DATA_DATE_RANGE_FALLBACK = 107
};
/* 108..109 are reserved, unused. Exact layouts in WIRE-SCHEMA.json.
 * 100+101+102 form an atomic group;103..107 optional, require the group.
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
#endif
