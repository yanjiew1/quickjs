/* SOURCE PROPOSAL ONLY: Intl binary wire schema v1.3.
 * XML/JSON are generator inputs/manifests, never runtime data.
 * This is a byte format, not a serialized C struct. Decode with byte readers;
 * never cast blob addresses to structs, pointers, uint16_t or uint32_t.
 */
#ifndef QJS_INTL_BINARY_DATA_H
#define QJS_INTL_BINARY_DATA_H
#include <stddef.h>
#include <stdint.h>
#include "data/plural-data-schema.h"
#include "data/relative-data-schema.h"
#include "data/number-data-schema.h"

/* Data versions are independent of the project's core Unicode18 algorithms. */
#define QJS_INTL_CLDR_RELEASE_48_2 ((48u << 16) | (2u << 8))
#define QJS_INTL_CLDR_PREVIEW_49 (49u << 16)
#define QJS_INTL_CLDR_VERSION_SUPPORTED(v) \
    ((v) == QJS_INTL_CLDR_RELEASE_48_2 || (v) == QJS_INTL_CLDR_PREVIEW_49)
#define QJS_INTL_COLLATION_VERSIONS_SUPPORTED(cldr, uca) \
    (((cldr) == QJS_INTL_CLDR_RELEASE_48_2 && (uca) == (17u << 16)) || \
     ((cldr) == QJS_INTL_CLDR_PREVIEW_49 && (uca) == (18u << 16)))

#define QJS_INTL_DATA_SCHEMA_MAJOR 1u
#define QJS_INTL_DATA_SCHEMA_MINOR 3u
#define QJS_INTL_DATA_HEADER_SIZE 64u
#define QJS_INTL_DATA_DIRECTORY_RECORD_SIZE 24u
#define QJS_INTL_DATA_INDEX_NONE UINT32_MAX
#define QJS_INTL_DATA_MAGIC "QJSINTL\0" /* exactly 8 bytes */

/* All integers are little-endian. Header byte offsets. */
enum {
    QJS_INTL_H_MAGIC = 0,             /* 8 bytes */
    QJS_INTL_H_SCHEMA_MAJOR = 8,      /* u16 */
    QJS_INTL_H_SCHEMA_MINOR = 10,     /* u16 */
    QJS_INTL_H_HEADER_SIZE = 12,      /* u32, exactly64 for v1 */
    QJS_INTL_H_TOTAL_SIZE = 16,       /* u32, exact blob size */
    QJS_INTL_H_SECTION_COUNT = 20,    /* u32 */
    QJS_INTL_H_DIRECTORY_OFFSET = 24, /* u32, aligned to4 */
    QJS_INTL_H_DIRECTORY_RECORD_SIZE = 28, /* u32, exactly24 */
    QJS_INTL_H_FLAGS = 32,            /* u32, zero in v1 */
    QJS_INTL_H_UNICODE_VERSION = 36,  /* u32: major<<16|minor<<8|patch */
    QJS_INTL_H_CLDR_VERSION = 40,     /* same encoding */
    QJS_INTL_H_UCA_VERSION = 44,      /* same encoding */
    QJS_INTL_H_RESERVED = 48          /* 16 zero bytes */
};
/* Directory record byte offsets, sorted by unique section ID. */
enum {
    QJS_INTL_D_ID = 0,
    QJS_INTL_D_OFFSET = 4,
    QJS_INTL_D_BYTE_LENGTH = 8,
    QJS_INTL_D_RECORD_COUNT = 12,
    QJS_INTL_D_RECORD_SIZE = 16,
    QJS_INTL_D_FLAGS = 20             /* zero in v1 */
};
/* Sections cannot overlap the header, directory or each other. Padding is
 * zero. Offset+length and count*record_size are checked using subtraction/
 * division before arithmetic. Every section's byte length equals its count
 * times record size. String pool has record_size1/count=byte length.
 */

typedef enum QJSIntlDataSectionId {
    QJS_INTL_DATA_UTF8_POOL = 1,       /* record_size1 */
    QJS_INTL_DATA_INPUT_SHA256 = 2,    /* count1, record_size32, raw digest */
    QJS_INTL_DATA_ALIAS = 10,          /* record_size20 */
    QJS_INTL_DATA_REPLACEMENT_LIST = 11, /* record_size8 string refs */
    QJS_INTL_DATA_LIKELY = 12,         /* record_size16 */
    QJS_INTL_DATA_BCP47 = 13,          /* record_size48 */
    QJS_INTL_DATA_WEEK = 14,           /* record_size12 */
    QJS_INTL_DATA_PREFERENCE = 15,     /* record_size36 */
    QJS_INTL_DATA_LOCALE = 16,         /* record_size48 */
    QJS_INTL_DATA_NUMBERING = 17,      /* record_size52 */
    QJS_INTL_DATA_LIST = 18,           /* record_size8 string refs */
    QJS_INTL_DATA_COMPONENT_PARENT = 19, /* record_size20 */
    QJS_INTL_DATA_REGION_TIME_ZONE = 20, /* record_size16 */
    QJS_INTL_DATA_LOCALE_AUX = 21,       /* record_size44, minor>=1 */
    QJS_INTL_DATA_NUMBERING_RULE = 22,   /* record_size12, minor>=1 */
    QJS_INTL_DATA_HOUR_DEFAULT = 23,     /* record_size20, minor>=1 */
    QJS_INTL_DATA_SCRIPT_DIRECTION = 24, /* record_size12, minor>=3 */
    QJS_INTL_DATA_AVAILABLE_CALENDAR = 25, /* record_size8, minor>=3 */
    QJS_INTL_DATA_LOCALE_SERVICE_INFO = 26, /* record_size28, minor>=3 */
    QJS_INTL_DATA_LIST_PATTERN = 40,     /* record_size40, minor>=2 */
    QJS_INTL_DATA_LIST_HEBREW_SCRIPT = 41, /* record_size8, minor>=2 */
    QJS_INTL_DATA_DISPLAY_LABEL = 50,    /* record_size28, minor>=3 */
    QJS_INTL_DATA_DISPLAY_PATTERN = 51,  /* record_size16, minor>=3 */
    QJS_INTL_DATA_SEGMENT_GCB = 60,      /* record_size12, minor>=3 */
    QJS_INTL_DATA_SEGMENT_WB = 61,       /* record_size12, minor>=3 */
    QJS_INTL_DATA_SEGMENT_SB = 62,       /* record_size12, minor>=3 */
    QJS_INTL_DATA_SEGMENT_INCB = 63,     /* record_size12, minor>=3 */
    QJS_INTL_DATA_SEGMENT_EP = 64,       /* record_size12, minor>=3 */
    QJS_INTL_DATA_SEGMENT_RULE = 65,     /* record_size24, minor>=3 */
    QJS_INTL_DATA_COLLATION_NODE = 90,   /* record_size20, minor3 */
    QJS_INTL_DATA_COLLATION_CE = 91,     /* record_size16, minor3 */
    QJS_INTL_DATA_COLLATION_IMPLICIT = 92, /* record_size16, minor3 */
    QJS_INTL_DATA_COLLATION_DIGIT = 93,  /* record_size4, minor3 */
    QJS_INTL_DATA_COLLATION_CAPABILITY = 94, /* record_size20, minor3 */
    QJS_INTL_DATA_COLLATION_CONFIG = 95  /* record_size16, minor3 */
    /* Later service schema additions allocate new IDs and a reviewed minor
     * revision; patterns/plurals/break/collation are separate packed sections.
     */
} QJSIntlDataSectionId;

/* Optional collation sections90..95 form one atomic schema1.3 group;
 * absence is valid, partial presence invalid. Presence requires actual
 * header Unicode18 and a supported CLDR/UCA pair. Runtime global service coverage stays0.
 * NODE(20): cp:u32@0, children:span@4, ces:span@12. Node0 cpFFFFFFFF/noCE;
 * BFS order with contiguous scalar-sorted children, every non-root node
 * owned once; empty spans0/0; nonempty CE spans reference91; leaf has CE.
 * CE(16): p:u32@0,s:u32@4,t:u32@8,flags:u32@12. Weights fit16bits;
 * flags bit0 variable (p nonzero), bit1 upper per the pinned CLDR Case_Untailored
 * t in{08..0C,0E,11,12,1D}; all other bits0. Complete CE spans interned.
 * IMPLICIT(16): first:u32@0,last:u32@4,base:u32@8,origin:u32@12.
 * Ascending disjoint scalar ranges; base bit31 selects Han formula:
 * lead=(base&7fffffff)+(cp>>15),trail=(cp&7fff)|8000,origin0;
 * Han baseFB40/FB80. Without bit31, lead=baseFB00..FB05 and
 * trail=(cp-origin)|8000; origin<=first,last-origin<=7FFF.
 * Synthesized pair [.lead.0020.0002][.trail.0000.0000]. Other codepoints,
 * including individual unpaired UTF16 units, use FBC0+(cp>>15) / low15bits.
 * DIGIT(4): zeroCp:u32@0; following9 scalar Nd characters give1..9.
 * Sets sorted/nonoverlapping and generated from sealed complete UCD18 Nd.
 * CAPABILITY(20): localeIndex:u32@0,usage:u32@4,flags:u32@8,
 * sensitivity:u32@12,ignorePunctuation:u32@16. Adjacent usage0 sort/usage1
 * search pairs ordered by localeIndex section16 row; both usages use the
 * explicit implementation-defined shared root comparator. flags31
 * (sensitivity,numeric,caseFirst,shifted,normalization); sensitivity3
 * variant; ignore0; only root/en/en-US. co=[null] in both LocaleData records.
 * CONFIG(16): revision2:u32@0,numericPrimary:u32@4,maxDepth:u32@8,
 * UCApacked:u32@12; exactly1 row, depth1..64 matching actual trie.
 * Ordinary primary<<16 leaves a numeric prefix gap before digit zero.
 */
/* A string reference is two u32 values: pool byte offset, byte length.
 * A list span is two u32 values: first record index, record count.
 * All below layouts specify byte offsets, not native C struct members.
 *
 * ALIAS(20): kind:u32@0, from:string@4, replacements:span@12.
 *   kinds0 language,1 script,2 region,3 variant,4 subdivision.
 *   Preserve multi-region replacement ordering and alias component fields.
 * LIKELY(16): from:string@0, to:string@8.
 * BCP47(48): extension:string@0 ('u' or 't'), key:string@8,
 *   name:string@16, alias:string@24, preferred:string@32,
 *   value_type:u32@40, deprecated:u32@44.
 *   Alias stores the full ordered ASCII-space token list, including case and
 *   punctuation such as underscores/slashes. Name may be empty for a key
 *   alias record. Preserve transformed-extension keys rather than erasing them.
 *   value_type0 single,1 multiple,2 incremental,3 any.
 * WEEK(12): region:string@0, ISO first_day:u8@8,
 *   weekend_mask:u8@9 (bit0 Monday...bit6 Sunday), minimal_days:u8@10,
 *   reserved_zero:u8@11.
 * PREFERENCE(36): key:string@0, scope:u32@8 (0 region,1 language-region),
 *   calendars:LIST span@12, hour_cycles:LIST span@20,
 *   raw_hour_symbols:LIST span@28 (preserve b/B context variants).
 * LOCALE(48): tag:string@0, parent_index:u32@8,
 *   language:string@12, script:string@20, region:string@28,
 *   default_numbering:string@36, service_coverage:u32@44.
 * NUMBERING(52): id:string@0, radix:u8@8, algorithmic:u8@9,
 *   reserved_zero:u16@10, ten scalar digits:u32[10]@12.
 * COMPONENT_PARENT(20): component:string@0, child_index:u32@8,
 *   parent_index:u32@12, flags:u32@16.
 * REGION_TIME_ZONE(16): region:string@0, primary_ids:LIST span@8.
 *   Generated from timezone owner's final primary/country metadata; do not
 *   invent aliases or select ICU zone enumeration in native mode.
 *
 * REQUIRED sections for base metadata:1,2,10,11,12,13. Other listed sections
 * are optional in this initial metadata conversion; consumers requiring them
 * must report missing data, and partial metadata cannot activate full Intl.
 * Readers accept major1/minor0,1,2,3 only; reject minor>3. Minor0 rejects
 * section21..26,40,41. Minor1 rejects24..26,40,41; Minor2 permits40,41
 * and rejects24..26. Minor3 permits optional LocaleInfo, DisplayNames and Segmenter additions.
 * Minors0..2 reject30..32,50,51,60..65,70/71,80..88.
 * IDs33..39,72..79,89 and all unreviewed future service IDs stay unknown.
 * Minor1 permits21..23; each consumer
 * requiring them must return missing data when absent. Existing1..20 widths
 * are unchanged. Unknown section IDs/flags are rejected. Directory offset is
 * exactly64. All payload offsets are 4-byte aligned and at/after directory
 * end; empty sections have count0/length0 at an aligned in-bounds payload
 * position and may share that position. Record sizes still match their ID.
 * UTF8_POOL has >=1 byte, begins/ends with NUL, and contains NUL-
 * terminated strings. Empty string is exactly offset0/length0. Nonempty refs
 * start at offset>=1 after NUL and end immediately before NUL; their bounds
 * and UTF8 are checked. No embedded NUL inside a referenced string.
 * Empty span is first0/count0; nonempty spans are checked against target table
 * count. Alias replacement spans must be nonempty. INDEX_NONE is allowed only
 * for LOCALE/COMPONENT_PARENT parent references. All other indices are real.
 * Required ASCII identifier fields are nonempty except explicitly optional
 * BCP47 name/alias/preferred and LOCALE script/region/default_numbering.
 * Strings such as BCP47 aliases retain source case/punctuation; do not apply
 * a lowercase-only or hyphen-only validator to every string field.
 * INPUT_SHA256 is a raw32-byte SHA256 over the deterministic consumed-input
 * manifest bytes (UTF8 JSON, sorted keys, compact separators, final LF,
 * relative source names, file hashes and version pins; no host paths/time).
 * This manifest is generator evidence only; runtime uses binary bytes.
 * Generated tables are stable ASCII-key sorted. Counts/indices are bounded.
 * Sort keys:ALIAS(kind,from); LIKELY(from); BCP47(extension,key,name,alias);
 * WEEK(region); PREFERENCE(scope,key); LOCALE(tag); NUMBERING(id);
 * COMPONENT_PARENT(component,child_index); REGION_TIME_ZONE(region).
 * Keys are unique; conflicting duplicates fail. List/replacement order is
 * semantic and is preserved, not sorted. Header flags/reserved and directory
 * flags/reserved record bytes are zero. WEEK ranges1..7, minimalDays1..7;
 * deprecated/algorithmic are0/1; scope0/1; alias kinds0..4; value_type0..3.
 * Generator rejects semantic alias graph cycles. Reader checks direct ALIAS
 * self replacement and locale/component-parent cycles; runtime canonicalize
 * bounds substitution passes by alias_count+1, then reports a data error.
 * Generic reader requires nonzero Unicode/CLDR major; UCA0 is allowed before
 * collation sections. Metadata-only generation records core Unicode18/selected CLDR/0; collation
 * records the actual pinned CLDR-bundled UCA version (release48.2/UCA17). Provider checks Unicode
 * against libunicode18 and collation compatibility before activation.
 * Invalid scalars/enums/UTF8 and inconsistent duplicate records are rejected.
 * Header/data versions and input digest are actual binary bytes.
 */

/* Decoder view/section structs and external reader API belong to the
 * binary reader owner's private header. Endian/bounds helper implementations
 * are static in ONE reader .c file, not duplicated in each including TU.
 * Optional embedding declarations belong to generated locale-metadata.h:
 * const unsigned char qjs_intl_locale_metadata_blob[];
 * const size_t qjs_intl_locale_metadata_blob_size;
 * The generator defines both exactly once in locale-metadata.c.
 * COMPONENT_PARENT flags are reserved and exactly0 in schema1.0/1.1.
 *
 * Additive schema1.1 records (all fields little-endian):
 * LOCALE_AUX(44): locale_index:u32@0, flags:u32@4, direction:u32@8,
 *   native_numbering:string@12, traditional_numbering:string@20,
 *   finance_numbering:string@28, default_collation:string@36.
 *   Sorted unique locale_index into16. Flagsbit0 marks defaultContent;
 *   all other bits zero. Direction0 inherits,1 LTR,2 RTL. Optional empty
 *   numbering/collation fields inherit; nonempty numbering fields name17.
 *   LOCALE/default numbering and AUX are sparse local fields; inheritance
 *   uses general parents, except collation uses collations component parents.
 *   Missing native falls back to default numbering; missing traditional/
 *   finance falls back to native after inheritance. Direction policy is
 *   explicit CLDR characterOrder, never Unicode script-name RTL inference.
 *   Collation aliases are canonicalized by generator through M03, retaining
 *   reserved standard/search metadata; service engines filter those values.
 * NUMBERING_RULE(12): numbering_index:u32@0, rules:string@4.
 *   Sorted unique numbering_index into17, algorithmic only. Nonempty rules
 *   are printable ASCII metadata, without formatter capability claims.
 *   Numeric17 radix10/ten distinct scalar digits; algorithmic17 radix10/
 *   ten zero placeholders. Rule metadata cannot enable a number service.
 * HOUR_DEFAULT(20): key:string@0, scope:u32@8, preferred_raw:string@12.
 *   Sorted(scope,key); scope0 region/1 language-region; key/scope must match
 *   a15 record. Preferred is an exact member of its raw allowed symbols.
 *   Raw symbols: H/h/K/k optionally followed by b/B. In minor1,15 cycles
 *   equal ordered unique raw mappings H/h/K/k=>h23/h12/h11/h24.15 preserves
 *   allowed order;23 separately preserves preferred symbol/dayperiod variant.
 *   Empty15 lists inherit. Preferences resolve language-region, region,001;
 *   locale rg/sd/fw handling belongs to locale algorithms, not generation.
 * General parent exceptions do not automatically apply to declared component
 * groups. Component groups, including empty groups, use explicit component
 * parents then structural fallback.19 stores sparse differences from16.
 * DefaultContent marks source routing only; coverage remains0, including
 * identity-only locales. No XML/JSON is required by runtime consumers.
 *
 * Additive schema1.2 records (all fields little-endian):
 * LIST_PATTERN(40): locale_index:u32@0, type:u8@4, style:u8@5,
 *   context:u8@6, reserved_zero:u8@7, pair:string@8, start:string@16,
 *   middle:string@24, end:string@32. References section16; sorted unique
 *   (locale_index,type,style,context). Types0 conjunction/1 disjunction/
 *   2 unit; styles0 long/1 short/2 narrow. Context0 is the base row.
 *   Optional context1 Spanish y/e,2 Spanish o/u,3 Hebrew vav/vav-dash
 *   is a complete alternate quartet, not an availability/coverage flag.
 *   Exactly one base and at most one alternate per locale/type/style.
 *   Four nonempty UTF8 scalar patterns each contain {0} and {1} once and
 *   no other braces; preserve literal apostrophes/NBSP/bidi, reject NUL.
 *   Alternate start/middle equal base; pair/end replace only exact
 *   {0} y {1}=>{0} e {1}, {0} o {1}=>{0} u {1}, or
 *   {0} U+05D5{1}=>{0} U+05D5-{1}, per context. Nonmatching pair/end
 *   are identical. At least one pair/end changes. Context1/2 requires
 *   section16.language es; context3 he or iw. A base triggering a known
 *   context in that language requires its corresponding alternate.
 *   General16 inheritance and requesting-locale aliases are resolved by
 *   generation, including per-field inheritance markers. No runtime XML.
 * LIST_HEBREW_SCRIPT(8): first_scalar:u32@0,last_scalar:u32@4 inclusive.
 *   Sorted unique ascending, first<=last<=0x10FFFF, no overlapping ranges
 *   and no surrogate intersections. Adjacent ranges coalesce during
 *   generation. Nonempty section required when any context3 row exists.
 *   Generated from verified Unicode18 Scripts.txt Script=Hebrew; header
 *   Unicode version must be18.0.0 when40/41 is present. CLDR must be a supported selected version
 *   when40 is present. Reader checks structure; digest/provider activation
 *   proves exact pinned source coverage rather than trusting generic ranges.
 *   Context predicates are pinned ICU78.3 reference behavior, evaluated on
 *   the final input element for pair/end; empty input selects base. Hebrew
 *   uses first Unicode scalar (unpaired surrogate has non-Hebrew script).
 *   New service sections alone never activate metadata service_coverage.
 */
/* Additive1.3 LocaleInfo records, reviewed ECMA4027ae78cf on2026-10-09:
 * SCRIPT_DIRECTION(12): script:string@0, direction:u32@8.
 *   Sorted unique canonical ISO15924 titlecase script. Direction0 unknown,
 *   1 LTR,2 RTL, generated from verified selected CLDR scriptMetadata.txt field6
 *   UNKNOWN/NO/YES respectively. No inferred horizontal locale orientation.
 *   Missing script in this complete source inventory means undefined.
 * AVAILABLE_CALENDAR(8): id:string@0; sorted unique canonical ca type.
 *   Actual installed implementation inventory, not all CLDR ca names.
 *   Nonempty and includes gregory. Optional until a calendar owner supplies
 *   actual capability evidence. Metadata-only bundle emits no25.
 * LOCALE_SERVICE_INFO(28): locale_index:u32@0, service:u32@4,
 *   default_value:string@8, values:LIST span@16, flags:u32@24=0.
 *   Sorted unique(service,locale_index). Service0 Collator: default_value
 *   empty represents omitted leading null co element; values are unique
 *   canonical co strings excluding standard/search, in any semantic order.
 *   Service1 NumberFormat: default_value is actual supported default nu,
 *   names17; values span exactly0/0, since Locale returns only that default.
 *   Entries constitute the exact AvailableLocales for those installed
 *   services. Each row requires corresponding16 service_coverage bit1 or3.
 *   Metadata-only generation never emits26 or sets those coverage bits.
 *   No root identity row is permitted; no defaultContent assumption.
 *   Missing25/26 is UNSUPPORTED, not a proved empty inventory.
 * All new IDs require minor>=3; minor0..2 keep previous widths/behavior.
 * Presence24 requires Unicode18.0.0 and supported CLDR header versions. Optional section
 * completeness and source provenance are verified by provider activation.
 */
enum { QJS_INTL_LOCALE_INFO_COLLATOR = 0, QJS_INTL_LOCALE_INFO_NUMBER = 1 };
/* Schema1.3 Plural30..32: exact layouts are declared once in
 * data/plural-data-schema.h. The triple is jointly optional and requires
 * LOCALE when present.30 locale/type keys, referenced31 category keys and32
 * range-pair keys are strictly unique/ordered; rules/ranges are shared spans.
 * All categories/types/reserved bytes and span/pool boundaries are checked.
 * Other rules are empty; named rules use the adopted exact scalar decimal
 * relation grammar. Supported CLDR version required. No service coverage inferred. Generic
 * reader invokes plural-data-validation.c, linked with adopted pure plural.c.
 */

/* Schema1.3 DisplayNames: optional sections; absence is reported by the
 * service consumer, never replaced by fabricated labels or patterns.
 * DISPLAY_LABEL(28): locale_index:u32@0, type:u32@4 [0..6],
 *   style:u32@8 [0..2], code:string@12, name:string@20. Code is nonempty
 *   ASCII bytes1..127; name is nonempty scalar UTF8. Strict unique ordering
 *   (locale_index,type,style,code UTF8 bytes). Type6 is private variant data.
 * DISPLAY_PATTERN(16): locale_index:u32@0, kind:u32@4 [0..6], text:string@8.
 *   Strict unique ordering (locale_index,kind). Kinds0..2 contain exactly
 *   one {0} and one {1}, no other braces; kinds3..6 are nonempty scalar text.
 * Both tables reference LOCALE indices. Presence proves shape, never locale
 * completeness or availability. Service coverage is not modified by reader.
 *
 * Schema1.3 Segmenter:60..64 have inclusive first:u32@0,last:u32@4,value:u32@8.
 * Tables are nonempty, scalar-only, sorted/disjoint; adjacent same-value
 * ranges coalesce. Nondefault values:60=1..13,61=1..18,62=1..14,63=1..3,64=1.
 * SEGMENT_RULE(24) has exactly3 ordered rows granularity:u32@0=0,1,2,
 *   unicode_version:u32@4=18<<16, uax29_revision:u32@8=49,
 *   algorithm_version:u32@12=1, ep_source:u32@16=1 or2,
 *   reserved_zero:u32@20=0. Every row uses identical ep_source.
 * Any60..65 requires60,61,62,63,65; ep_source1 requires64, ep_source2
 * forbids64. Header Unicode18.0.0 and a supported CLDR version required for the group.
 * Exact UCD completeness and external EP backend validation remain provider
 * responsibilities; no locale rules, dictionaries or availability inferred.
 * There is no fixed section-count cap: directory bounds plus recognized
 * unique IDs bound the count (43 recognized IDs in this additive schema).
 */
/* Schema1.3 RelativeTimeFormat70/71 and NumberFormat80..88: exact owner
 * byte layouts are declared in relative-data-schema.h and number-data-schema.h.
 * Generic pool/ref checks precede unchanged owner group/grammar gates. Relative
 * requires both70/71 and valid complete30..32 cardinal bindings for all16 rows.
 * Number80/81 are atomic,82/88 require87, and81/84/86/88 bind matching80 keys.
 * All present groups pin core Unicode18 and the selected supported CLDR. Coverage/provider activation remains
 * external; accepting these sections never adds availability or capabilities.
 * No reviewed layout exists here for reserved72..79/89 or future90..110 IDs.
 */
#endif /* QJS_INTL_BINARY_DATA_H */
