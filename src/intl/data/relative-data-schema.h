/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Additive schema1.3 proposal; existing IDs/widths/reader API unchanged.
 * Root schema owner must add these IDs/widths to its generic validator. */
#ifndef QJS_INTL_RELATIVE_DATA_SCHEMA_H
#define QJS_INTL_RELATIVE_DATA_SCHEMA_H
#define QJS_INTL_DATA_RELATIVE_PATTERN 70u
#define QJS_INTL_DATA_RELATIVE_LITERAL 71u
#define QJS_INTL_DATA_RELATIVE_PATTERN_WIDTH 16u
#define QJS_INTL_DATA_RELATIVE_LITERAL_WIDTH 20u
/* Patterns admit zero or one {0}; implicit-number patterns remain literal.
 * Record70: locale:u32@0, unit/style/tense/category:u8@4, UTF8ref@8.
 * Record71: locale:u32@0, unit/style:u8@4, reserved0:u16@6,
 * signedoffset:i32@8, UTF8ref@12. All integers are little endian.
 * 70 is complete sorted locale*8*3*2*6 lattice; 71 sorted unique
 * (locale,unit,style,signedoffset). Text refs scalar UTF8, nonempty. */
#endif
