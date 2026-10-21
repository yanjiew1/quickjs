/* Borrowed logical root input for the host tailoring compiler, not the engine.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMPILER_ROOT_INPUT_H
#define QJS_INTL_COMPILER_ROOT_INPUT_H
#include "../../src/intl/collation/coordinates.h"
#include "../../src/intl/collation/keys.h"
#include "../../src/intl/collation/base.h"

enum {
    QJS_INTL_ROOT_META, QJS_INTL_ROOT_MAPPINGS, QJS_INTL_ROOT_KEYS,
    QJS_INTL_ROOT_CONTEXTS, QJS_INTL_ROOT_CES, QJS_INTL_ROOT_HAN_RANGES,
    QJS_INTL_ROOT_HAN_RANKS, QJS_INTL_ROOT_RESETS, QJS_INTL_ROOT_WEAK_LIMITS,
    QJS_INTL_ROOT_GROUPS, QJS_INTL_ROOT_TOKENS, QJS_INTL_ROOT_TOKEN_BYTES,
    QJS_INTL_ROOT_SECTION_COUNT
};

/* Typed source positions in the QJCR record order; parsing one does not imply
   that the semantic executor supports that reset form. */
typedef enum QJSIntlRootReset {
    QJS_INTL_ROOT_FIRST_TERTIARY_IGNORABLE = 1,
    QJS_INTL_ROOT_LAST_TERTIARY_IGNORABLE,
    QJS_INTL_ROOT_FIRST_SECONDARY_IGNORABLE,
    QJS_INTL_ROOT_LAST_SECONDARY_IGNORABLE,
    QJS_INTL_ROOT_FIRST_PRIMARY_IGNORABLE,
    QJS_INTL_ROOT_LAST_PRIMARY_IGNORABLE,
    QJS_INTL_ROOT_FIRST_VARIABLE,
    QJS_INTL_ROOT_LAST_VARIABLE,
    QJS_INTL_ROOT_FIRST_REGULAR,
    QJS_INTL_ROOT_LAST_REGULAR,
    QJS_INTL_ROOT_FIRST_IMPLICIT,
    QJS_INTL_ROOT_LAST_IMPLICIT,
    QJS_INTL_ROOT_FIRST_TRAILING,
    QJS_INTL_ROOT_LAST_TRAILING
} QJSIntlRootReset;

enum {
    QJS_INTL_ROOT_SOURCE_CE = 1,
    QJS_INTL_ROOT_ARTIFICIAL_BOUNDARY,
    QJS_INTL_ROOT_DERIVED_INITIAL
};

typedef struct QJSIntlRootInputSection {
    const uint8_t *bytes;
    uint32_t length, count;
} QJSIntlRootInputSection;

typedef struct QJSIntlRootInputMetadata {
    QJSIntlCollationRootLimits limits;
    uint32_t han_base, han_count, generic_base, generic_count, generic_limit;
    uint32_t implicit_first_marker, implicit_last_marker;
    uint32_t variable_top, numeric, field_separator, last_regular;
} QJSIntlRootInputMetadata;

typedef struct QJSIntlRootInput {
    QJSIntlRootInputSection sections[QJS_INTL_ROOT_SECTION_COUNT];
    QJSIntlRootInputMetadata metadata;
    QJSIntlCollationBase base; /* Opened common semantics; compiler facts stay here. */
    QJSIntlCollationKeyIndex keys;
    const uint8_t *fingerprint; /* Borrowed 32-byte source-manifest identity. */
} QJSIntlRootInput;

/* Initialize fresh/cleared, disjoint output. Bytes are immutable and outlive
   the view and every copied key slice. Only opening allocates temporary audit
   storage; no allocation is retained. A failed open clears out and distinguishes
   allocation failure from malformed data. The driver must pair the fingerprint
   with its independently acquired program/source identity. */
QJSIntlStatus qjs_intl_root_input_open(const uint8_t *, size_t,
                                      const QJSIntlAllocator *, QJSIntlRootInput *out);
/* Resolve a mapping's span into the once-validated CE pool, without expansion.
   element_at reads a pool index, not a mapping ID. All outputs are disjoint and
   reset on failure. Accessors allocate nothing. */
QJSIntlStatus qjs_intl_root_input_mapping(const QJSIntlRootInput *, uint32_t mapping_id,
                                         uint32_t *ce_start, uint32_t *ce_count);
QJSIntlStatus qjs_intl_root_input_element_at(const QJSIntlRootInput *, uint32_t,
                                            QJSIntlCollationCoordinate *);
/* Exact ASCII catalog token -> source group ID. Aliases can name the same
   group; the semantic reorder owner handles duplicates and others/Zzzz.
   No allocation; output resets on failure. */
QJSIntlStatus qjs_intl_root_input_group(const QJSIntlRootInput *, QJSIntlBytes token,
                                        uint32_t *group_id);
/* Read an initial source position, not a derived current logical position.
   Role and CE outputs reset on failure. This query allocates nothing. */
QJSIntlStatus qjs_intl_root_input_reset(const QJSIntlRootInput *, QJSIntlRootReset,
                                        unsigned int *role, QJSIntlCollationCoordinate *);
/* Original source keys are scalars; deterministic fallback also accepts isolated
   UTF-16 surrogate units. Declared Han uses radical order; other codes use the
   arithmetic generic interval. */
QJSIntlStatus qjs_intl_root_input_implicit(const QJSIntlRootInput *, uint32_t code,
                                          QJSIntlCollationCoordinate *);
#endif
