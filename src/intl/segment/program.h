/* Prepared segmentation rules and borrowed deterministic machine views.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_SEGMENT_PROGRAM_H
#define QJS_INTL_SEGMENT_PROGRAM_H

#include "../native/data.h"
#include "segment.h"

typedef enum QJSIntlSegmentMachineMode {
    QJS_INTL_SEGMENT_LEFT_RESTART,
    QJS_INTL_SEGMENT_RIGHT_REVERSED_RESTART,
    QJS_INTL_SEGMENT_LEFT_BOS_ONLY
} QJSIntlSegmentMachineMode;

enum {
    QJS_INTL_SEGMENT_PRESERVE = 0,
    QJS_INTL_SEGMENT_LEXICAL = 1,
    QJS_INTL_SEGMENT_SOFT_SENTENCE = 2,
    QJS_INTL_SEGMENT_NEEDS_DICTIONARY = 1,
    QJS_INTL_SEGMENT_NEEDS_SUPPRESSIONS = 2,
    QJS_INTL_SEGMENT_NEEDS_WORD_STATUS = 4,
    QJS_INTL_SEGMENT_NEEDS_GRAPHEMES = 8,
    QJS_INTL_SEGMENT_CONTEXT_TERM = 1,
    QJS_INTL_SEGMENT_CONTEXT_CLOSE = 2,
    QJS_INTL_SEGMENT_CONTEXT_SPACE = 4,
    QJS_INTL_SEGMENT_CONTEXT_SEPARATOR = 8,
    QJS_INTL_SEGMENT_CONTEXT_IGNORED = 16
};

typedef struct QJSIntlSegmentProgramData {
    QJSIntlSection section;
    const uint8_t *descriptors, *bindings, *fingerprint;
    uint32_t program_count, binding_count, locale_count;
} QJSIntlSegmentProgramData;

typedef struct QJSIntlSegmentProgram {
    const uint8_t *bytes, *ends, *classes, *flags, *rules, *machines;
    size_t length;
    uint32_t kind, requirements, class_count, range_count, rule_count, machine_count;
} QJSIntlSegmentProgram;

typedef struct QJSIntlSegmentMachine {
    const uint8_t *transitions, *accepts;
    uint32_t state_count, class_count, mode;
} QJSIntlSegmentMachine;

typedef struct QJSIntlSegmentRule {
    uint32_t left, right;
    unsigned int breaks, role;
} QJSIntlSegmentRule;

/* Require an unchanged successful shared-data view. Validate the whole optional
   section before publishing, without allocation. Canonical first-use/BFS
   numbering proves use/reachability with counters, not graph scratch.
   Copying these fixed views is safe while their immutable pack bytes live.
   No close is required. Missing section/route is UNSUPPORTED. The fingerprint
   is pairing metadata, not authentication; prepared routes are not capability.
   Unknown version/Unicode/profile is UNSUPPORTED; malformed known-format
   records, including bad magic, are DATA_ERROR.
   Outputs must be fresh and disjoint from inputs/bytes; reset on failure. */
QJSIntlStatus qjs_intl_segment_program_data_open(const QJSIntlData *, QJSIntlSegmentProgramData *out);
QJSIntlStatus qjs_intl_segment_program_at(const QJSIntlSegmentProgramData *, uint32_t index,
                                          QJSIntlSegmentProgram *out);
QJSIntlStatus qjs_intl_segment_program_find(const QJSIntlSegmentProgramData *, uint32_t locale,
                                            QJSIntlGranularity, QJSIntlSegmentProgram *out);

/* Queries require unchanged views from successful open. They allocate nothing
   and do not mutate shared traversal state. Classify includes lone surrogates;
   the input owner remains responsible for original UTF-16 boundary mapping. */
QJSIntlStatus qjs_intl_segment_program_classify(const QJSIntlSegmentProgram *, uint32_t code,
                                                uint32_t *out);
QJSIntlStatus qjs_intl_segment_program_rule(const QJSIntlSegmentProgram *, uint32_t index,
                                            QJSIntlSegmentRule *out);
QJSIntlStatus qjs_intl_segment_program_machine(const QJSIntlSegmentProgram *, uint32_t index,
                                               QJSIntlSegmentMachine *out);
QJSIntlStatus qjs_intl_segment_machine_step(const QJSIntlSegmentMachine *, uint32_t state,
                                            uint32_t character_class, uint32_t *out);
QJSIntlStatus qjs_intl_segment_machine_accepts(const QJSIntlSegmentMachine *, uint32_t state,
                                               int *out);

#endif
