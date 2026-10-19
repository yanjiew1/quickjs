/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "pack.h"
#include "wire.h"
#include <string.h>

enum { HEADER_SIZE = 64, DIRECTORY_SIZE = 16, MAX_SECTIONS = 128 };

QJSIntlStatus qjs_intl_pack_open(const void *bytes, size_t length,
                               QJSIntlPack *out)
{
    const uint8_t *data = bytes;
    uint32_t count, index, previous = 0;
    size_t offset;

    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!bytes)
        return QJS_INTL_INVALID_ARGUMENT;
    if (length < HEADER_SIZE || length > 256 * 1024 * 1024 ||
        memcmp(data, "QJIN", 4) || qjs_intl_read_u32(data + 8) != length ||
        qjs_intl_read_u32(data + 48) > 1 || qjs_intl_read_u32(data + 52) ||
        qjs_intl_read_u32(data + 56) || qjs_intl_read_u32(data + 60))
        return QJS_INTL_DATA_ERROR;
    if (qjs_intl_read_u32(data + 4) != 1) /* little-endian u16 major=1, minor=0 */
        return QJS_INTL_UNSUPPORTED;
    count = qjs_intl_read_u32(data + 12);
    if (!count || count > MAX_SECTIONS)
        return QJS_INTL_DATA_ERROR;
    offset = HEADER_SIZE + (size_t)count * DIRECTORY_SIZE;
    if (offset > length)
        return QJS_INTL_DATA_ERROR;
    for (index = 0; index < count; index++) {
        const uint8_t *entry = data + HEADER_SIZE + index * DIRECTORY_SIZE;
        uint32_t identity = qjs_intl_read_u32(entry);
        size_t start = qjs_intl_read_u32(entry + 4), size = qjs_intl_read_u32(entry + 8);
        size_t aligned = (offset + 3) & ~(size_t)3;

        if (identity <= previous || start != aligned || start > length ||
            size > length - start)
            return QJS_INTL_DATA_ERROR;
        while (offset < start) {
            if (data[offset++])
                return QJS_INTL_DATA_ERROR;
        }
        previous = identity;
        offset = start + size;
    }
    if (offset != length)
        return QJS_INTL_DATA_ERROR;
    out->bytes = data;
    out->length = length;
    out->generation = data + 16;
    out->section_count = count;
    out->profile = qjs_intl_read_u32(data + 48);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_pack_section(const QJSIntlPack *pack, uint32_t identity,
                                  QJSIntlSection *out)
{
    uint32_t low = 0, high;

    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!pack || !pack->bytes || !identity)
        return QJS_INTL_INVALID_ARGUMENT;
    high = pack->section_count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t *entry = pack->bytes + HEADER_SIZE + middle * DIRECTORY_SIZE;
        uint32_t found = qjs_intl_read_u32(entry);

        if (found < identity) {
            low = middle + 1;
        } else if (found > identity) {
            high = middle;
        } else {
            out->bytes = pack->bytes + qjs_intl_read_u32(entry + 4);
            out->length = qjs_intl_read_u32(entry + 8);
            out->count = qjs_intl_read_u32(entry + 12);
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}
