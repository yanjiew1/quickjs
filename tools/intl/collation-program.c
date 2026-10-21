/* Command order stays separate from structural input and construction owners.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-program.h"
#include "collation-text.h"
#include "../../src/cutils/cutils.h"
#include "../../src/unicode/libunicode.h"
#include <string.h>

typedef struct Executor {
    const QJSIntlRootInput *root;
    const QJSIntlAllocator *allocator;
    QJSIntlCollationProgramFinalizer finalizer;
    void *opaque;
    QJSIntlCollationProgram program;
    QJSIntlIRDiagnostic pending;
    uint32_t program_index, instruction_index;
    int have_reset, pending_reset, dangling;
} Executor;

static void clear(Executor *state)
{
    qjs_intl_collation_reorder_clear(&state->program.reorder);
    qjs_intl_collation_builder_clear(&state->program.builder);
    memset(&state->program, 0, sizeof(state->program));
    state->have_reset = state->pending_reset = 0;
}

static QJSIntlStatus begin(void *opaque, const QJSIntlIRProgram *identity)
{
    Executor *state = opaque;
    QJSIntlStatus status;
    (void)identity;
    state->instruction_index = 0;
    status = qjs_intl_collation_builder_init(&state->program.builder, state->root, state->allocator);
    if (!status)
        status = qjs_intl_collation_reorder_init(&state->program.reorder, state->root, state->allocator);
    qjs_intl_collation_settings_init(&state->program.policy);
    return status;
}

static QJSIntlStatus decode(const QJSIntlAllocator *allocator, const QJSIntlIRScalars *text,
                            QJSIntlCompilerText *out)
{
    uint32_t i;
    *out = (QJSIntlCompilerText){0};
    if (!text->count)
        return QJS_INTL_OK;
    if ((uint64_t)text->count * sizeof(*out->codes) > SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    out->codes = allocator->malloc(allocator->opaque, (size_t)text->count * sizeof(*out->codes));
    if (!out->codes)
        return QJS_INTL_NO_MEMORY;
    out->count = text->count;
    for (i = 0; i < text->count; i++)
        out->codes[i] = qjs_intl_ir_scalar(text, i);
    return QJS_INTL_OK;
}

static QJSIntlStatus reset(Executor *state, const QJSIntlIRInstruction *node)
{
    static const char *const names[] = {
        "first tertiary ignorable", "last tertiary ignorable",
        "first secondary ignorable", "last secondary ignorable",
        "first primary ignorable", "last primary ignorable",
        "first variable", "last variable", "first regular", "last regular",
        "first implicit", "last implicit", "first trailing", "last trailing"
    };
    QJSIntlCompilerText text = {0};
    QJSIntlStatus status;
    unsigned int i;
    if (state->pending_reset)
        return QJS_INTL_DATA_ERROR;
    if (node->fields[1].count) {
        for (i = 0; i < sizeof(names) / sizeof(*names); i++) {
            if (qjs_intl_ir_ascii_equal(node->fields + 1, names[i], 0))
                return qjs_intl_collation_builder_logical(&state->program.builder,
                                                           (QJSIntlRootReset)(i + 1), node->auxiliary);
        }
        return QJS_INTL_DATA_ERROR;
    }
    status = decode(state->allocator, node->fields, &text);
    if (!status)
        status = qjs_intl_collation_builder_reset(&state->program.builder, text.codes, text.count, node->auxiliary);
    qjs_intl_compiler_text_clear(state->allocator, &text);
    return status;
}

static QJSIntlStatus relate(Executor *state, const QJSIntlIRInstruction *node)
{
    QJSIntlCompilerText text[3] = {{0}};
    QJSIntlStatus status = QJS_INTL_OK;
    unsigned int i;
    for (i = 0; i < 3 && !status; i++)
        status = decode(state->allocator, node->fields + i, text + i);
    /* Decode only: the builder checks original prefix boundaries before NFD. */
    if (!status)
        status = qjs_intl_collation_builder_relate(&state->program.builder, node->auxiliary,
                                                   text[0].codes, text[0].count,
                                                   text[1].codes, text[1].count,
                                                   text[2].codes, text[2].count);
    for (i = 0; i < 3; i++)
        qjs_intl_compiler_text_clear(state->allocator, text + i);
    return status;
}

static QJSIntlStatus ranges(Executor *state, const QJSIntlIRInstruction *node)
{
    uint32_t i;
    /* The reader bounded enumerated cardinalities for the entire artifact,
       including repeated suppression ranges. Optimize remains a no-op hint. */
    if (node->kind == QJS_INTL_IR_OPTIMIZE)
        return QJS_INTL_OK;
    for (i = 0; i < node->fields[0].count; i += 2) {
        uint32_t code = qjs_intl_ir_scalar(node->fields, i);
        uint32_t last = qjs_intl_ir_scalar(node->fields, i + 1);
        for (; code <= last; code++) {
            QJSIntlStatus status;
            if (node->kind == QJS_INTL_IR_SUPPRESS) {
                /* A UnicodeSet selector is an exact scalar, not a string to
                   normalize or match through inherited contractions. */
                status = qjs_intl_collation_map_suppress(&state->program.builder.map, code);
            } else {
                int inert;
                status = qjs_intl_compiler_text_inert(state->allocator, code, &inert);
                if (!status && !inert)
                    status = QJS_INTL_DATA_ERROR;
                if (!status)
                    status = qjs_intl_collation_builder_relate(&state->program.builder, node->auxiliary,
                                                               NULL, 0, &code, 1, NULL, 0);
            }
            if (status)
                return status;
        }
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus instruction(void *opaque, const QJSIntlIRInstruction *node)
{
    Executor *state = opaque;
    QJSIntlStatus status;
    uint32_t index = state->instruction_index++;
    switch (node->kind) {
    case QJS_INTL_IR_RESET:
        status = reset(state, node);
        if (!status) {
            state->have_reset = state->pending_reset = 1;
            state->pending = (QJSIntlIRDiagnostic){node->source, node->line, node->column,
                                                  state->program_index, index};
        }
        return status;
    case QJS_INTL_IR_RELATION:
    case QJS_INTL_IR_STARRED:
        if (!state->have_reset || (state->program.builder.before &&
                                  state->program.builder.before != node->auxiliary))
            return QJS_INTL_DATA_ERROR;
        status = node->kind == QJS_INTL_IR_RELATION ? relate(state, node) : ranges(state, node);
        if (!status)
            state->pending_reset = 0;
        return status;
    case QJS_INTL_IR_SETTING:
        status = qjs_intl_collation_setting_apply(&state->program.policy, node);
        if (status == QJS_INTL_UNSUPPORTED)
            status = qjs_intl_collation_reorder_apply(&state->program.reorder, node);
        return status;
    case QJS_INTL_IR_SUPPRESS:
    case QJS_INTL_IR_OPTIMIZE:
        return ranges(state, node);
    default:
        return QJS_INTL_UNSUPPORTED;
    }
}

static QJSIntlStatus end(void *opaque, const QJSIntlIRProgram *identity)
{
    Executor *state = opaque;
    QJSIntlStatus status;
    if (state->pending_reset) {
        state->dangling = 1;
        return QJS_INTL_DATA_ERROR;
    }
    status = state->finalizer ? state->finalizer(state->opaque, identity, &state->program) : QJS_INTL_OK;
    clear(state);
    state->program_index++;
    return status;
}

QJSIntlStatus qjs_intl_collation_program_execute(const QJSIntlRootInput *root, const QJSIntlIRView *view,
                                                 const QJSIntlAllocator *allocator,
                                                 QJSIntlCollationProgramFinalizer finalizer, void *opaque,
                                                 QJSIntlIRDiagnostic *error)
{
    const QJSIntlIRVisitor visitor = {begin, instruction, end};
    Executor state = {0};
    QJSIntlStatus status;
    if (error)
        *error = (QJSIntlIRDiagnostic){0};
    if (!root || !root->fingerprint || !view || !view->bytes || !view->fingerprint ||
        !allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    if (memcmp(root->fingerprint, view->fingerprint, 32))
        return QJS_INTL_DATA_ERROR;
    if ((((uint32_t)LIBUNICODE_UNICODE_VERSION_MAJOR << 16) |
         (LIBUNICODE_UNICODE_VERSION_MINOR << 8) | LIBUNICODE_UNICODE_VERSION_PATCH) != QJS_INTL_IR_UNICODE_VERSION)
        return QJS_INTL_UNSUPPORTED;
    state.root = root;
    state.allocator = allocator;
    state.finalizer = finalizer;
    state.opaque = opaque;
    status = qjs_intl_ir_visit(view, &visitor, &state, error);
    if (state.dangling && error)
        *error = state.pending;
    /* end() is not called for failed begin/instruction; always clear the active
       program too. Clearing after a successful finalizer is harmless. */
    clear(&state);
    return status;
}
