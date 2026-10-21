/* Whole-artifact preflight precedes every semantic visitor.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-ir.h"
#include "../../src/cutils/cutils.h"
#include <string.h>

#define HEADER_SIZE 52u
#define PROGRAM_WORK_MAX 1000000u
#define ARTIFACT_WORK_MAX 4000000u

typedef struct Reader {
    const uint8_t *at, *end;
} Reader;

typedef struct Budget {
    uint32_t program_work, work;
} Budget;

static int word(Reader *reader, uint32_t *out)
{
    if ((size_t)(reader->end - reader->at) < 4)
        return 0;
    *out = qjs_intl_read_u32(reader->at);
    reader->at += 4;
    return 1;
}

static int label(Reader *reader, uint32_t length, QJSIntlIRLabel *out)
{
    const uint8_t *end, *next;
    int code;
    if (length > (size_t)(reader->end - reader->at))
        return 0;
    out->bytes = reader->at;
    out->length = length;
    end = reader->at + length;
    for (; reader->at < end; reader->at = next) {
        code = unicode_from_utf8(reader->at, (int)(end - reader->at), &next);
        if (code < 0x20 || code == 0x7f || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            return 0;
    }
    return 1;
}

static int scalars(Reader *reader, QJSIntlIRScalars *out)
{
    uint32_t i;
    if (!word(reader, &out->count) || out->count > (size_t)(reader->end - reader->at) / 4)
        return 0;
    out->bytes = reader->at;
    reader->at += (size_t)out->count * 4;
    for (i = 0; i < out->count; i++) {
        uint32_t code = qjs_intl_ir_scalar(out, i);
        if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            return 0;
    }
    return 1;
}

static int ranges(const QJSIntlIRScalars *text, uint32_t kind, Budget *budget)
{
    uint32_t i;
    if ((text->count & 1) || (kind == QJS_INTL_IR_STARRED && !text->count))
        return 0;
    for (i = 0; i < text->count; i += 2) {
        uint32_t first = qjs_intl_ir_scalar(text, i), last = qjs_intl_ir_scalar(text, i + 1), count;
        if (first > last || (first <= 0xdfff && last >= 0xd800))
            return 0;
        /* Optimize is a validated hint, not an enumerated semantic operation.
           Count repeated suppression ranges too: compact records must not hide
           millions of mutations outside starred-only accounting. */
        if (kind == QJS_INTL_IR_OPTIMIZE)
            continue;
        count = last - first + 1;
        if (count > PROGRAM_WORK_MAX - budget->program_work || count > ARTIFACT_WORK_MAX - budget->work)
            return 0;
        budget->program_work += count;
        budget->work += count;
    }
    return 1;
}

static int instruction(Reader *program, QJSIntlIRInstruction *out, Budget *budget,
                       QJSIntlIRDiagnostic *error)
{
    Reader reader;
    uint32_t size, source_length, i;
    const uint8_t *start = program->at;
    error->source = (QJSIntlIRLabel){0};
    error->line = error->column = 0;
    if (!word(program, &size) || size < 36 || size > (size_t)(program->end - start))
        return 0;
    reader = (Reader){program->at, start + size};
    program->at = reader.end;
    if (!word(&reader, &out->kind) || !word(&reader, &out->auxiliary) ||
        !word(&reader, &out->line) || !out->line || !word(&reader, &out->column) || !out->column ||
        !word(&reader, &source_length) || !source_length || !label(&reader, source_length, &out->source))
        return 0;
    error->source = out->source;
    error->line = out->line;
    error->column = out->column;
    for (i = 0; i < 3; i++) {
        if (!scalars(&reader, out->fields + i))
            return 0;
    }
    if (reader.at != reader.end)
        return 0;
    switch (out->kind) {
    case QJS_INTL_IR_RESET:
        return out->auxiliary <= 3 && !!out->fields[0].count != !!out->fields[1].count && !out->fields[2].count;
    case QJS_INTL_IR_RELATION:
        return out->auxiliary <= 4 && out->fields[1].count;
    case QJS_INTL_IR_STARRED:
    case QJS_INTL_IR_SUPPRESS:
    case QJS_INTL_IR_OPTIMIZE:
        return out->auxiliary <= (out->kind == QJS_INTL_IR_STARRED ? 4u : 0u) &&
            !out->fields[1].count && !out->fields[2].count && ranges(out->fields, out->kind, budget);
    case QJS_INTL_IR_SETTING:
        return !out->auxiliary && out->fields[0].count && out->fields[1].count && !out->fields[2].count &&
            !qjs_intl_ir_scalar(out->fields + 1, out->fields[1].count - 1);
    default:
        return 0;
    }
}

static int label_order(QJSIntlIRLabel a, QJSIntlIRLabel b)
{
    uint32_t length = a.length < b.length ? a.length : b.length;
    int order = memcmp(a.bytes, b.bytes, length);
    return order ? order : a.length < b.length ? -1 : a.length > b.length ? 1 : 0;
}

static int program_order(const QJSIntlIRProgram *a, const QJSIntlIRProgram *b)
{
    int order = label_order(a->locale, b->locale);
    if (!order)
        order = label_order(a->collation, b->collation);
    if (!order)
        order = label_order(a->alternate, b->alternate);
    return order;
}

static QJSIntlStatus scan(const QJSIntlIRView *view, const QJSIntlIRVisitor *visitor, void *opaque,
                          QJSIntlIRDiagnostic *error)
{
    Reader reader = {view->bytes + HEADER_SIZE, view->bytes + view->length};
    QJSIntlIRProgram previous = {0};
    Budget budget = {0};
    uint32_t i;
    for (i = 0; i < view->program_count; i++) {
        Reader program;
        QJSIntlIRProgram data;
        const uint8_t *start = reader.at;
        uint32_t size, locale, collation, alternate, j;
        QJSIntlStatus status;
        *error = (QJSIntlIRDiagnostic){.program_index = i};
        if (!word(&reader, &size) || size < 20 || size > (size_t)(reader.end - start))
            return QJS_INTL_DATA_ERROR;
        program = (Reader){reader.at, start + size};
        reader.at = program.end;
        if (!word(&program, &locale) || !locale || !word(&program, &collation) || !collation ||
            !word(&program, &alternate) || !word(&program, &data.instruction_count) ||
            !label(&program, locale, &data.locale) || !label(&program, collation, &data.collation) ||
            !label(&program, alternate, &data.alternate) ||
            data.instruction_count > (size_t)(program.end - program.at) / 36 ||
            (i && program_order(&previous, &data) >= 0))
            return QJS_INTL_DATA_ERROR;
        previous = data;
        budget.program_work = 0;
        if (visitor && visitor->begin && (status = visitor->begin(opaque, &data)) != QJS_INTL_OK)
            return status;
        for (j = 0; j < data.instruction_count; j++) {
            QJSIntlIRInstruction node;
            error->instruction_index = j;
            if (!instruction(&program, &node, &budget, error))
                return QJS_INTL_DATA_ERROR;
            if (visitor && visitor->instruction && (status = visitor->instruction(opaque, &node)) != QJS_INTL_OK)
                return status;
        }
        if (program.at != program.end)
            return QJS_INTL_DATA_ERROR;
        error->source = (QJSIntlIRLabel){0};
        error->line = error->column = 0;
        error->instruction_index = data.instruction_count;
        if (visitor && visitor->end && (status = visitor->end(opaque, &data)) != QJS_INTL_OK)
            return status;
    }
    return reader.at == reader.end ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
}

QJSIntlStatus qjs_intl_ir_open(const uint8_t *bytes, size_t length, const uint8_t *expected,
                               QJSIntlIRView *out, QJSIntlIRDiagnostic *error)
{
    QJSIntlIRDiagnostic local = {0};
    QJSIntlIRView view;
    QJSIntlStatus status;
    if (!error)
        error = &local;
    *error = (QJSIntlIRDiagnostic){0};
    if (out)
        *out = (QJSIntlIRView){0};
    if (!bytes || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    if (length < HEADER_SIZE || length > QJS_INTL_IR_MAX_SIZE || memcmp(bytes, "QJCI", 4) ||
        qjs_intl_read_u32(bytes + 4) != 2 || qjs_intl_read_u32(bytes + 8) != QJS_INTL_IR_UNICODE_VERSION ||
        qjs_intl_read_u32(bytes + 12) != length ||
        qjs_intl_read_u32(bytes + 16) > (length - HEADER_SIZE) / 20 ||
        (expected && memcmp(bytes + 20, expected, 32)))
        return QJS_INTL_DATA_ERROR;
    view = (QJSIntlIRView){bytes, bytes + 20, (uint32_t)length, qjs_intl_read_u32(bytes + 16)};
    status = scan(&view, NULL, NULL, error);
    if (!status)
        *out = view;
    return status;
}

QJSIntlStatus qjs_intl_ir_visit(const QJSIntlIRView *view, const QJSIntlIRVisitor *visitor, void *opaque,
                                QJSIntlIRDiagnostic *error)
{
    QJSIntlIRDiagnostic local = {0};
    if (!error)
        error = &local;
    *error = (QJSIntlIRDiagnostic){0};
    if (!view || !view->bytes || !view->fingerprint || !visitor)
        return QJS_INTL_INVALID_ARGUMENT;
    return scan(view, visitor, opaque, error);
}
