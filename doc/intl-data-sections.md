# QJIN section reference

See [the envelope and maintenance guide](intl-data-format.md) first. This
reference describes the current encoder/reader contracts, not a public file ABI
or a declaration of service completeness.

## Notation

- Unless explicitly stated otherwise, every field is an unsigned little-endian
  32-bit word, arrays are tightly packed and dimensions count records.
- `L` is section 2's selected locale count. In sections 25 through 32, `N` has
  that same meaning. Sections 33 through 35 define their own `N`.
- `sid` indexes section 1; string 0 is empty. A numbering-system definition
  index indexes section 13, not a numeric-only projection of that table.
- `(start, count)` describes records in a named payload table, not byte extents.
- Ordinary indexes are zero-based; layered-map heads/parents are one-based,
  with 0 terminating the chain. Every referenced table has its own index space.
  Sparse text-map descriptors are `(first_entry, entry_count)`, partition the
  flat entry table contiguously and describe nonempty maps, except section 21
  explicitly permits empty base maps. Section 34 has separate rule-map rules.
- `ABSENT = 0xffffffff`, `BLOCKED = 0xfffffffe`, `EMPTY = 0xfffffffd` apply only
  where the individual schema permits them. Sparse text maps instead encode
  empty as string ID 0 and absence as no key; their only text sentinel is BLOCKED.
- Deduplicated word vectors/maps are sorted by numeric word sequences, not by
  `memcmp` on their little-endian representation. Byte vectors have byte order.
  Canonical tables require used/reachable records and exact payload coverage.
- Sparse nodes are `(origin_locale_sid, source_group_index, map_index,
  parent_head)`. Parents precede children. For sections 26/29/30/31/32,
  canonical ordering is by depth and then tuple; section 21 uses its separately
  specified breadth-first parent-ID ordering. The first matching leaf masks
  inherited leaves even when blocked or empty. Aliases can change source group/calendar within a compatible family.
- Dense source-state records use `0` missing, `1` value, `2` blocked. Missing is
  an all-zero record. Blocked retains provenance; its text and, where present,
  numbers_sid are 0. A value may have empty text. These states are distinct
  from sparse map heads and text sentinels.
- Sections 1 through 16 have no section-version word. Later sections explicitly
  store the versions given below, independently of envelope version `(1, 0)`.

The source paths below identify the Python codec and C owner to inspect when
changing a layout. Source enumerations are part of the wire contract.

## 1 STRINGS

Directory count S. Payload `offsets[S+1]:u32`, then concatenated UTF-8 bytes, no per-string
terminator. Offsets relative to the beginning of that trailing byte pool. offsets[0]=offsets[1]=0
(empty string 0), monotonic thereafter; last offset exact byte-pool length. Source
`tools/intl/tables.py`; `src/intl/native/data.c`.

## 2 LOCALES

Directory count L. No local header. `L * (tag_sid, parent_locale_index)`, 2 words/row. Parent
ROOT=`0xffffffff` iff tag `root`; every other parent <L; graph acyclic, parent need not precede
child numerically. Source `tools/intl/tables.py`; `src/intl/native/data.c`.

## 3 ALIASES / 4 LIKELY / 5 KEYS / 6 TYPES

No local header, directory count is row count, flat fixed-width rows. All 3..6 present together or
absent together.

- 3 (4 words): `(alias_kind, source_sid, joined_replacements_sid, replacement_count)`, sorted
  `(kind,source_sid)`. Kinds 0 language,1 script,2 territory,3 variant,4 subdivision. Joined
  replacements are single-space separated, unique, no empty tokens; only territory/subdivision may
  have >1 replacement.
- 4 (2 words): `(source_locale_sid, maximized_locale_sid)`, sorted source_sid. Target is
  language-script-region; must include `und` source.
- 5 (6 words): `(extension_sid, key_sid, value_type, space_joined_aliases_sid, preferred_sid,
  deprecated_bool)`, sorted extension/key. value_type 0 single,1 multiple,2 incremental,3 any.
  Extension `u` or `t`.
- 6 (8 words):
  `(extension_sid,key_sid,type_name_sid,space_joined_aliases_sid,preferred_sid,flags,iana_sid,region_sid)`,
  sorted extension/key/name. flags bit 0 deprecated, bit 1 variable, no other bits. IANA/region may
  be empty. Raw registered type spelling/alias data is not identical to an advertised formatter
  capability; preferred chains are validated separately.

Source `tools/intl/locale_tables.py`; `tools/intl/locale_metadata.py`; C
`src/intl/locale/native-data.c` and its record validators.

## 7 WEEKS / 8 CALENDAR_PREFERENCES / 9 HOUR_PREFERENCES / 11 SCRIPT_DIRECTIONS

No local header. Directory count = rows, not L; sorted first string ID. All four present together or
absent together.

- 7: 4 words `(region_sid, first_day, minimum_days, weekend_mask)`. Weekdays 1 Monday..7 Sunday; min
  1..7; mask bit 0 Monday..bit 6 Sunday, 1..127.
- 8: 3 words `(target_sid, space_joined_calendar_ids_sid, token_count)`; ordered preference list,
  not set.
- 9: 3 words `(target_sid, space_joined_hour_ids_sid, token_count)`; likewise.
- 11: 2 words `(script_sid,direction)` direction 0 empty, 1 ltr, 2 rtl.

Sections 7/8/9 require world `001` default. Source `tools/intl/preference_tables.py`;
`src/intl/locale/preference-data.c`. Target can be locale/region per source grammar, not necessarily
section 2 row.

## 10 LISTS / 12 LIST_CONTEXTS

Directory count=L. Each independently: `set_count; locale_to_set[L];
deduplicated_sets[set_count][width]`. No version. Shared vectors strictly sorted/unique, every set
used.

- 10 width 36: 9 variants ×4 template sids. Variants in order
  standard,standard-short,standard-narrow,or,or-short,or-narrow,unit,unit-short,unit-narrow. Parts
  in order `2,start,middle,end`. Fully resolved source templates (not inheritance markers).
- 12 width 27: same 9 variants × `(context_kind,pair_sid,end_sid)`. Kind 0 none (both sids zero),1
  Spanish-and,2 Spanish-or,3 Hebrew-and. These replacement templates supplement matching default
  pair/end templates; not arbitrary context-language dispatch. Section 12 requires 10.

Source `tools/intl/tables.py`; `tools/intl/list_patterns.py`; C `src/intl/list/data.c`,
`src/intl/list/native-engine.h`.

## 13 NUMBERING_SYSTEMS / 14 NUMBERING_PREFERENCES

No local header. Directory count is local row count. Both present together.

- 13 width 3 `(name_sid,kind,value_sid)`, name sorted. Kind 0 numeric, value contains precisely 10
  distinct Unicode scalar digits; kind 1 algorithmic, value is rule-reference metadata. Must contain
  `latn` / `0123456789`.
- 14 width 5 `(locale_sid,default,native,traditional,finance)`, locale_sid sorted. Four preferences
  are section 13 definition indexes. Only traditional/finance may use ABSENT or BLOCKED. Root source
  row required. This table can contain metadata locales distinct from profile registry L.

Source `tools/intl/numbering_tables.py`; C `src/intl/locale/numbering-data.c`. Algorithmic row
presence does not implement its rule system.

## 15 NUMBER_SYMBOLS

Directory count=L. Header (3 words) words `(S,V,R)` = numeric-system count,vector count,shared-row
count. Then:

`system_definition_indices[S]; locale_to_row[L]; rows[R][S]; vectors[V][15]`.

System indices equal all numeric definitions in section 13 order, not string IDs. Rows index
vectors. Field order:
decimal,group,list,percentSign,plusSign,minusSign,approximatelySign,exponential,superscriptingExponent,perMille,infinity,nan,timeSeparator,currencyDecimal,currencyGroup.
Last 2 optional allow ABSENT/BLOCKED; others must resolve to valid string IDs (empty may be
schema-valid where data permits).

Source `tools/intl/number_symbol_tables.py`; `tools/intl/number_symbols.py`; C
`src/intl/number/symbol-data.c`.

## 16 PLURALS

Directory count=L. Header (2 words) `(rule_vector_count,range_vector_count)`. Then:

`locale_kind_pairs[L][2][2]:u32; rules[rule_vector_count][6]:u32; ranges[range_vector_count][36]:u8`.

Per locale kind 0 cardinal then 1 ordinal. Pair=`(rule_vector_index,range_vector_index)`. Six
categories zero,one,two,few,many,other. Rule entry=condition sid or ABSENT; other mandatory
empty-string sid 0, other categories nonempty canonical relation strings. Range byte
index=start_category*6+end_category; byte 0..5 result category or 0xff missing. 36 bytes/vector, not
36 words! No local version. Source `tools/intl/plural_tables.py`; C `src/intl/plural/data.c`.

## Common number tokens for sections 17/18

Token (2 words) `(kind,text_sid)`, kinds 0 literal,1 number placeholder,2 minus,3 plus,4 percent,5
permille,6 currency symbol,7 currency ISO,8 currency name,9 narrow currency. Literal nonempty
string, no adjacent literals; nonliteral text_sid=0. Affix descriptor (2 words)
`(first_token,token_count)` covers canonical concatenated token table. Numeric forms include a
number placeholder at its correct position, not just pre/post substrings. Compact LITERAL-mode forms
instead contain no number token.

Source `tools/intl/number_pattern_syntax.py`; `src/intl/number/pattern-data.h`;
`src/intl/number/pattern-data.c` validation.

## 17 NUMBER_PATTERNS (version 2)

Directory count=L. Header (8 words) `(2,S,V,R,P,A,T,O)`
=systems,vectors,rows,patterns,affixes,tokens,overrides.

Then exactly:
`system_definition_indices[S]; locale_records[L][2]; rows[R][S]; vectors[V][9]; overrides[O][2]; patterns[P][12]; affix_descriptors[A][2]; tokens[T][2]`.

Locale record `(minimum_grouping_digits,row_index)`, grouping>0. Vector (9 fields)
decimal,scientific,percent,currency-standard,currency-accounting,currency-standard-alpha,currency-accounting-alpha,currency-standard-none,currency-accounting-none.
First 5 mandatory P index; final 4 can be ABSENT/BLOCKED/EMPTY.
Override=`(raw_pattern_sid,pattern_index)`, sorted raw sid; legacy currency override reuse typed
patterns.

Pattern (12 words)
=`(primary_grouping,secondary_grouping,min_integer_digits,integer_placeholders,min_fraction_digits,max_fraction_digits,decimal_point_bool,exponent_digits,exponent_plus_bool,positive_affix,negative_affix,plus_affix)`.

Source `tools/intl/number_pattern_tables.py`; C `src/intl/number/pattern-data.c` and
`src/intl/number/pattern-data.h`. These LDML digit settings are metadata; actual ECMAScript options
decide rounding.

## 18 COMPACT_PATTERNS (version 1)

Directory count=L. Header (8 words) `(1,S,R,M,E,P,A,T)`
systems,rows,maps,entries,patterns,affixes,tokens. Then:

`system_definition_indices[S]; locale_to_row[L]; rows[R][S*3]; map_descriptors[M][2]; entries[E][4]; patterns[P][6]; affix_descriptors[A][2]; tokens[T][2]`.

Rows order system-major,kind-minor; kind 0 decimal-short, 1 decimal-long, 2 currency-short.
Entry=`(magnitude,plural_count_selector,variant,pattern_index_or_BLOCKED_or_EMPTY)`. Magnitude is
power-of-ten exponent, not numeric 10^n. Count selectors 0..7 represent
`'0','1',zero,one,two,few,many,other`. Variant 0 standard, 1 alphaNextToNumber, 2 noCurrency. Map
absence is no selector entry, not ABSENT word.

Pattern (6 words)
=`(mode,divisor_exponent,zero_placeholders,positive_affix,negative_affix,plus_affix)`; mode 0
scaled, 1 literal, 2 fallback. Scaled divisor relationship exponent+zeros=magnitude+1, literal uses
family baseline exponent, fallback=`0` ordinary formatting. Tokens use the same encoding as section
17, excluding percent/permille. Maps/rows/patterns/affix sequences are canonical, sorted and used;
descriptors cover payload. Source `tools/intl/compact_tables.py`;
`tools/intl/compact_pattern_syntax.py`; C `src/intl/number/compact-data.c`.

## 19 CURRENCIES (version 2)

Directory count=L. Header (9 words)
`(2,C,F,V,M,default_digits,default_rounding,default_cash_digits,default_cash_rounding)`.

Then `code_sids[C]; fraction_overrides[F][5]; locale_to_map[L]; maps[M][C]; vectors[V][17]`.

Inventory currencies uppercase 3-letter ASCII, includes formatting codes and supplemental fraction
codes. Fraction override `(code_index,digits,rounding,cash_digits,cash_rounding)`, sorted code
index. Map slot=vector index or ABSENT. Vector first 16 fields: 9 names in counts
`'',0,1,zero,one,two,few,many,other`, then symbol,narrow,formal,variant,decimal,group,pattern.
Fields are sid/ABSENT/BLOCKED/EMPTY. The 17th word is the root-name provenance mask: bits 1..8
correspond nonempty count names, bit 0 forbidden; referenced field cannot be ABSENT. Root provenance
influences consumer fallback; not a lexical alias.

Source `tools/intl/currency_tables.py`; `tools/intl/currencies.py`; C `src/intl/currency/data.c` and
`src/intl/currency/data.h`. `pattern` is raw sid here; section 17 overrides locate its compiled
program.

## 20 NUMBER_LAYOUTS (version 3)

Directory count=L. Header (5 words) `(3,S,V,R,CV)` numeric systems,vectors,rows,currency-only
vectors.

Then `system_definition_indices[S]; locale_to_row[L]; rows[R][S]; vectors[V][20];
locale_to_currency_vector[L]; currency_vectors[CV][9]`.

Vector first 19 fields: range,approximately,atLeast,atMost; currency-unit-count for counts
0,1,zero,one,two,few,many,other (8); beforeCurrency-currencyMatch/surroundingMatch/insertBetween
(3); afterCurrency same 3; currencyPatternAppendISO. The 20th word is the root field mask, only
currency-unit bits 4..11. Fields sid/ABSENT/BLOCKED (empty uses sid 0, no EMPTY sentinel). Required
range/misc/spacing cannot be absent/blocked; insertBetween may be empty. Currency-only vector 8
count pattern sids/sentinels +root mask (8 bits), intentionally independent of selected numbering
system.

Source `tools/intl/number_layout_tables.py`; `tools/intl/number_layout.py`; C
`src/intl/number/layout-data.c`.

## 21 UNIT_PATTERNS (version 2)

Directory count=L. Header (8 words) `(2,G,C,R,M,E,OR,N)` groups,cases,sharedbase
rows,maps,entries,sharedoverlay rows,nodes. W=3*G. Then exactly:

`groups[G][2]; case_sids[C]; locale_to_base_row[L]; base_rows[R][W]; locale_to_overlay_row[L]; overlay_rows[OR][W]; nodes[N][4]; map_descriptors[M][2]; entries[E][2]`.

Group=`(kind,name_sid)` kind 0 compound/name per, 1 unit/unit ID. Groups sorted kind/name, starts
compound/per. Cases sorted, first empty. Column=width*G+group, width 0 long, 1 short, 2 narrow. Base
row values map index; overlay row values head ID 0 none or 1..N. Node
`(origin_locale_sid,source_column,map_index,parent_node_ID)`; parent < own one-based ID, zero ends,
breadth-first canonical order parent/locale/column/map. Source column may differ from requested
column through alias but same group family. Origins need not be profile registry locales.
Descriptors count entries.

Entry=`(packed_selector,text_sid_or_BLOCKED)`; selector high 8 bits field, next 8 bits count tag,
low 16 bits case index. Field 0 displayName, 1 unitPattern, 2 perUnitPattern, 3 gender, 4
compoundUnitPattern. Count tag 0 no count, 1..8 counts `'0','1',zero,one,two,few,many,other`. Only
unitPattern carries count. Cases apply to unitPattern/compoundUnitPattern. All other absence
represented by no entry, explicit empty = sid 0; no ABSENT/EMPTY sentinel. Overlay preserves exact
per-field source precedence; do not flatten away provenance/blocking.

Source `tools/intl/unit_tables.py`; `tools/intl/unit_patterns.py`; C `src/intl/unit/data.c`. Maps
may be empty, but overlay nodes require their assigned map nonempty. No independent family word
stored in wire; validated from entries.

## 22 UNIT_GRAMMAR (version 1)

Directory count=L. Header (2 words) `(1,R)`, then `locale_to_row[L]; rows[R][6]`. Six words are
`(plural_origin_sid,plural_first_token_sid,plural_second_token_sid,case_origin_sid,case_first_token_sid,case_second_token_sid)`.
Derivations currently division/per; metadata for composition, not arbitrary grammar program. Sorted
shared rows, all referenced. Source `tools/intl/unit_grammar_tables.py`;
`tools/intl/unit_grammar.py`; C `src/intl/unit/grammar-data.c`.

## 23 RELATIVE_PATTERNS (version 1)

Directory count=L. Header (5 words) `(1,K,R,M,E)` literalkeys,sharedrows,maps,entries. Then
`literal_key_sids[K]; locale_to_row[L]; rows[R][24]; map_descriptors[M][2]; entries[E][4]`.

Row24 unit-major, width-minor: units second,minute,hour,day,week,month,quarter,year × width long,
short, narrow. Entry `(selector,text_sid_or_BLOCKED,origin_locale_sid,origin_unit_width_column)`;
selector 0..5 past zero/one/two/few/many/other, 6..11 future same categories, 12 + literal key index
for signed canonical integer offset strings. Literal key inventory is lexically sorted/string IDs
ascending, **not numerically sorted**. Origin column unit*3+width,may differ from requested column.
Empty = sid 0, missing = no entry; long forms require usable past/other and future/other. Source
`tools/intl/relative_tables.py`; `tools/intl/relative_patterns.py`; C `src/intl/relative/data.c`.

## 24 DURATION_PATTERNS (version 1)

Directory count=L. Header (3 words) `(1,R,N)` shared rows, records. Then `locale_to_row[L];
rows[R][3]; records[N][4]`. Columns are skeletons hm, hms, ms. Record
`(state,text_sid,origin_locale_sid,source_skeleton_index)`; state 0 missing canonical all zero, 1
value, 2 blocked. Blocked has empty text and present origin, value may be empty/origin present.
Source key 0 hm / 1 hms / 2 ms may differ from requested column. Raw digital patterns, not compiled
tokens. Rows/records lexically sorted, deduplicated and fully referenced. Source
`tools/intl/duration_tables.py`; C `src/intl/duration/data.c`.


## 25: DATE_FIELD_NAMES

Encoder/decoder: `tools/intl/date_field_tables.py`.
C owner: `src/intl/datetime/field-data.c`.

Header: `(version=1, A alternate_count, R shared_row_count, V record_count)` (4 words).
Then:

1. `alternate_sids[A]` (sorted unique; entry 0 is empty).
2. `locale_row_indexes[N]`.
3. `rows[R][12*3*A]`: zero-based indexes into records.
4. `records[V][6]`.

Each record is `(state, text_sid, origin_locale_sid, origin_field_enum, origin_width_enum,
origin_alternate_index)`.
Row column order: field outermost, then width, then alternate innermost:
`column=(field*3+width)*A+alternate`.

Field enums in order: `era, year, quarter, month, weekOfYear, weekday, day, dayPeriod, hour, minute,
second, timeZoneName`.
Width enums: `long, short, narrow`.

Exact length in words: `4 + A + N + R*(36*A) + V*6`.
All rows/records are sorted unique and all referenced; `1<=R<=N`, `1<=V<=R*36*A`. State/provenance
reflect the resolved originating leaf, which may differ from the requested selector.

## 26: DISPLAY_NAMES

Encoder/decoder: `tools/intl/display_tables.py`.
C owner: `src/intl/display/data.c`, slicing its open routine.

Header `(1, R row_count, M map_count, H node_count, E entry_count, K key_count)` (6 words).
Then:

1. `locale_row_indexes[N]`.
2. `rows[R][7]` (heads in group-enum order).
3. `maps[M][2]`.
4. `nodes[H][4]`.
5. `entries[E][6]`.
6. `lookup_keys[K][2]`.

Group enums: `language, script, region, variant, calendar, pattern, bracket`.
Entry `(normalized_code_sid, alt_sid, menu_sid, scope_sid, original_code_sid, text_sid_or_BLOCKED)`;
key is first four words. Normalization applies to the first five groups only: ASCII lower-case plus
`_`→`-`; the original code remains stored. Pattern/bracket keys are preserved. Menu selector is only
valid for language; scope selector only for calendar.

Lookup-key record `(group_enum, normalized_code_sid)` is the sorted unique exact union over actual
node/map memberships, not a leaf reference or a count/offset. It gives the key inventory independent
of layers. Parents/heads must keep the exact display-name group.

Length `6 + N + R*7 + M*2 + H*4 + E*6 + K*2`.

## 27: DATE_STYLES

Encoder/decoder `tools/intl/date_style_tables.py`.
C owner `src/intl/datetime/style-data.c`, slicing its open routine.

Header `(1, C calendar_count, A alternate_count, R shared_row_count, V record_count)` (5 words).
Then `calendar_sids[C]`, `alternate_sids[A]`, `locale_row_indexes[N]`,
`rows[R][C*2*4*2*A]`, `records[V][9]`.

Calendar catalog is sorted unique source names, not the engine's calendar enum. Alternates sorted
unique, first empty.
Row column nesting: `calendar, family, width, field, alternate`:
`column=((((calendar*2+family)*4+width)*2+field)*A+alternate)`.
Family: `date, time`. Width: `full, long, medium, short`. Field: `pattern, skeleton`.
Record `(state, text_sid, numbers_sid, origin_locale_sid, origin_calendar_index, origin_family_enum,
origin_width_enum, origin_field_enum, origin_alternate_index)`.
`numbers_sid` preserves the CLDR numbers override string; it does not independently authorize a numbering-system choice.

Length `5+C+A+N+R*(C*16*A)+V*9`. Dense resolved state semantics apply; do not interpret record 0 as
always missing unless its state actually is zero.

## 28: DATE_COMBINATIONS

Encoder/decoder `tools/intl/date_combination_tables.py`.
C owner `src/intl/datetime/combination-data.c`, slicing its open routine.

Header `(1,C calendar_count,A alternate_count,R shared_row_count,V record_count)`.
Then `calendar_sids[C]`, `alternate_sids[A]`, `locale_row_indexes[N]`,
`rows[R][C*4*3*A]`, `records[V][8]`.
Column nesting `calendar,width,form,alternate`, i.e.
`column=(((calendar*4+width)*3+form)*A+alternate)`.
Width `full,long,medium,short`. Form `standard,atTime,relative`.
Record
`(state,text_sid,numbers_sid,origin_locale_sid,origin_calendar_index,origin_width_enum,origin_form_enum,origin_alternate_index)`.
Length `5+C+A+N+R*(C*12*A)+V*8`.
These are source date/time connector templates; selection/composition/formatting remain consumers'
responsibility.

## 29: CALENDAR_NAMES

Encoder/decoder `tools/intl/calendar_name_tables.py`.
C owner `src/intl/datetime/name-data.c`, slicing its open routine.

Header `(1,C calendar_count,G group_count,R row_count,M map_count,H node_count,E entry_count)` (7
words).
Then `calendar_sids[C]`, `groups[G][5]`, `locale_row_indexes[N]`,
`rows[R][G]`, `maps[M][2]`, `nodes[H][4]`, `entries[E][4]`.
Group `(calendar_index,family_enum,cycle_enum,context_enum,width_enum)` in numeric sorted order.

Enums (zero-based, exactly these orders):

- family `cyclicNameSets,dayPeriods,days,eras,monthPatterns,months,quarters`.
- cycle `empty,years,months,days,dayParts,zodiacs,solarTerms`.
- context `empty,format,stand-alone,numeric`.
- width `abbreviated,narrow,wide,short,all`.
- leaf year variant `empty,standard,leap`.

Entry `(code_sid,year_variant_enum,leaf_alt_sid,text_sid_or_BLOCKED)`, key first 3 words. Nonempty
year variant only valid for months. Era code is a raw numeric CLDR era identifier **string**, not an
engine-era array position. Month/quarter/day/day-period/cycle codes remain typed strings checked by
family.

Length `7+C+G*5+N+R*G+M*2+H*4+E*4`.
Parent and requested-head compatibility is same family, not necessarily identical group/calendar.
Missing/empty/blocked handling must remain exact; width/context/year/alt fallback is a separate
consumer policy, not an implicit reader behavior.

## 30: DATE_AVAILABLE_FORMATS

Encoder/decoder `tools/intl/date_available_tables.py`.
C owner `src/intl/datetime/available-data.c`.

Same header/array skeleton as section 29, but `groups[G][3]`:
`(calendar_index,family_enum,group_alt_sid)`.
Family `availableFormats,appendItems`.
Entry `(code_sid,plural_enum,leaf_alt_sid,text_sid_or_BLOCKED)`.
Plural enum `empty,zero,one,two,few,many,other`. Empty means **NO_PLURAL**, not other. appendItems
requires NO_PLURAL.
For availableFormats, code is source skeleton/identifier; for appendItems a CLDR request name. A
source identifier is not sufficient evidence its pattern grammar or component set is renderable.

Length `7+C+G*3+N+R*G+M*2+H*4+E*4`.
Group alternate and leaf alternate are separate selectors; first visible exact leaf masks parents
before any filtering, plural/alternate selection, or renderability assessment. Parent/head
compatibility same family.

## 31: DATE_INTERVALS

Encoder/decoder `tools/intl/date_interval_tables.py`.
C owner `src/intl/datetime/interval-data.c`.

Header `(1,C calendar_count,S suffix_count,R shared_block_count,M map_count,H node_count,E
entry_count)`.
Then:

1. `calendar_sids[C]`.
2. `suffixes[S][4]` = `(group_alt_sid,family_enum,skeleton_sid,item_alt_sid)` in numeric sorted
order.
3. `locale_calendar_block_indexes[N][C]`, locale outermost, calendar inner.
4. `blocks[R][S]`, one head per suffix.
5. `maps[M][2]`.
6. `nodes[H][4]`.
7. `entries[E][3]` = `(code_sid,leaf_alt_sid,text_sid_or_BLOCKED)`, first two words key.

Family `fallback,item,range`. The item family requires a nonempty skeleton and permits an empty or
nonempty item-alt; fallback/range require both empty. Item difference codes are `B G H M a d h m y`;
range codes are `numeric,non-numeric,mixed`; fallback code is empty.
Global node group index is a **factored computed index** `calendar_index*S+suffix_index`; there is
no stored flat group array. Resolve requested locale/calendar via its block index, then suffix head.
Its parent's semantic family may agree while calendar/suffix differs.

Length `7+C+S*4+N*C+R*S+M*2+H*4+E*3`.
The factored rectangular inventory is intentional: do not expand a huge locale×calendar×suffix
matrix in documentation examples or runtime validation allocations.

## 32: TIMEZONE_NAMES

Encoder/decoder `tools/intl/timezone_name_tables.py`.
C owner `src/intl/datetime/timezone-name-data.c`.

Header `(1,G group_count,M map_count,H node_count,E entry_count,R0,R1,R2,R3)` (9 words).
Groups `groups[G][5]` = `(family_enum,entity_sid,entity_alt_sid,width_enum,width_alt_sid)` in
numeric sorted order.
Family `format,city,zone-name,metazone-name`. Width `empty,long,short`.

After groups, for **each family in enum order** with `Gf` groups:

- If Gf=0, Rf must be 0 and **no bytes** for that family.
- Else `locale_row_indexes_f[N]`, then `rows_f[Rf][Gf]` of heads. Groups are the family's
  subsequence of the global sorted group array; with current numeric family-first ordering this is
  contiguous. `1<=Rf<=N`.

Then `maps[M][2]`, `nodes[H][4]`, `entries[E][4]`.
Entry `(code_sid,type_enum,leaf_alt_sid,text_sid_or_BLOCKED)`, first 3 words are the key.
Type `empty,standard,daylight`; nonempty only for format/regionFormat.
Formats:
`hourFormat,gmtFormat,gmtZeroFormat,gmtUnknownFormat,dualOffsetFormat,regionFormat,fallbackFormat`.
City code `exemplarCity`. Zone/metazone name codes `generic,standard,daylight`.

Length `9+G*5+sum_{Gf>0}(N+Rf*Gf)+M*2+H*4+E*4`.
Zone-name and metazone-name have the same semantic leaf family and may alias each other; format and
city are separate. This is localized presentation, **not** IANA transition/offset data or the
timezone identity backend.

## 33: TIMEZONE_PRESENTATION

Encoder/decoder `tools/intl/timezone_presentation_tables.py`.
C owner `src/intl/datetime/timezone-presentation-data.c`, slicing its open routine.

**N is zone descriptor count, not locale count**. Global supplemental data, not locale-indexed.
Header `(1,P period_count,F preferred_count,D primary_count,I identity_count)` (5 words).
Then:

1. `identities[I][2]` = `(long_metazone_sid,short_metazone_sid)` sorted by long SID. Short is unique
four lower-case ASCII letters (not a BCP47 timezone short ID).
2. `zones[N][3]` = `(iana_zone_sid,period_start_index,period_count)` sorted by zone SID; spans
partition periods contiguously, may be empty.
3. `periods[P][8]` =
`(start_lo,start_hi,end_lo,end_hi,metazone_identity_index,flags,standard_offset_bits,daylight_offset_bits)`.
4. `preferred[F][3]` = `(metazone_identity_index,territory_sid,zone_descriptor_index)`, sorted by
first 2 words; unique pair.
5. `primary[D][2]` = `(territory_sid,iana_zone_sid)`, sorted by territory SID; unique territory.
Last word is a **string ID**, not a descriptor index and need not have a periods descriptor.

Flags START=1, END=2, STANDARD_OFFSET=4, DAYLIGHT_OFFSET=8. Other bits invalid. Missing-flag
associated words are exactly zero. Timestamps are signed two's-complement 64-bit seconds, assembled
`lo | hi<<32`; full signed domain valid. No start/end flag means unbounded, **not** timestamp 0.
When both bounds are present, start < end; per-zone periods chronological, nonoverlapping; gaps
allowed.
Offset words are signed two's-complement 32-bit seconds, integral minutes in ±86340. Flag
distinguishes explicit 0 from absent. A preferred zone must have membership in the requested
metazone among its period records. Territory is uppercase 2 letters or numeric 3 digits. Primary
zone need not be listed in preferred/membership.

Length `5+I*2+N*3+P*8+F*3+D*2`.
Release/since metadata stays in provenance, not these runtime records. Metazone period/name
presentation data is not a substitute for IANA offset-transition calculations. Store flags
explicitly; do not reserve extreme timestamp values as infinity.

## 34: DAYPERIOD_RULES

Encoder/decoder `tools/intl/dayperiod_tables.py`.
C owner `src/intl/datetime/dayperiod-data.c`, slicing its open routine.
Rule validation: `tools/intl/dayperiod_rules.py` and C owner's `valid_rules`; vocabulary
`tools/intl/dayperiod_codes.py`, `src/intl/datetime/dayperiod-code.h`.

**N is explicit (kind,origin_locale) row count, not selected locale count**. Global supplemental
source origins, including origins outside a minimal selected locale inventory.
Header `(1,M map_count,Q rule_count)` (3 words).
Then:

1. `rows[N][3]` = `(kind_enum,origin_locale_sid,map_index)`, strictly sorted by first 2 words.
2. `maps[M][2]` = `(rule_start_index,rule_count)`, contiguous partition, **empty maps allowed**; all
referenced. Lists sorted unique lexicographically, including empty list if present.
3. `rules[Q][4]` = `(code_enum,form_enum,start_second,end_second)`, sorted per list; code unique per
list.

Kind `format,selection`. Form `at,range`.
Code order
`am,pm,midnight,noon,morning1,morning2,afternoon1,afternoon2,evening1,evening2,night1,night2`.
Start in 0..86399; end in 0..86400; both multiples of 60. End 86400 is 24:00 exclusive. Range with
end<=start wraps midnight; variable ranges must not have equal endpoints.
Fixed semantics exactly:
`am=(range,0,43200)`, `pm=(range,43200,86400)`,
`midnight=(at,0,0)`, `noon=(at,43200,43200)`.
Variable codes only ranges. When variable ranges exist, their split/wrapped segments partition all
86400 seconds with no gaps/overlap. Fixed am/pm and exact points may coexist and are not part of
that partition proof. Format ranges additionally touch at most 12 distinct hour buckets
(`(unwrapped_end-1)//3600 - start//3600 +1 <=12`), stronger than duration <= 12 hours. Selection
does not impose this limit; a shared map referenced by format and selection must satisfy format
constraints.

Length `3+N*3+M*2+Q*4`; at most 12 rules per map. Missing row and explicit empty map differ; there
is no blocked sentinel here. Consumer inheritance truncates locale structurally, selects one whole
explicit list, does not follow main-data exceptional parents or merge leaves across origins.
Format/selection independent.

## 35 — Calendar era name keys

Encoder/decoder `tools/intl/calendar_era_tables.py`; source
`tools/intl/calendar_eras.py`. C owner `src/intl/datetime/era-data.c`.

**N is the number of explicit canonical-era mappings**, independent of selected
locale count. Header `(1)` followed by `rows[N][3]` =
`(raw_calendar_sid, canonical_era_code_sid, raw_era_name_key_sid)`.
Length is exactly `1+3*N` words; an empty table is valid.

Rows are strictly ordered by `(raw_calendar_sid, canonical_era_code_sid)`;
each `(raw_calendar_sid, raw_era_name_key_sid)` must also be unique. Raw calendar
identities match `[a-z0-9]+(-[a-z0-9]+)*`; canonical era codes match
`[a-z0-9]{2,8}(-[a-z0-9]{3,8})*`. Raw keys are nonempty decimal ASCII strings,
without leading zeroes except `0`; they are not row positions or C enums.

Only explicit code-bearing `calendarData/calendar/eras/era` records are retained.
No localized labels, era boundaries, alias expansion or inherited-era flattening
occurs here. In particular, Chinese/Dangi sequential epoch records without a
canonical code do not create era 0, and Japanese inherited Gregorian eras are
resolved explicitly by the chronology/presentation consumer. Future canonical
codes and noncontiguous raw IDs remain data, without changing a compiled table.

## 36 — Root collation elements

Encoder/decoder `tools/intl/collation_tables.py`; authenticated source collector
`tools/intl/collation_root.py`; C owner `src/intl/collation/root-data.c`.
This optional section describes explicit CLDR
root UCA mappings, independently of selected locale inventories. Its presence
does not establish executable Collator support or apply locale tailorings.

**N is the number of mappings**, and must be positive. Header words are
`(1, unicode_version, K, E)`. The first word is this section's format version;
`unicode_version` packs major/minor/patch in bits 16/8/0. The current source
version is Unicode 18.0.0, encoded as `18 << 16`.

The header is followed by:

1. `mappings[N][4]` = `(key_start, key_length, element_start, element_length)`.
2. `keys[K]`, Unicode scalar values stored as little-endian u32.
3. `elements[E]`, each eight bytes: little-endian u16 primary, secondary,
   tertiary and variable flag, in that order.

Total length is exactly `4 + 4*N + K + 2*E` words. Each mapping has nonzero
key and element lengths. Both start/length pairs form contiguous partitions
of their entire respective pool, beginning at zero; no unused suffixes,
overlaps or forward references are permitted. This initial representation
keeps validation independent of temporary allocation or vector-interning state.

Keys are strictly ordered by lexicographic scalar-sequence order, including
prefixes before longer keys. Scalars include U+0000 and noncharacters, but
exclude surrogate code points and values above U+10FFFF. These sequences do
not enter the NUL-free UTF-8 string pool. Handling an unpaired UTF-16 surrogate
at runtime is a separate collation algorithm concern.

Weights retain all three source levels, including zero/ignorable weights.
The variable flag must be 0 or 1; a variable element has nonzero primary
weight. Source contractions and ordered expansions remain intact. Neither
key length nor expansion length is restricted to the observed source maxima.
Normalization, implicit-weight derivation, case/numeric options, variable
handling and locale tailoring are consumers of this data, not encoded results
or additional comparison levels.

The C reader borrows an immutable validated pack without allocation or a
dependency on locale/string sections. It provides exact and longest contiguous
scalar-prefix lookup; a missing section or mapping is `UNSUPPORTED`. Prefix
lookup preserves an earlier complete key while narrowing its extensions, so a
later mismatch can fall back to that key. It does not implement UCA's
discontiguous contractions or establish executable service availability.


## 37 — Root collation prefix contexts

Collector/codec `tools/intl/collation_contexts.py`; C metadata reader
`src/intl/collation/contexts.c`. Both profiles include this section. Its presence
does not establish executable Collator support.

CLDR root `FractionalUCA.txt` distinguishes preceding-context rules such as
`L|·` from ordinary contractions. `allkeys_CLDR.txt` represents their combined
CE sequences as if they were contractions; section 36 alone loses this
distinction. Prefix matching observes already-consumed normalized input and
must not consume or emit the preceding context again. Overlapping contractions
make this observably different from a contraction for `L·`.

**N counts context records**, including zero when a source has none. The two
header words are `(1, 18 << 16)`. They are followed by `records[N][3]` with
`(source_mapping_index, prefix_scalar_count, prefix_element_count)`, for an
exact size of `2 + 3*N` words. Source indices refer to section 36 and must be
strictly increasing. Each scalar/element split must be nonzero and smaller
than its source mapping's corresponding count. The leading scalar sequence
must be an exact root mapping whose entire CE vector equals the leading
`prefix_element_count` CEs. The remaining scalar/CE slices are the contextual
suffix mapping. No key, weight or string pool is duplicated.

The collector reads prefix identity from the authenticated FractionalUCA file,
validates its version and prefix-record syntax, and verifies the equivalent
allkeys decomposition. It does not convert fractional weights or pair up CE
vectors from the two encodings, which can have different expansion lengths.
The current four mappings require 56 bytes, excluding the pack directory row.

The reader validates both the root section and all metadata without allocation,
then returns borrowed `before` and `mapping` slices plus the source index. It
provides storage metadata only. The shared matcher in
`src/intl/collation/match.c` replaces the marked pseudo-contractions with prefix
rules and matches preceding context against unchanged NFD text. It owns the
normalized input, successor links for consumed positions and equal-CCC group
ends. Full contiguous matches are checkpoints; discontiguous extension requires
a full single-code-point match at each step. Skipped marks remain available for
subsequent mappings. Input consumption does not rewrite the normalized text.

The matcher reads a CE-free key index over the validated rows. It returns a
mapping ID and suffix scalar count, unmatched scalar or optional numeric run. The raw
element facade in `src/intl/collation/elements.c` emits root expansion CEs or
constructs implicit weights; its context reader applies the prefix CE skip.
Mapping payload fields belong to that reader, not the shared key index.
Comparison settings and locale tailoring remain
separate responsibilities. Group
skipping and iterative successor compression bound particular rescanning costs;
contiguous lookahead still depends on the table's key depth, so this is not a
claim of universal linear matching for arbitrary tables.

The private root comparator in `src/intl/collation/compare.c` consumes those
streams in primary, secondary, then tertiary order. It retains nonzero CEs for
later levels only while primaries tie, with a small inline cache and checked
allocator-backed growth. Variable weights are non-ignorable; there is no
code-point tie-breaker. This foundation does not establish locale-tailoring or
Collator capability. The source-data CI job checks adjacent strings in the full
pinned CLDR non-ignorable ordering file; quick repository tests use authored
weights, level precedence, canonical equivalence and allocation failures.

Numeric root comparison recognizes adjacent Unicode `Nd` runs in NFD text,
before mapping individual digits. It removes leading zeros once per complete
run, then compares significant length and decimal values without a machine
integer limit or ICU's 254-digit segmentation. Non-digits terminate runs even
when their collation weights are ignorable. Equal numbers share one logical
secondary/tertiary weight, independent of digit script and leading-zero count.
Stored CEs remain unchanged: the comparison layer doubles ordinary primary
weights and places a numeric token in the odd band immediately before ordinary
zero. Script reordering and locale-tailored numeric processing are separate
integration work. The Unicode decimal query reuses indexed category runs; the
generator checks repeated ascending decimal values at their original origins.

Root comparison sensitivity selects primary only (`base`), primary and
secondary (`accent`), primary and case (`case`), or all three original levels
(`variant`, the default). Root case is derived separately for each CE from
TR35's allkeys tertiary classes, including Kana size distinctions. `caseFirst`
changes case priority within each tertiary CE; it does not insert a whole case
pass ahead of other tertiary variants. `off` preserves raw tertiary order and
is distinct from `lower`. Primary-ignorable accents do not contribute to
case-only comparison, while tertiary-only CEs keep the artificial highest case
rank required by well-formedness even with upper-first order. These root rules
do not substitute for recomputing case metadata on future locale tailorings.

`ignore_punctuation` uses CLDR's root variable flags, which cover the space and
punctuation groups in this pin (`maxVariable=punct`). Shifted variables and the
primary-ignorable CEs following them do not contribute through tertiary; the
next nonvariable primary restores ordinary handling. Numeric tokens count as
nonvariable primaries, but removed punctuation still terminates source digit
runs. No quaternary level is exposed by these sensitivity options. Source flags
are per CE, not a test of the input character's General_Category or an unsafe
upper-threshold check that would also erase reserved U+FFFE. Symbols, currency,
trailing weights and implicit continuations remain significant. The CI shifted
ordering audit checks only the implemented first three levels, allowing ties
which the source file's fourth level may distinguish.

### Tailoring source syntax (no executable section yet)

`tools/intl/collation_rules.py` collects the pinned release's ordered rule
programs before semantic compilation. Source-located nodes retain reset
positions, relation strengths, prefix and extension strings, starred scalar
ranges, settings, imports, suppression and optimization commands. Tool escapes
are decoded before rule syntax; decoded syntax and whitespace still require
apostrophe quoting to be literals. Unquoted whitespace ends a rule string.
UnicodeSet commands accept the pinned scalar/range/pattern-escape subset and
reject unsupported operators instead of discarding them.

Programs retain locale, type, alternate, draft and reference metadata. An empty
root standard program is present data, distinct from missing or alternate
programs. Repeated operations/imports stay ordered. Source positions identify
the XML collation and original rule-text line/column, not an invented absolute
XML line. The source catalog described below resolves aliases, collation-specific parents
and ordered imports. NFD-inert star validation, canonical closure and CE
allocation remain semantic weight-compilation work. This collector creates no binary section
and advertises no executable locale collation capability.

`tools/intl/collation_programs.py` owns that source catalog. It reuses
`parents.ParentGraph` with the `collations` component and aliases/preferred names
from `locale_metadata`; it does not duplicate a language-wide inheritance or
hardcoded collation alias table. Exact lookup searches a requested type through
its parents without concatenating ancestor programs. Normal profile selection
applies TR35's requested/search/default/standard/root type fallback. Explicit
alternate lookup does not substitute short/proposed and normal programs.

Import lookup is separate from profile fallback: a missing imported type is an
error, and absent `co` means standard, not the imported locale's default. Import
expansion preserves execution order, including repeated imports of cached
immutable syntax. An explicit stack detects active cycles, limits depth and
counts all visited nodes, including imports into empty programs; a branching
empty import graph must not bypass the work budget. Original node provenance
survives expansion. Private programs may be imported but are excluded from complete-profile
selection and establish no public service inventory; private defaults reject. The root standard program must exist and be empty.
