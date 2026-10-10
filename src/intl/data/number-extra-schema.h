/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_EXTRA_SCHEMA_H
#define QJS_INTL_NUMBER_EXTRA_SCHEMA_H
#include "number-data-schema.h"
enum {
    QJS_INTL_DATA_NUMBER_COMPACT = 120,
    QJS_INTL_DATA_NUMBER_DENOMINATOR = 121,
    QJS_INTL_DATA_NUMBER_COMPOUND_PER = 122
};
/* Additional optional schema1.3, CLDR49/Unicode18; no coverage activation.
 *120(76): locale:u32@0,numbering17index:u32@4,compactDisplay:u8@8
 * (0short,1long),zero9..11,magnitude:u32@12,exponent:u32@16,
 * six notation template refs@20 in plural category order,exactCount1@68.
 * Keys(locale,numbering,display,magnitude); matching80 required. Magnitude
 *0..1e9;0<=exponent<=magnitude. Six nonempty templates may contain {number}
 * zero/one times; no other tokens/braces. Nonzero exponent requires affix
 * text; exponent0 templates are exactly {number}. ExactCount1 is optional.
 *121(32): locale:u32@0,sanctionedSimpleUnit:string@4,display:u8@12
 * (0long,1short,2narrow),zero13..15,perUnitTemplate:string@16(optional),
 * denominatorName:string@24(nonempty literal, no braces). Per-unit template
 * requires {number} exactly once and no other braces. Name is selected by
 * pinned per-component grammar and source one-form without number field.
 * Keys(locale,unit,display); requires matching83(locale,unit,display).
 *122(16): locale:u32@0,display:u8@4(0long,1short,2narrow),zero5..7,
 * compoundPerTemplate:string@8. Contains {numerator} and {denominator}
 * exactly once each in source order. Keys(locale,display).
 *121/122 atomic optional pair; every121 row requires matching122.121 strings
 * and122 templates are scalar UTF8. This host/runtime composition stores
 * O(simple units) rows, avoiding a cartesian product of compound-unit rows.
 *83 existing layout is unchanged; its six unit templates now permit an
 * omitted number field for authentic CLDR singular/dual unit forms.
 *123..129 remain reserved/unknown.89 remains reserved/unknown.
 */
#endif
