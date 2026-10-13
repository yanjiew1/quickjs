/* Additive wire1.3 proposal. Integrating owner updates central reader/schema.
 * Strings are refs in the one shared canonical UTF8 pool. */
#ifndef QJS_INTL_DISPLAY_NAMES_NATIVE_DATA_H
#define QJS_INTL_DISPLAY_NAMES_NATIVE_DATA_H
#define QJS_INTL_DATA_DISPLAY_LABEL 50u
#define QJS_INTL_DATA_DISPLAY_PATTERN 51u
#define QJS_INTL_DISPLAY_LABEL_WIDTH 28u
#define QJS_INTL_DISPLAY_PATTERN_WIDTH 16u
/* 50: locale_index u32, type u32 [0..6], style u32 [0..2], code ref8,
 * name ref8. Type6 is private language variant labels. Strict sort by
 * (locale_index,type,style,code UTF8 bytes), no duplicate keys. */
/* 51: locale_index u32, pattern_kind u32 [0..6], text ref8. Strict sort by
 * (locale_index,pattern_kind). Kind0 localePattern,1 localeSeparator,
 * 2 localeKeyTypePattern,3..6 nested bracket replacements for (,),（,）.
 * Kinds0..2 require exactly one {0} and one {1}; only these placeholders.
 * Kinds3..6 preserve CLDR replacement scalars and are nonempty.
 * Rows flatten verified CLDR inheritance. No coverage bits are set. */
#endif
