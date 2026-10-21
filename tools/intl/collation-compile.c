/* Host tailoring compiler input and pinned-Unicode normalization audit.
 * This stage does not allocate weights or emit executable tailorings.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../../src/cutils/cutils.h"
#include "../../src/unicode/libunicode.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IR_MAX_SIZE (64u * 1024 * 1024)
#define IR_MAX_EXPANDED 1000000u
#define IR_MAX_TOTAL_EXPANDED (4u * IR_MAX_EXPANDED)
#define IR_UNICODE_VERSION (((uint32_t)LIBUNICODE_UNICODE_VERSION_MAJOR << 16) | \
                            (LIBUNICODE_UNICODE_VERSION_MINOR << 8) | LIBUNICODE_UNICODE_VERSION_PATCH)

typedef struct Reader {
    const uint8_t *at, *end;
} Reader;

typedef struct Scalars {
    const uint8_t *data;
    uint32_t count;
} Scalars;

typedef struct Audit {
    uint64_t programs, instructions, texts, starred;
    uint32_t program_starred;
    const uint8_t *source;
    uint32_t source_length, line, column;
    int preflight;
} Audit;

static uint32_t load32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int word(Reader *reader, uint32_t *out)
{
    if ((size_t)(reader->end - reader->at) < 4)
        return -1;
    *out = load32(reader->at);
    reader->at += 4;
    return 0;
}

static int label(Reader *reader, uint32_t length, const uint8_t **out)
{
    const uint8_t *end, *next;
    int code;
    if (length > (size_t)(reader->end - reader->at))
        return -1;
    end = reader->at + length;
    /* Labels are diagnostics, not executable strings. Reject embedded control
       bytes so a malformed artifact cannot forge extra diagnostic lines. */
    for (*out = reader->at; reader->at < end; reader->at = next) {
        code = unicode_from_utf8(reader->at, end - reader->at, &next);
        if (code < 0x20 || code == 0x7f || code > 0x10ffff ||
            (code >= 0xd800 && code <= 0xdfff))
            return -1;
    }
    return 0;
}

static int scalars(Reader *reader, Scalars *out)
{
    uint32_t i, code;
    if (word(reader, &out->count) || out->count > (size_t)(reader->end - reader->at) / 4)
        return -1;
    out->data = reader->at;
    reader->at += (size_t)out->count * 4;
    for (i = 0; i < out->count; i++) {
        code = load32(out->data + (size_t)i * 4);
        if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            return -1;
    }
    return 0;
}

static void *normalize_realloc(void *opaque, void *ptr, size_t size)
{
    (void)opaque;
    if (!size) {
        free(ptr);
        return NULL;
    }
    if (size > IR_MAX_SIZE)
        return NULL;
    return realloc(ptr, size);
}

static int normalize_text(const Scalars *text, Audit *audit)
{
    uint32_t *input, *normalized = NULL, i;
    int length;
    if (!text->count || audit->preflight)
        return 0;
    /* IR extents bound this allocation and the int count accepted by Unicode.
       Decode rather than aliasing an unaligned little-endian scalar vector. */
    input = malloc((size_t)text->count * sizeof(*input));
    if (!input)
        return -1;
    for (i = 0; i < text->count; i++)
        input[i] = load32(text->data + (size_t)i * 4);
    length = unicode_normalize(&normalized, input, text->count, UNICODE_NFD,
                               NULL, normalize_realloc);
    free(input);
    free(normalized);
    if (length < 0)
        return -1;
    audit->texts++;
    return 0;
}

static int ranges(const Scalars *text, int starred, Audit *audit)
{
    uint32_t i, first, last, code;
    if ((text->count & 1) || (starred && !text->count))
        return -1;
    for (i = 0; i < text->count; i += 2) {
        first = load32(text->data + (size_t)i * 4);
        last = load32(text->data + (size_t)(i + 1) * 4);
        if (first > last || (first <= 0xdfff && last >= 0xd800))
            return -1;
        if (!starred)
            continue;
        if (last - first + 1 > IR_MAX_EXPANDED - audit->program_starred)
            return -1;
        audit->program_starred += last - first + 1;
        if (last - first + 1 > IR_MAX_TOTAL_EXPANDED - audit->starred)
            return -1;
        audit->starred += last - first + 1;
        if (audit->preflight)
            continue;
        /* Starred ranges must be NFD-inert BEFORE normalization. Checking only
           endpoints misses decomposable/combining characters in the interior. */
        for (code = first; code <= last; code++) {
            uint32_t *normalized = NULL;
            int length, inert;
            if (unicode_get_cc(code))
                return -1;
            length = unicode_normalize(&normalized, &code, 1, UNICODE_NFD,
                                       NULL, normalize_realloc);
            inert = length == 1 && normalized[0] == code;
            free(normalized);
            if (!inert)
                return -1;
        }
    }
    return 0;
}

static int node(Reader *program, Audit *audit)
{
    Reader reader;
    Scalars fields[3];
    uint32_t size, kind, auxiliary, i, source_length, line, column;
    const uint8_t *start = program->at, *source;
    audit->source = NULL;
    if (word(program, &size) || size < 36 || size > (size_t)(program->end - start))
        return -1;
    reader = (Reader){program->at, start + size};
    program->at = reader.end;
    if (word(&reader, &kind) || word(&reader, &auxiliary) ||
        word(&reader, &line) || word(&reader, &column) ||
        word(&reader, &source_length) || !line || !column ||
        !source_length || label(&reader, source_length, &source))
        return -1;
    audit->source = source;
    audit->source_length = source_length;
    audit->line = line;
    audit->column = column;
    for (i = 0; i < 3; i++) {
        if (scalars(&reader, &fields[i]))
            return -1;
    }
    if (reader.at != reader.end)
        return -1;
    switch (kind) {
    case 1: /* text reset or logical reset name */
        if (auxiliary > 3 || !!fields[0].count == !!fields[1].count || fields[2].count)
            return -1;
        if (normalize_text(&fields[0], audit))
            return -1;
        break;
    case 2: /* prefix, relation string, extension */
        if (auxiliary > 4 || !fields[1].count)
            return -1;
        /* Prefix and mapped text will normalize separately. Check their
           ORIGINAL starts before normalization so context cannot cross the
           split. Extension strings are independent lookups, not prefixes. */
        if (fields[0].count &&
            (!unicode_nfc_boundary_before(load32(fields[0].data)) ||
             !unicode_nfc_boundary_before(load32(fields[1].data))))
            return -1;
        for (i = 0; i < 3; i++) {
            if (normalize_text(&fields[i], audit))
                return -1;
        }
        break;
    case 3: /* ordered starred ranges */
    case 5: /* suppression set */
    case 6: /* optimization hint set */
        if (auxiliary > (kind == 3 ? 4 : 0) || fields[1].count || fields[2].count ||
            ranges(&fields[0], kind == 3, audit))
            return -1;
        break;
    case 4: /* name and NUL-separated values, interpreted by semantic compilation */
        if (auxiliary || !fields[0].count || !fields[1].count || fields[2].count ||
            load32(fields[1].data + (size_t)(fields[1].count - 1) * 4))
            return -1;
        break;
    default:
        return -1;
    }
    audit->instructions++;
    return 0;
}

static int audit_ir(const uint8_t *bytes, size_t length, Audit *audit)
{
    Reader reader = {bytes, bytes + length};
    uint32_t version, unicode_version, size, count, i;
    if (length < 20 || length > IR_MAX_SIZE || memcmp(bytes, "QJCI", 4))
        return -1;
    reader.at += 4;
    if (word(&reader, &version) || version != 1 ||
        word(&reader, &unicode_version) || unicode_version != IR_UNICODE_VERSION ||
        word(&reader, &size) || size != length || word(&reader, &count) ||
        count > (size_t)(reader.end - reader.at) / 20)
        return -1;
    for (i = 0; i < count; i++) {
        Reader program;
        uint32_t locale, type, alternate, operations, j;
        const uint8_t *ignored, *start = reader.at;
        audit->source = NULL;
        if (word(&reader, &size) || size < 20 || size > (size_t)(reader.end - start))
            return -1;
        program = (Reader){reader.at, start + size};
        reader.at = program.end;
        if (word(&program, &locale) || !locale || word(&program, &type) || !type ||
            word(&program, &alternate) || word(&program, &operations) ||
            label(&program, locale, &ignored) || label(&program, type, &ignored) ||
            label(&program, alternate, &ignored) ||
            operations > (size_t)(program.end - program.at) / 36)
            return -1;
        audit->program_starred = 0;
        for (j = 0; j < operations; j++) {
            if (node(&program, audit))
                return -1;
        }
        if (program.at != program.end)
            return -1;
        audit->programs++;
    }
    return reader.at == reader.end ? 0 : -1;
}

int main(int argc, char **argv)
{
    FILE *file;
    uint8_t *bytes;
    long size;
    Audit audit = {.preflight = 1};
    int failed;
    if (argc != 3 || strcmp(argv[1], "--audit-ir")) {
        fprintf(stderr, "usage: %s --audit-ir input.qjci\n", argv[0]);
        return 2;
    }
    file = fopen(argv[2], "rb");
    if (!file)
        return 1;
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 20 || size > IR_MAX_SIZE ||
        fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return 1;
    }
    bytes = malloc((size_t)size);
    if (!bytes) {
        fclose(file);
        return 1;
    }
    failed = fread(bytes, 1, (size_t)size, file) != (size_t)size;
    if (fclose(file))
        failed = 1;
    /* Preflight the whole artifact before any allocating normalization. A
       compact repeated range must not evade the budget by changing programs. */
    if (!failed)
        failed = audit_ir(bytes, (size_t)size, &audit);
    if (!failed) {
        audit = (Audit){0};
        failed = audit_ir(bytes, (size_t)size, &audit);
    }
    if (failed) {
        if (audit.source)
            fprintf(stderr, "%.*s:%u:%u: ", (int)audit.source_length, (const char *)audit.source,
                    audit.line, audit.column);
        fputs("invalid tailoring IR or normalization constraint\n", stderr);
    } else {
        printf("{\"programs\":%" PRIu64 ",\"instructions\":%" PRIu64
               ",\"normalized_texts\":%" PRIu64 ",\"starred_scalars\":%" PRIu64 "}\n",
               audit.programs, audit.instructions, audit.texts, audit.starred);
    }
    free(bytes);
    return failed ? 1 : 0;
}
