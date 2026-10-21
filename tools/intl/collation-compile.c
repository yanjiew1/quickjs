/* Host tailoring compiler input and pinned-Unicode normalization audit.
 * This stage does not allocate weights or emit executable tailorings.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-root-input.h"
#include "collation-ir.h"
#include "../../src/cutils/cutils.h"
#include "../../src/unicode/libunicode.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IR_MAX_SIZE QJS_INTL_IR_MAX_SIZE
#define IR_UNICODE_VERSION (((uint32_t)LIBUNICODE_UNICODE_VERSION_MAJOR << 16) | \
                            (LIBUNICODE_UNICODE_VERSION_MINOR << 8) | LIBUNICODE_UNICODE_VERSION_PATCH)

typedef QJSIntlIRScalars Scalars;

typedef struct Audit {
    uint64_t programs, instructions, texts, starred;
    int preflight;
} Audit;

static void *normalize_realloc(void *opaque, void *ptr, size_t size)
{
    QJSIntlStatus *failure = opaque;
    void *result;
    if (!size) {
        free(ptr);
        return NULL;
    }
    if (size > IR_MAX_SIZE) {
        if (failure)
            *failure = QJS_INTL_OVERFLOW;
        return NULL;
    }
    result = realloc(ptr, size);
    if (!result && failure)
        *failure = QJS_INTL_NO_MEMORY;
    return result;
}

static QJSIntlStatus normalize_text(const Scalars *text, Audit *audit)
{
    uint32_t *input, *normalized = NULL, i;
    int length;
    QJSIntlStatus failure = QJS_INTL_OK;
    if (!text->count || audit->preflight)
        return QJS_INTL_OK;
    /* Decode counted wire scalars without assuming native alignment/endian. */
    input = malloc((size_t)text->count * sizeof(*input));
    if (!input)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i < text->count; i++)
        input[i] = qjs_intl_ir_scalar(text, i);
    length = unicode_normalize(&normalized, input, text->count, UNICODE_NFD,
                               &failure, normalize_realloc);
    free(input);
    free(normalized);
    if (length < 0)
        return failure ? failure : QJS_INTL_DATA_ERROR;
    audit->texts++;
    return QJS_INTL_OK;
}

static QJSIntlStatus audit_starred(const Scalars *text, Audit *audit)
{
    uint32_t i;
    /* Structural preflight already counted cardinalities across every program.
       Semantic checks still examine every interior scalar before normalization. */
    for (i = 0; i < text->count; i += 2) {
        uint32_t first = qjs_intl_ir_scalar(text, i), last = qjs_intl_ir_scalar(text, i + 1), code;
        audit->starred += last - first + 1;
        if (audit->preflight)
            continue;
        for (code = first; code <= last; code++) {
            uint32_t *normalized = NULL;
            int length, inert;
            QJSIntlStatus failure = QJS_INTL_OK;
            if (unicode_get_cc(code))
                return QJS_INTL_DATA_ERROR;
            length = unicode_normalize(&normalized, &code, 1, UNICODE_NFD,
                                       &failure, normalize_realloc);
            inert = length == 1 && normalized[0] == code;
            free(normalized);
            if (length < 0)
                return failure ? failure : QJS_INTL_DATA_ERROR;
            if (!inert)
                return QJS_INTL_DATA_ERROR;
        }
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus audit_instruction(void *opaque, const QJSIntlIRInstruction *node)
{
    Audit *audit = opaque;
    QJSIntlStatus status = QJS_INTL_OK;
    uint32_t i;
    const Scalars *fields = node->fields;
    switch (node->kind) {
    case QJS_INTL_IR_RESET:
        status = normalize_text(fields, audit);
        break;
    case QJS_INTL_IR_RELATION:
        /* Keep the ORIGINAL boundary check in the nonallocating semantic pass.
           Extensions are independent text, not part of the prefix split. */
        if (fields[0].count &&
            (!unicode_nfc_boundary_before(qjs_intl_ir_scalar(fields, 0)) ||
             !unicode_nfc_boundary_before(qjs_intl_ir_scalar(fields + 1, 0))))
            return QJS_INTL_DATA_ERROR;
        for (i = 0; i < 3 && !status; i++)
            status = normalize_text(fields + i, audit);
        break;
    case QJS_INTL_IR_STARRED:
        status = audit_starred(fields, audit);
        break;
    default:
        break; /* Structural audit does not promise semantic setting support. */
    }
    if (!status)
        audit->instructions++;
    return status;
}

static QJSIntlStatus audit_end(void *opaque, const QJSIntlIRProgram *program)
{
    Audit *audit = opaque;
    (void)program;
    audit->programs++;
    return QJS_INTL_OK;
}

static void *root_malloc(void *opaque, size_t size)
{
    (void)opaque;
    return malloc(size);
}

static void root_free(void *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}

static int audit_root(const uint8_t *bytes, size_t size)
{
    QJSIntlAllocator allocator = {NULL, root_malloc, normalize_realloc, root_free};
    QJSIntlRootInput view;
    QJSIntlStatus status = qjs_intl_root_input_open(bytes, size, &allocator, &view);
    if (status) {
        fprintf(stderr, "invalid logical root input or allocation failure (%d)\n", (int)status);
        return 1;
    }
    printf("{\"mappings\":%u,\"ces\":%u,\"han\":%u,\"primary_max\":%u}\n",
           view.keys.row_count, view.sections[QJS_INTL_ROOT_CES].count,
           view.metadata.han_count, view.metadata.limits.primary_max);
    return 0;
}

static int read_input(const char *path, uint8_t **bytes, size_t *length)
{
    FILE *file = fopen(path, "rb");
    long size;
    int failed;
    *bytes = NULL;
    *length = 0;
    if (!file)
        return 1;
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 20 || size > IR_MAX_SIZE ||
        fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return 1;
    }
    *bytes = malloc((size_t)size);
    if (!*bytes) {
        fclose(file);
        return 1;
    }
    failed = fread(*bytes, 1, (size_t)size, file) != (size_t)size;
    if (fclose(file))
        failed = 1;
    if (failed) {
        free(*bytes);
        *bytes = NULL;
    } else {
        *length = (size_t)size;
    }
    return failed;
}

int main(int argc, char **argv)
{
    uint8_t *bytes, *root_bytes = NULL;
    size_t size, root_size;
    QJSIntlRootInput root = {0};
    QJSIntlIRView view;
    QJSIntlIRDiagnostic error;
    const QJSIntlIRVisitor visitor = {NULL, audit_instruction, audit_end};
    QJSIntlAllocator allocator = {NULL, root_malloc, normalize_realloc, root_free};
    Audit audit = {.preflight = 1};
    int failed, root_mode, pair_mode;
    pair_mode = argc == 4 && !strcmp(argv[1], "--audit-pair");
    if (!pair_mode && (argc != 3 || (strcmp(argv[1], "--audit-ir") && strcmp(argv[1], "--audit-root")))) {
        fprintf(stderr, "usage: %s --audit-ir input.qjci | --audit-root root.qjcr | "
                "--audit-pair root.qjcr input.qjci\n", argv[0]);
        return 2;
    }
    root_mode = !strcmp(argv[1], "--audit-root");
    if (IR_UNICODE_VERSION != QJS_INTL_IR_UNICODE_VERSION) {
        fputs("compiler Unicode version does not match its input schema\n", stderr);
        return 1;
    }
    if (pair_mode) {
        if (read_input(argv[2], &root_bytes, &root_size))
            return 1;
        if (qjs_intl_root_input_open(root_bytes, root_size, &allocator, &root)) {
            free(root_bytes);
            fputs("invalid logical root input or allocation failure\n", stderr);
            return 1;
        }
    }
    if (read_input(argv[pair_mode ? 3 : 2], &bytes, &size)) {
        free(root_bytes);
        return 1;
    }
    if (root_mode) {
        failed = audit_root(bytes, size);
        free(bytes);
        return failed ? 1 : 0;
    }
    /* Pair identities and preflight the whole artifact before allocating any
       normalization or future per-program execution state. */
    failed = qjs_intl_ir_open(bytes, size, root.fingerprint, &view, &error);
    if (!failed)
        failed = qjs_intl_ir_visit(&view, &visitor, &audit, &error);
    if (!failed) {
        audit = (Audit){0};
        failed = qjs_intl_ir_visit(&view, &visitor, &audit, &error);
    }
    if (failed) {
        if (error.source.bytes)
            fprintf(stderr, "%.*s:%u:%u: ", (int)error.source.length, (const char *)error.source.bytes,
                    error.line, error.column);
        fputs("invalid tailoring IR, source identity or normalization constraint\n", stderr);
    } else {
        printf("{\"programs\":%" PRIu64 ",\"instructions\":%" PRIu64
               ",\"normalized_texts\":%" PRIu64 ",\"starred_scalars\":%" PRIu64 "}\n",
               audit.programs, audit.instructions, audit.texts, audit.starred);
    }
    free(bytes);
    free(root_bytes);
    return failed ? 1 : 0;
}
