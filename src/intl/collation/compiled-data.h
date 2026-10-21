/* Runtime archive views, independent of host compiler data structures.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_COMPILED_DATA_H
#define QJS_INTL_COLLATION_COMPILED_DATA_H
#include "base.h"
#include "policy.h"
#include "ranked.h"
#include "trie.h"

#define QJS_INTL_COMPILED_COLLATION_MAX_SIZE (256u * 1024 * 1024)
#define QJS_INTL_COMPILED_COLLATION_VERSION 2u
#define QJS_INTL_COMPILED_COLLATION_UNICODE (18u << 16)
#define QJS_INTL_COMPILED_COLLATION_HEADER 64u
#define QJS_INTL_COMPILED_COLLATION_PROGRAM_HEADER 64u

enum {
    QJS_INTL_COMPILED_META, QJS_INTL_COMPILED_MAPPINGS, QJS_INTL_COMPILED_KEYS,
    QJS_INTL_COMPILED_CONTEXTS, QJS_INTL_COMPILED_ELEMENTS, QJS_INTL_COMPILED_HAN_RANGES,
    QJS_INTL_COMPILED_HAN_RANKS, QJS_INTL_COMPILED_GROUPS, QJS_INTL_COMPILED_PROGRAMS,
    QJS_INTL_COMPILED_LABELS, QJS_INTL_COMPILED_BLOCKS, QJS_INTL_COMPILED_BINDINGS,
    QJS_INTL_COMPILED_SECTION_COUNT
};

typedef struct QJSIntlCompiledCollationData {
    QJSIntlSection sections[QJS_INTL_COMPILED_SECTION_COUNT];
    QJSIntlCollationBase root;
    const uint8_t *fingerprint;
    uint32_t core_groups[5], hani_group, program_count;
    int ready;
} QJSIntlCompiledCollationData;

typedef struct QJSIntlCompiledCollationProgram {
    QJSIntlCollationBase root;
    QJSIntlSection groups;

    uint32_t core_groups[5], hani_group;
    QJSIntlBytes locale, collation, alternate;

    QJSIntlCollationPolicy policy;

    QJSIntlCollationKeyIndex keys;
    QJSIntlSection elements, suppressed, group_ranks, offsets[4];
    QJSIntlCollationTrie trie;
    uint32_t final_maxima[4];
    int ready;
} QJSIntlCompiledCollationProgram;

typedef struct QJSIntlCollationBinding {
    QJSIntlBytes locale, collation; /* Empty collation is the null default. */
    uint32_t sort_program, search_program; /* UINT32_MAX means unsupported. */
} QJSIntlCollationBinding;

/* Validate the COMPLETE archive before publishing any borrowed view. Bytes are
   immutable/alive through all views and iterators. Opening retains no heap and
   distinguishes malformed data/OOM. expected_fingerprint is NULL or 32 bytes.
   Empty resolved bindings remain valid for standalone compiler output;
   success establishes data validity, not a public locale service capability.
   Output is fresh/disjoint and reset on failure. */
QJSIntlStatus qjs_intl_compiled_collation_open(const uint8_t *, size_t, const uint8_t *expected_fingerprint,
                                               const QJSIntlAllocator *, QJSIntlCompiledCollationData *);
/* Decode a program from an unchanged opened archive, without revalidation or
   allocation. Copy descriptors before creating an overlay/iterator: once those
   borrow program.trie, its address must remain stable. Root descriptors and
   labels are copied views; only archive BYTES must outlive the program, not
   the parent data descriptor. Never modify opened program arrays/policy. */
QJSIntlStatus qjs_intl_compiled_collation_program(const QJSIntlCompiledCollationData *, uint32_t index,
                                                  QJSIntlCompiledCollationProgram *);
QJSIntlStatus qjs_intl_compiled_collation_element(const QJSIntlCompiledCollationProgram *, uint32_t index,
                                                  QJSIntlCollationRankedElement *);
QJSIntlStatus qjs_intl_compiled_collation_mapping(const QJSIntlCompiledCollationProgram *, uint32_t index,
                                                  uint32_t *ce_start, uint32_t *ce_count);
QJSIntlStatus qjs_intl_compiled_collation_root_element(const QJSIntlCompiledCollationProgram *, uint32_t index,
                                                       QJSIntlCollationRankedElement *);
QJSIntlStatus qjs_intl_compiled_collation_implicit(const QJSIntlCompiledCollationProgram *, uint32_t code,
                                                   QJSIntlCollationRankedElement *);
QJSIntlStatus qjs_intl_compiled_collation_root_rank(const QJSIntlCompiledCollationProgram *, unsigned int axis,
                                                    uint32_t ordinal, uint32_t *rank);
/* Binding labels and IDs borrow only archive bytes. No locale fallback here:
   callers first match an available service locale, then resolve its co list.
   Allocate nothing; fresh/disjoint output resets on error. */
QJSIntlStatus qjs_intl_compiled_collation_binding(const QJSIntlCompiledCollationData *, uint32_t index,
                                                  QJSIntlCollationBinding *);
#endif
