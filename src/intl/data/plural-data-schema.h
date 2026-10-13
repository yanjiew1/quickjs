/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Additive wire1.3 proposal; does not replace metadata-data.h. */
#ifndef QJS_INTL_PLURAL_DATA_SCHEMA_H
#define QJS_INTL_PLURAL_DATA_SCHEMA_H
#define QJS_INTL_DATA_PLURAL_LOCALE 30u
#define QJS_INTL_DATA_PLURAL_RULE 31u
#define QJS_INTL_DATA_PLURAL_RANGE 32u
#define QJS_INTL_DATA_PLURAL_LOCALE_WIDTH 24u
#define QJS_INTL_DATA_PLURAL_RULE_WIDTH 12u
#define QJS_INTL_DATA_PLURAL_RANGE_WIDTH 4u
/* 30: locale_index:u32@0, type:u8@4, reserved_zero:u8[3]@5,
 *     rules:section31 span@8, ranges:section32 span@16.
 *     Sorted unique (locale_index,type). Locale references section16.
 * 31: category:u8@0, reserved_zero:u8[3]@1, relation:string@4.
 *     Within each referenced span, category sorted unique, other last.
 *     Other relation empty; all other relation strings nonempty exact ASCII.
 * 32: start:u8@0, end:u8@1, result:u8@2, reserved_zero:u8@3.
 *     Within each referenced span, (start,end) sorted unique.
 * Type0 cardinal,1 ordinal. Category0 zero,1 one,2 two,3 few,4 many,5 other.
 * Rule span count1..6; range span count0..36. Empty span exactly first0/count0.
 * Tables31/32 are shared spans, not globally category/key sorted tables.
 * Absent range pairs default to other according to LDML plural ranges.
 * Section30/31/32 jointly optional: absent all or present all, no service bits.
 * Minor0..2 reject all30..39. Minor3 permits reviewed30/31/32 only.
 * 33..39 reserved,24..26 LocaleInfo,40/41 List,50/51 DisplayNames,
 * 60..65 Segmenter allocated by their owners. This packet claims none of them.
 */
#endif
