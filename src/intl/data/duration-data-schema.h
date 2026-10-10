/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DURATION_DATA_SCHEMA_H
#define QJS_INTL_DURATION_DATA_SCHEMA_H
/* Optional additive schema1.3 extension. IDs111..119 stay reserved.
 * 110 width32: locale16_index:u32@0, numeric_numbering17_index:u32@4,
 * twoDigitHours:u8@8 (0/1), reserved zero bytes9..15,
 * hourMinuteSeparator:string_ref@16, minuteSecondSeparator:string_ref@24.
 * Keys(locale,numbering) strictly sorted unique. References are scalar UTF8
 * nonempty, no NUL; version CLDR49/UCD18. This revision uses exact inherited
 * CLDR units/durationUnit[@type=hms]/durationUnitPattern separators for each
 * admitted numeric numbering system (matching existing pinned frontend).
 * Reuse Number80/81 decimal symbols/patterns and83 unit plural patterns and
 * List40/41 unit styles. No duplicate unit/list/digit/plural/rounding tables.
 * Presence/validation of110 never enables service coverage or activation.
 * Central reader must explicitly register width32 and call independent gate.
 */
enum { QJS_INTL_DATA_DURATION_CLOCK = 110, QJS_INTL_DURATION_CLOCK_WIDTH = 32 };
#endif
