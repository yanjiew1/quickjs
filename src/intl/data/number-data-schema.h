/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_DATA_SCHEMA_H
#define QJS_INTL_NUMBER_DATA_SCHEMA_H
/* Additive optional schema1.3. These are byte layouts, never C structs. */
enum {
    QJS_INTL_DATA_NUMBER_SYMBOL = 80,
    QJS_INTL_DATA_NUMBER_PATTERN = 81,
    QJS_INTL_DATA_NUMBER_CURRENCY = 82,
    QJS_INTL_DATA_NUMBER_UNIT = 83,
    QJS_INTL_DATA_NUMBER_CURRENCY_NAME = 84,
    QJS_INTL_DATA_NUMBER_CURRENCY_DIGITS = 85,
    QJS_INTL_DATA_NUMBER_MISC = 86,
    QJS_INTL_DATA_NUMBER_UNICODE_CLASS = 87,
    QJS_INTL_DATA_NUMBER_CURRENCY_SPACING = 88
};
/*80(96): locale_index:u32@0, numbering17_index:u32@4,
 * minimumGroupingDigits:u8@8, zero bytes9..15,
 * string refs@16: decimal,group,plusSign,minusSign,percentSign,exponential,
 * infinity,nan,currencyDecimal,currencyGroup (10 refs).
 *81(40): locale:u32@0,numbering:u32@4,style:u8@8
 * (0decimal,1percent,2currency,3accounting),variant:u8@9(0base,1alpha),
 * zero:u16@10,primaryGroup:u8@12,secondaryGroup:u8@13,zero:u16@14,
 * zeroPattern:string@16,negativePattern:string@24,positivePattern:string@32.
 *82(76): locale:u32@0,code:string@4,symbol:string@12,narrow:string@20,
 * six plural category name refs@28 in zero,one,two,few,many,other order.
 *83(64): locale:u32@0,unit:string@4,display:u8@12(0long,1short,2narrow),
 * zero bytes13..15,six unit template refs@16 in plural category order.
 *84(56): locale:u32@0,numbering:u32@4,six currency name templates@8.
 *85(12): code:string@0,digits:u8@8(0..100),zero bytes9..11.
 * DEFAULT row is required; uppercase 3-letter codes otherwise. DEFAULT
 * supplies ECMA402 CurrencyDigits fallback; cash rounding is not NF default.
 *86(24): locale:u32@0,numbering:u32@4,approximately:string@8,range:string@16.
 * Raw LDML placeholders {0} / {0},{1}; exact approximation/range rendering.
 *87(12): first:u32@0,last:u32@4,flags:u32@8(bits0L,1!S&&!Z,2Nd).
 * Sorted disjoint scalar ranges; missing scalars have flags2 (Cn default).
 * Adjacent equal flags coalesce; flags2 rows omitted. No surrogates.
 *88(24): locale:u32@0,numbering:u32@4,beforeCurrencyInsert:string@8,
 * afterCurrencyInsert:string@16. Only exact CLDR currencyMatch
 * [[:^S:]&[:^Z:]] / surroundingMatch[:digit:] is supported by this revision.
 * Other rules produce explicit generator gaps and no88 row.
 *
 *80/81 are atomic optional pair; all others optional.82 or88 requires87.
 * Keys80,84,86,88(locale,numbering);81(locale,numbering,style,variant);
 *82(locale,code);83(locale,unit,display);85(code);87(first,last).
 * All keys sorted unique. Locale indices reference16, numeric numbering17
 * indices only.81 requires matching80, base required for alpha; alpha only
 * currency/accounting.84,86,88 require matching80. Strings are exact UTF8
 * scalar slices, no NUL. Symbols nonempty. Currency name/unit plural slots
 * are complete through count=other fallback; no invented category text.
 * Numeric templates use {number} exactly once; unit templates permit its
 * omission for authentic singular/dual forms. Currency slots {currency} exactly
 * once, percent {percentSign} exactly once; sign templates preserve placement
 * from LDML. Currency/unit identifiers validated by generator/frontend.
 * Prefix/suffix sign/bidi/space literals retained.81 groups0..9, primary0
 * iff secondary0, minGrouping1..9.80/87 require Unicode18 and CLDR49.
 * No service_coverage bit is enabled by any record or generator.
 */
#endif
