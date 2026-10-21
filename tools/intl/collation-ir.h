/* Borrowed counted program input, independent of Unicode semantics/execution.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMPILER_IR_H
#define QJS_INTL_COMPILER_IR_H
#include "../../src/intl/common/types.h"
#include "../../src/intl/native/wire.h"
#include <string.h>

#define QJS_INTL_IR_UNICODE_VERSION (18u << 16)
#define QJS_INTL_IR_MAX_SIZE (64u * 1024 * 1024)

typedef struct QJSIntlIRLabel {
    const uint8_t *bytes;
    uint32_t length;
} QJSIntlIRLabel;

typedef struct QJSIntlIRScalars {
    const uint8_t *bytes; /* LE32 words, potentially unaligned. */
    uint32_t count;
} QJSIntlIRScalars;

/* Index is less than count in a slice from a validated immutable view. */
static inline uint32_t qjs_intl_ir_scalar(const QJSIntlIRScalars *text, uint32_t index)
{
    return qjs_intl_read_u32(text->bytes + (size_t)index * 4);
}

/* Counted scalar text versus a host ASCII literal. terminated includes the one
   trailing scalar NUL used by setting values, never a C-string assumption. */
static inline int qjs_intl_ir_ascii_equal(const QJSIntlIRScalars *text, const char *value, int terminated)
{
    size_t length = strlen(value), i;
    if (text->count != length + (terminated != 0))
        return 0;
    for (i = 0; i < length; i++) {
        if (qjs_intl_ir_scalar(text, (uint32_t)i) != (unsigned char)value[i])
            return 0;
    }
    return !terminated || !qjs_intl_ir_scalar(text, (uint32_t)length);
}

enum {
    QJS_INTL_IR_RESET = 1, QJS_INTL_IR_RELATION, QJS_INTL_IR_STARRED,
    QJS_INTL_IR_SETTING, QJS_INTL_IR_SUPPRESS, QJS_INTL_IR_OPTIMIZE
};

typedef struct QJSIntlIRProgram {
    QJSIntlIRLabel locale, collation, alternate;
    uint32_t instruction_count;
} QJSIntlIRProgram;

typedef struct QJSIntlIRInstruction {
    uint32_t kind, auxiliary, line, column;
    QJSIntlIRLabel source;
    QJSIntlIRScalars fields[3];
} QJSIntlIRInstruction;

typedef struct QJSIntlIRDiagnostic {
    QJSIntlIRLabel source;
    uint32_t line, column, program_index, instruction_index;
} QJSIntlIRDiagnostic;

typedef struct QJSIntlIRView {
    const uint8_t *bytes, *fingerprint;
    uint32_t length, program_count;
} QJSIntlIRView;

typedef struct QJSIntlIRVisitor {
    QJSIntlStatus (*begin)(void *, const QJSIntlIRProgram *);
    QJSIntlStatus (*instruction)(void *, const QJSIntlIRInstruction *);
    QJSIntlStatus (*end)(void *, const QJSIntlIRProgram *);
} QJSIntlIRVisitor;

/* Validate ALL structure, identities and expansion budgets without allocation
   or callbacks. expected_fingerprint is NULL for a standalone audit, otherwise
   points to the root's 32-byte manifest identity. Outputs reset on failure.
   Input bytes remain immutable and alive through every visit. Output/error
   storage is disjoint from each other and the input. */
QJSIntlStatus qjs_intl_ir_open(const uint8_t *, size_t, const uint8_t *expected_fingerprint,
                               QJSIntlIRView *, QJSIntlIRDiagnostic *);
/* Only successfully opened, unchanged views may be visited. Callback descriptor
   pointers last for that call; their byte slices borrow input lifetime. Callbacks
   are optional. First error propagates without further callbacks; end is success
   finalization, not error cleanup. The executor must clear active state itself.
   Unicode properties/normalization and mutable execution belong to consumers. */
QJSIntlStatus qjs_intl_ir_visit(const QJSIntlIRView *, const QJSIntlIRVisitor *, void *,
                                QJSIntlIRDiagnostic *);
#endif
