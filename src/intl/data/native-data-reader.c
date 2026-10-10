/* Portable bounded native Intl binary reader. See metadata-data.h for bytes.
 * No allocation, global mutable state, unaligned casts, or ICU dependency.
 */
#include "native-data-reader.h"
#include "plural-data-validation.h"
#include "relative-data-validation.h"
#include "number-extra-validation.h"
#include "duration-data-validation.h"
#include "date-data-validation.h"
#include "intl/collator-native-data.h"
#include <string.h>

static uint16_t read_u16(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static uint32_t read_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int range_ok(size_t total, size_t offset, size_t length)
{
    return offset <= total && length <= total - offset;
}

static int all_zero(const unsigned char *p, size_t length)
{
    size_t i;
    for (i = 0; i < length; i++) {
        if (p[i])
            return 0;
    }
    return 1;
}

/* NUL, overlong sequences, surrogate encodings and >U+10FFFF are invalid.
 * The tests include each restriction and truncations of multibyte sequences.
 */
static int utf8_valid(const unsigned char *p, size_t length)
{
    size_t i = 0;
    while (i < length) {
        unsigned char c = p[i++];
        size_t n;
        uint32_t cp, minimum;
        if (c > 0 && c < 0x80)
            continue;
        if (c >= 0xc2 && c <= 0xdf) {
            n = 1; cp = (uint32_t)(c & 0x1f); minimum = 0x80;
        } else if (c >= 0xe0 && c <= 0xef) {
            n = 2; cp = (uint32_t)(c & 0x0f); minimum = 0x800;
        } else if (c >= 0xf0 && c <= 0xf4) {
            n = 3; cp = (uint32_t)(c & 7); minimum = 0x10000;
        } else {
            return 0;
        }
        if (n > length - i)
            return 0;
        while (n--) {
            c = p[i++];
            if ((c & 0xc0) != 0x80)
                return 0;
            cp = (cp << 6) | (uint32_t)(c & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff ||
            (cp >= 0xd800 && cp <= 0xdfff))
            return 0;
    }
    return 1;
}

static int ascii_nonempty(QJSIntlDataSlice s)
{
    size_t i;
    if (!s.length)
        return 0;
    for (i = 0; i < s.length; i++) {
        if (s.data[i] < 0x21 || s.data[i] > 0x7e)
            return 0;
    }
    return 1;
}

static int lower_alnum(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static int ascii_text(QJSIntlDataSlice s)
{
    size_t i;
    for (i = 0; i < s.length; i++) {
        if (s.data[i] < 0x20 || s.data[i] > 0x7e)
            return 0;
    }
    return 1;
}

static int slice_compare(QJSIntlDataSlice a, QJSIntlDataSlice b)
{
    size_t n = a.length < b.length ? a.length : b.length;
    int result = n ? memcmp(a.data, b.data, n) : 0;
    if (result)
        return result;
    return a.length < b.length ? -1 : a.length > b.length ? 1 : 0;
}

static int directory_bounds(const QJSIntlDataView *view,
                            size_t *offset, uint32_t *count)
{
    uint32_t at, n;
    if (!view->data || view->length < QJS_INTL_DATA_HEADER_SIZE)
        return 0;
    at = read_u32(view->data + QJS_INTL_H_DIRECTORY_OFFSET);
    n = read_u32(view->data + QJS_INTL_H_SECTION_COUNT);
    if ((size_t)at > view->length ||
        (size_t)n > (view->length - (size_t)at) /
                    QJS_INTL_DATA_DIRECTORY_RECORD_SIZE)
        return 0;
    *offset = (size_t)at;
    *count = n;
    return 1;
}

static const unsigned char *directory_record(const QJSIntlDataView *view,
                                             size_t offset, uint32_t index)
{
    return view->data + offset +
           (size_t)index * QJS_INTL_DATA_DIRECTORY_RECORD_SIZE;
}

static int section_decode(const QJSIntlDataView *view,
                          const unsigned char *d, QJSIntlDataSection *out)
{
    uint32_t at = read_u32(d + QJS_INTL_D_OFFSET);
    uint32_t length = read_u32(d + QJS_INTL_D_BYTE_LENGTH);
    uint32_t count = read_u32(d + QJS_INTL_D_RECORD_COUNT);
    uint32_t width = read_u32(d + QJS_INTL_D_RECORD_SIZE);
    if (!width || !range_ok(view->length, at, length) ||
        count > length / width || (size_t)count * width != length)
        return 0;
    out->bytes.data = view->data + at;
    out->bytes.length = length;
    out->id = read_u32(d + QJS_INTL_D_ID);
    out->record_width = width;
    out->record_count = count;
    return 1;
}

QJSIntlDataStatus qjs_intl_data_section(const QJSIntlDataView *view,
                                       uint32_t id, QJSIntlDataSection *out)
{
    size_t at;
    uint32_t i, count;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!view || !view->data || !out)
        return QJS_INTL_DATA_INVALID_ARGUMENT;
    if (!directory_bounds(view, &at, &count))
        return QJS_INTL_DATA_INVALID;
    for (i = 0; i < count; i++) {
        const unsigned char *d = directory_record(view, at, i);
        if (read_u32(d + QJS_INTL_D_ID) == id) {
            if (!section_decode(view, d, out))
                return QJS_INTL_DATA_INVALID;
            return QJS_INTL_DATA_OK;
        }
    }
    return QJS_INTL_DATA_NOT_FOUND;
}

QJSIntlDataStatus qjs_intl_data_record(const QJSIntlDataSection *section,
                                      uint32_t index, QJSIntlDataSlice *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!section || !section->bytes.data || !out)
        return QJS_INTL_DATA_INVALID_ARGUMENT;
    if (!section->record_width ||
        section->record_count > section->bytes.length / section->record_width ||
        (size_t)section->record_count * section->record_width !=
        section->bytes.length)
        return QJS_INTL_DATA_INVALID;
    if (index >= section->record_count)
        return QJS_INTL_DATA_NOT_FOUND;
    out->data = section->bytes.data + (size_t)index * section->record_width;
    out->length = section->record_width;
    return QJS_INTL_DATA_OK;
}

QJSIntlDataStatus qjs_intl_data_record_u32(const QJSIntlDataSection *section,
                                          uint32_t index, uint32_t field_offset,
                                          uint32_t *out)
{
    QJSIntlDataSlice record;
    QJSIntlDataStatus status;
    if (out)
        *out = 0;
    if (!out)
        return QJS_INTL_DATA_INVALID_ARGUMENT;
    status = qjs_intl_data_record(section, index, &record);
    if (status != QJS_INTL_DATA_OK)
        return status;
    if (!range_ok(record.length, field_offset, 4))
        return QJS_INTL_DATA_INVALID_ARGUMENT;
    *out = read_u32(record.data + field_offset);
    return QJS_INTL_DATA_OK;
}

static int pool_string(QJSIntlDataSlice pool, uint32_t offset, uint32_t length,
                       QJSIntlDataSlice *out)
{
    if (!length) {
        if (offset || !pool.length || pool.data[0])
            return 0;
    } else {
        if (!offset || !range_ok(pool.length, offset, length) ||
            (size_t)length == pool.length - offset ||
            pool.data[offset - 1] || pool.data[(size_t)offset + length] ||
            !utf8_valid(pool.data + offset, length))
            return 0;
    }
    out->data = pool.data + offset;
    out->length = length;
    return 1;
}

QJSIntlDataStatus qjs_intl_data_string(const QJSIntlDataView *view,
                                      uint32_t offset, uint32_t length,
                                      QJSIntlDataSlice *out)
{
    QJSIntlDataSection pool;
    QJSIntlDataStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out)
        return QJS_INTL_DATA_INVALID_ARGUMENT;
    status = qjs_intl_data_section(view, QJS_INTL_DATA_UTF8_POOL, &pool);
    if (status != QJS_INTL_DATA_OK)
        return status;
    if (!pool_string(pool.bytes, offset, length, out))
        return QJS_INTL_DATA_INVALID;
    return QJS_INTL_DATA_OK;
}

QJSIntlDataStatus qjs_intl_data_record_string(const QJSIntlDataView *view,
                                             const QJSIntlDataSection *section,
                                             uint32_t index,
                                             uint32_t field_offset,
                                             QJSIntlDataSlice *out)
{
    QJSIntlDataSlice record;
    QJSIntlDataSection owned;
    QJSIntlDataStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!view || !view->data || !section || !out)
        return QJS_INTL_DATA_INVALID_ARGUMENT;
    status = qjs_intl_data_section(view, section->id, &owned);
    if (status != QJS_INTL_DATA_OK)
        return status;
    if (owned.bytes.data != section->bytes.data ||
        owned.bytes.length != section->bytes.length ||
        owned.record_width != section->record_width ||
        owned.record_count != section->record_count)
        return QJS_INTL_DATA_INVALID_ARGUMENT;
    status = qjs_intl_data_record(section, index, &record);
    if (status != QJS_INTL_DATA_OK)
        return status;
    if (!range_ok(record.length, field_offset, 8))
        return QJS_INTL_DATA_INVALID_ARGUMENT;
    return qjs_intl_data_string(view, read_u32(record.data + field_offset),
                                read_u32(record.data + field_offset + 4), out);
}

static uint32_t section_width(uint32_t id)
{
    switch (id) {
    case QJS_INTL_DATA_UTF8_POOL: return 1;
    case QJS_INTL_DATA_INPUT_SHA256: return 32;
    case QJS_INTL_DATA_ALIAS: return 20;
    case QJS_INTL_DATA_REPLACEMENT_LIST: return 8;
    case QJS_INTL_DATA_LIKELY: return 16;
    case QJS_INTL_DATA_BCP47: return 48;
    case QJS_INTL_DATA_WEEK: return 12;
    case QJS_INTL_DATA_PREFERENCE: return 36;
    case QJS_INTL_DATA_LOCALE: return 48;
    case QJS_INTL_DATA_NUMBERING: return 52;
    case QJS_INTL_DATA_LIST: return 8;
    case QJS_INTL_DATA_COMPONENT_PARENT: return 20;
    case QJS_INTL_DATA_REGION_TIME_ZONE: return 16;
    case QJS_INTL_DATA_LOCALE_AUX: return 44;
    case QJS_INTL_DATA_NUMBERING_RULE: return 12;
    case QJS_INTL_DATA_HOUR_DEFAULT: return 20;
    case QJS_INTL_DATA_SCRIPT_DIRECTION: return 12;
    case QJS_INTL_DATA_AVAILABLE_CALENDAR: return 8;
    case QJS_INTL_DATA_LOCALE_SERVICE_INFO: return 28;
    case QJS_INTL_DATA_PLURAL_LOCALE: return 24;
    case QJS_INTL_DATA_PLURAL_RULE: return 12;
    case QJS_INTL_DATA_PLURAL_RANGE: return 4;
    case QJS_INTL_DATA_LIST_PATTERN: return 40;
    case QJS_INTL_DATA_LIST_HEBREW_SCRIPT: return 8;
    case QJS_INTL_DATA_DISPLAY_LABEL: return 28;
    case QJS_INTL_DATA_DISPLAY_PATTERN: return 16;
    case QJS_INTL_DATA_SEGMENT_GCB:
    case QJS_INTL_DATA_SEGMENT_WB:
    case QJS_INTL_DATA_SEGMENT_SB:
    case QJS_INTL_DATA_SEGMENT_INCB:
    case QJS_INTL_DATA_SEGMENT_EP: return 12;
    case QJS_INTL_DATA_SEGMENT_RULE: return 24;
    case QJS_INTL_DATA_RELATIVE_PATTERN: return 16;
    case QJS_INTL_DATA_RELATIVE_LITERAL: return 20;
    case QJS_INTL_DATA_NUMBER_SYMBOL: return 96;
    case QJS_INTL_DATA_NUMBER_PATTERN: return 40;
    case QJS_INTL_DATA_NUMBER_CURRENCY: return 76;
    case QJS_INTL_DATA_NUMBER_UNIT: return 64;
    case QJS_INTL_DATA_NUMBER_CURRENCY_NAME: return 56;
    case QJS_INTL_DATA_NUMBER_CURRENCY_DIGITS: return 12;
    case QJS_INTL_DATA_NUMBER_MISC: return 24;
    case QJS_INTL_DATA_NUMBER_UNICODE_CLASS: return 12;
    case QJS_INTL_DATA_NUMBER_CURRENCY_SPACING: return 24;
    case QJS_INTL_DATA_COLLATION_NODE: return 20;
    case QJS_INTL_DATA_COLLATION_CE: return 16;
    case QJS_INTL_DATA_COLLATION_IMPLICIT: return 16;
    case QJS_INTL_DATA_COLLATION_DIGIT: return 4;
    case QJS_INTL_DATA_COLLATION_CAPABILITY: return 20;
    case QJS_INTL_DATA_COLLATION_CONFIG: return 16;
    case QJS_INTL_DATA_DATE_PATTERN: return 32;
    case QJS_INTL_DATA_DATE_NAME: return 28;
    case QJS_INTL_DATA_DATE_SYMBOL: return 48;
    case QJS_INTL_DATA_DATE_PERIOD_RULE: return 16;
    case QJS_INTL_DATA_DATE_ZONE_NAME: return 72;
    case QJS_INTL_DATA_DATE_META_PERIOD: return 32;
    case QJS_INTL_DATA_DATE_ZONE_ALIAS: return 16;
    case QJS_INTL_DATA_DATE_RANGE_FALLBACK: return 20;
    case QJS_INTL_DATA_DURATION_CLOCK: return 32;
    case QJS_INTL_DATA_NUMBER_COMPACT: return 76;
    case QJS_INTL_DATA_NUMBER_DENOMINATOR: return 32;
    case QJS_INTL_DATA_NUMBER_COMPOUND_PER: return 16;
    default: return 0;
    }
}

static int validate_layout(const QJSIntlDataView *view)
{
    size_t at, directory_end, cursor;
    uint32_t i, j, count, previous_id = 0, required = 0;
    const unsigned char *p = view->data;
    if (view->length < QJS_INTL_DATA_HEADER_SIZE ||
        view->length > UINT32_MAX ||
        memcmp(p, QJS_INTL_DATA_MAGIC, 8) ||
        read_u32(p + QJS_INTL_H_HEADER_SIZE) != QJS_INTL_DATA_HEADER_SIZE ||
        read_u32(p + QJS_INTL_H_TOTAL_SIZE) != view->length ||
        read_u32(p + QJS_INTL_H_DIRECTORY_OFFSET) != QJS_INTL_DATA_HEADER_SIZE ||
        read_u32(p + QJS_INTL_H_DIRECTORY_RECORD_SIZE) !=
        QJS_INTL_DATA_DIRECTORY_RECORD_SIZE ||
        read_u32(p + QJS_INTL_H_FLAGS) ||
        !all_zero(p + QJS_INTL_H_RESERVED, 16) ||
        !directory_bounds(view, &at, &count))
        return 0;
    directory_end = at + (size_t)count * QJS_INTL_DATA_DIRECTORY_RECORD_SIZE;
    for (i = 0; i < count; i++) {
        const unsigned char *d = directory_record(view, at, i);
        QJSIntlDataSection s;
        uint32_t offset = read_u32(d + QJS_INTL_D_OFFSET);
        uint32_t id = read_u32(d + QJS_INTL_D_ID);
        if (id <= previous_id ||
            (!read_u16(p + QJS_INTL_H_SCHEMA_MINOR) && id > QJS_INTL_DATA_REGION_TIME_ZONE) ||
            (read_u16(p + QJS_INTL_H_SCHEMA_MINOR) < 3 &&
             ((id >= QJS_INTL_DATA_SCRIPT_DIRECTION && id <= QJS_INTL_DATA_LOCALE_SERVICE_INFO) ||
              (id >= QJS_INTL_DATA_PLURAL_LOCALE && id <= QJS_INTL_DATA_PLURAL_RANGE) ||
              id >= QJS_INTL_DATA_DISPLAY_LABEL)) ||
            (read_u16(p + QJS_INTL_H_SCHEMA_MINOR) < 2 &&
             id >= QJS_INTL_DATA_LIST_PATTERN) ||
            !section_decode(view, d, &s) ||
            s.record_width != section_width(id) ||
            read_u32(d + QJS_INTL_D_FLAGS) || (offset & 3) ||
            (size_t)offset < directory_end)
            return 0;
        if (id >= QJS_INTL_DATA_DATE_PATTERN &&
            (read_u32(p + QJS_INTL_H_UNICODE_VERSION) != (18u << 16) ||
             read_u32(p + QJS_INTL_H_CLDR_VERSION) != (49u << 16)))
            return 0;
        previous_id = id;
        switch (id) {
        case QJS_INTL_DATA_UTF8_POOL:
            if (!s.record_count)
                return 0;
            required |= 1u; break;
        case QJS_INTL_DATA_INPUT_SHA256:
            if (s.record_count != 1)
                return 0;
            required |= 2u; break;
        case QJS_INTL_DATA_ALIAS: required |= 4u; break;
        case QJS_INTL_DATA_REPLACEMENT_LIST: required |= 8u; break;
        case QJS_INTL_DATA_LIKELY: required |= 16u; break;
        case QJS_INTL_DATA_BCP47: required |= 32u; break;
        case QJS_INTL_DATA_COLLATION_NODE:
        case QJS_INTL_DATA_COLLATION_CE:
        case QJS_INTL_DATA_COLLATION_IMPLICIT:
        case QJS_INTL_DATA_COLLATION_DIGIT:
        case QJS_INTL_DATA_COLLATION_CAPABILITY:
        case QJS_INTL_DATA_COLLATION_CONFIG:
            if (read_u16(p + QJS_INTL_H_SCHEMA_MINOR) != 3 ||
                read_u32(p + QJS_INTL_H_UNICODE_VERSION) != (18u << 16) ||
                read_u32(p + QJS_INTL_H_CLDR_VERSION) != (49u << 16) ||
                read_u32(p + QJS_INTL_H_UCA_VERSION) != (18u << 16))
                return 0;
            break;
        case QJS_INTL_DATA_SCRIPT_DIRECTION:
            if (read_u32(p + QJS_INTL_H_UNICODE_VERSION) != (18u << 16) ||
                read_u32(p + QJS_INTL_H_CLDR_VERSION) != (49u << 16))
                return 0;
            break;
        case QJS_INTL_DATA_LIST_PATTERN: {
            QJSIntlDataSection locales;
            if (read_u32(p + QJS_INTL_H_UNICODE_VERSION) != (18u << 16) ||
                read_u32(p + QJS_INTL_H_CLDR_VERSION) != (49u << 16) ||
                qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) !=
                QJS_INTL_DATA_OK)
                return 0;
            break;
        }
        case QJS_INTL_DATA_LIST_HEBREW_SCRIPT:
            if (read_u32(p + QJS_INTL_H_UNICODE_VERSION) != (18u << 16))
                return 0;
            break;
        default: break;
        }
        /* Empty sections reserve no bytes and may share a payload position. */
        if (!s.bytes.length)
            continue;
        for (j = 0; j < i; j++) {
            const unsigned char *other = directory_record(view, at, j);
            size_t other_at = read_u32(other + QJS_INTL_D_OFFSET);
            size_t other_length = read_u32(other + QJS_INTL_D_BYTE_LENGTH);
            if (other_length && (size_t)offset < other_at + other_length &&
                other_at < (size_t)offset + s.bytes.length)
                return 0;
        }
    }
    if (required != 63u)
        return 0;
    /* Walk payload order independently of ID order, checking only padding.
     * No sort workspace or bytewise O(blob_size * section_count) scan.
     */
    cursor = directory_end;
    for (;;) {
        size_t next = view->length, next_length = 0;
        for (i = 0; i < count; i++) {
            const unsigned char *d = directory_record(view, at, i);
            size_t start = read_u32(d + QJS_INTL_D_OFFSET);
            size_t length = read_u32(d + QJS_INTL_D_BYTE_LENGTH);
            if (length && start >= cursor && start < next) {
                next = start;
                next_length = length;
            }
        }
        if (!all_zero(view->data + cursor, next - cursor))
            return 0;
        if (!next_length)
            break;
        cursor = next + next_length;
    }
    return 1;
}

static int validate_pool(QJSIntlDataSlice pool)
{
    size_t at = 0;
    if (!pool.length || pool.data[0] || pool.data[pool.length - 1])
        return 0;
    while (at < pool.length) {
        size_t end = at;
        while (end < pool.length && pool.data[end])
            end++;
        if (!utf8_valid(pool.data + at, end - at))
            return 0;
        at = end + 1;
    }
    return 1;
}

static int record_string(QJSIntlDataSlice pool, const unsigned char *record,
                         size_t field, int optional, int text,
                         QJSIntlDataSlice *out)
{
    if (!pool_string(pool, read_u32(record + field),
                     read_u32(record + field + 4), out))
        return 0;
    if (!out->length)
        return optional;
    return text ? ascii_text(*out) : ascii_nonempty(*out);
}

static int span_valid(const QJSIntlDataView *view, uint32_t target_id,
                      const unsigned char *span, int require_nonempty)
{
    QJSIntlDataSection target;
    uint32_t first = read_u32(span), count = read_u32(span + 4);
    if (!count)
        return !first && !require_nonempty;
    if (qjs_intl_data_section(view, target_id, &target) != QJS_INTL_DATA_OK)
        return 0;
    return first <= target.record_count && count <= target.record_count - first;
}

static int index_valid(uint32_t index, uint32_t count, int optional)
{
    return index < count || (optional && index == QJS_INTL_DATA_INDEX_NONE);
}

/* Compare precisely the wire schema's composite key; list order is semantic. */
static int key_compare(uint32_t id, QJSIntlDataSlice pool,
                       const unsigned char *a, const unsigned char *b)
{
    size_t fields[4], n = 0, i;
    uint32_t av, bv;
    if (id == QJS_INTL_DATA_DISPLAY_LABEL || id == QJS_INTL_DATA_DISPLAY_PATTERN) {
        size_t last = id == QJS_INTL_DATA_DISPLAY_LABEL ? 8 : 4;
        for (i = 0; i <= last; i += 4) {
            av = read_u32(a + i); bv = read_u32(b + i);
            if (av != bv) return av < bv ? -1 : 1;
        }
        if (id == QJS_INTL_DATA_DISPLAY_LABEL) {
            QJSIntlDataSlice as, bs;
            if (!pool_string(pool, read_u32(a + 12), read_u32(a + 16), &as) ||
                !pool_string(pool, read_u32(b + 12), read_u32(b + 16), &bs))
                return 0;
            return slice_compare(as, bs);
        }
        return 0;
    }
    if (id == QJS_INTL_DATA_LOCALE_SERVICE_INFO) {
        av = read_u32(a + 4); bv = read_u32(b + 4);
        if (av != bv) return av < bv ? -1 : 1;
        av = read_u32(a); bv = read_u32(b);
        return av < bv ? -1 : av > bv ? 1 : 0;
    }
    if (id == QJS_INTL_DATA_LIST_PATTERN) {
        av = read_u32(a); bv = read_u32(b);
        if (av != bv)
            return av < bv ? -1 : 1;
        for (i = 4; i <= 6; i++) {
            if (a[i] != b[i])
                return a[i] < b[i] ? -1 : 1;
        }
        return 0;
    }
    if (id == QJS_INTL_DATA_LOCALE_AUX || id == QJS_INTL_DATA_NUMBERING_RULE ||
        id == QJS_INTL_DATA_LIST_HEBREW_SCRIPT) {
        av = read_u32(a); bv = read_u32(b);
        return av < bv ? -1 : av > bv ? 1 : 0;
    }
    if (id == QJS_INTL_DATA_ALIAS || id == QJS_INTL_DATA_PREFERENCE ||
        id == QJS_INTL_DATA_HOUR_DEFAULT) {
        size_t at = id == QJS_INTL_DATA_ALIAS ? 0 : 8;
        av = read_u32(a + at); bv = read_u32(b + at);
        if (av != bv)
            return av < bv ? -1 : 1;
        fields[n++] = id == QJS_INTL_DATA_ALIAS ? 4 : 0;
    } else if (id == QJS_INTL_DATA_BCP47) {
        fields[n++] = 0; fields[n++] = 8;
        fields[n++] = 16; fields[n++] = 24;
    } else {
        fields[n++] = 0;
    }
    for (i = 0; i < n; i++) {
        QJSIntlDataSlice as, bs;
        int result;
        size_t at = fields[i];
        if (!pool_string(pool, read_u32(a + at), read_u32(a + at + 4), &as) ||
            !pool_string(pool, read_u32(b + at), read_u32(b + at + 4), &bs))
            return 0;
        result = slice_compare(as, bs);
        if (result)
            return result;
    }
    if (id == QJS_INTL_DATA_COMPONENT_PARENT) {
        av = read_u32(a + 8); bv = read_u32(b + 8);
        return av < bv ? -1 : av > bv ? 1 : 0;
    }
    return 0;
}

static int scalar_valid(uint32_t cp)
{
    return cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
}

static int list_string(QJSIntlDataSlice pool, const QJSIntlDataSection *list,
                       uint32_t index, QJSIntlDataSlice *out)
{
    const unsigned char *r;
    if (index >= list->record_count)
        return 0;
    r = list->bytes.data + (size_t)index * 8;
    return pool_string(pool, read_u32(r), read_u32(r + 4), out);
}

static int hour_symbol(QJSIntlDataSlice raw)
{
    if (raw.length < 1 || raw.length > 2 ||
        (raw.length == 2 && raw.data[1] != 'b' && raw.data[1] != 'B'))
        return -1;
    switch (raw.data[0]) {
    case 'H': return 2; /* h23 */
    case 'h': return 1; /* h12 */
    case 'K': return 0; /* h11 */
    case 'k': return 3; /* h24 */
    default: return -1;
    }
}

static int cycle_equals(QJSIntlDataSlice cycle, int value)
{
    static const unsigned char digits[4][2] = {
        {'1', '1'}, {'1', '2'}, {'2', '3'}, {'2', '4'}
    };
    return cycle.length == 3 && cycle.data[0] == 'h' &&
           cycle.data[1] == digits[value][0] && cycle.data[2] == digits[value][1];
}

/* Schema1.1 stores raw allowed symbols exactly; semantic cycles are their
 * ordered unique mapping. Preferred is a separate HOUR_DEFAULT record.
 */
static int validate_hour_lists(const QJSIntlDataView *view, QJSIntlDataSlice pool,
                                const unsigned char *preference)
{
    QJSIntlDataSection list;
    uint32_t raw_first = read_u32(preference + 28);
    uint32_t raw_count = read_u32(preference + 32);
    uint32_t cycle_first = read_u32(preference + 20);
    uint32_t cycle_count = read_u32(preference + 24);
    uint32_t i, mapped = 0, mask = 0;
    if (!raw_count)
        return !cycle_count;
    if (cycle_count > 4 ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LIST, &list) != QJS_INTL_DATA_OK)
        return 0;
    for (i = 0; i < raw_count; i++) {
        QJSIntlDataSlice raw, cycle;
        int value;
        uint32_t bit;
        if (!list_string(pool, &list, raw_first + i, &raw))
            return 0;
        value = hour_symbol(raw);
        if (value < 0)
            return 0;
        bit = UINT32_C(1) << (unsigned int)value;
        if (mask & bit)
            continue;
        if (mapped >= cycle_count ||
            !list_string(pool, &list, cycle_first + mapped, &cycle) ||
            !cycle_equals(cycle, value))
            return 0;
        mask |= bit;
        mapped++;
    }
    return mapped == cycle_count;
}

static int optional_type_valid(QJSIntlDataSlice type)
{
    size_t i, segment = 0;
    if (!type.length)
        return 1;
    for (i = 0; i < type.length; i++) {
        unsigned char c = type.data[i];
        if (c == '-') {
            if (segment < 3 || segment > 8)
                return 0;
            segment = 0;
        } else {
            if (!lower_alnum(c))
                return 0;
            segment++;
        }
    }
    return segment >= 3 && segment <= 8;
}

static int numbering_named(const QJSIntlDataView *view, QJSIntlDataSlice pool,
                            QJSIntlDataSlice name)
{
    QJSIntlDataSection numbering;
    uint32_t lo = 0, hi;
    if (!name.length)
        return 1; /* explicit inherit */
    if (qjs_intl_data_section(view, QJS_INTL_DATA_NUMBERING, &numbering) !=
        QJS_INTL_DATA_OK)
        return 0;
    hi = numbering.record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const unsigned char *r = numbering.bytes.data + (size_t)mid * 52;
        QJSIntlDataSlice id;
        int compare;
        if (!pool_string(pool, read_u32(r), read_u32(r + 4), &id))
            return 0;
        compare = slice_compare(id, name);
        if (compare < 0)
            lo = mid + 1;
        else if (compare > 0)
            hi = mid;
        else
            return 1;
    }
    return 0;
}

static int preferred_hour_allowed(const QJSIntlDataView *view,
                                   QJSIntlDataSlice pool, QJSIntlDataSlice key,
                                   uint32_t scope, QJSIntlDataSlice preferred)
{
    QJSIntlDataSection preferences, list;
    uint32_t i;
    if (hour_symbol(preferred) < 0 ||
        qjs_intl_data_section(view, QJS_INTL_DATA_PREFERENCE, &preferences) !=
        QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LIST, &list) != QJS_INTL_DATA_OK)
        return 0;
    for (i = 0; i < preferences.record_count; i++) {
        const unsigned char *r = preferences.bytes.data + (size_t)i * 36;
        QJSIntlDataSlice candidate;
        uint32_t j, first, count;
        if (read_u32(r + 8) != scope)
            continue;
        if (!pool_string(pool, read_u32(r), read_u32(r + 4), &candidate))
            return 0;
        if (slice_compare(candidate, key))
            continue;
        first = read_u32(r + 28); count = read_u32(r + 32);
        for (j = 0; j < count; j++) {
            if (!list_string(pool, &list, first + j, &candidate))
                return 0;
            if (!slice_compare(candidate, preferred))
                return 1;
        }
        return 0;
    }
    return 0;
}

static int pattern_string(QJSIntlDataSlice pool, const unsigned char *record,
                          size_t field, QJSIntlDataSlice *out)
{
    size_t i = 0;
    unsigned int seen = 0;
    if (!pool_string(pool, read_u32(record + field),
                     read_u32(record + field + 4), out) || !out->length)
        return 0;
    while (i < out->length) {
        unsigned char c = out->data[i];
        if (c == '{') {
            unsigned int bit;
            if (out->length - i < 3 || out->data[i + 2] != '}' ||
                (out->data[i + 1] != '0' && out->data[i + 1] != '1'))
                return 0;
            bit = 1u << (unsigned int)(out->data[i + 1] - '0');
            if (seen & bit)
                return 0;
            seen |= bit;
            i += 3;
        } else {
            if (c == '}')
                return 0;
            i++;
        }
    }
    return seen == 3u;
}

static int slice_equals_text(QJSIntlDataSlice s, const char *text)
{
    size_t length = strlen(text);
    return s.length == length && !memcmp(s.data, text, length);
}

static int locale_service_info_valid(const QJSIntlDataView *view,
                                      QJSIntlDataSlice pool, const unsigned char *r)
{
    QJSIntlDataSection locales, list;
    QJSIntlDataSlice tag, value;
    uint32_t index = read_u32(r), service = read_u32(r + 4), coverage;
    uint32_t first = read_u32(r + 16), count = read_u32(r + 20), i, j;
    const unsigned char *locale;
    if (service > 1 || read_u32(r + 24) ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK ||
        index >= locales.record_count)
        return 0;
    locale = locales.bytes.data + (size_t)index * 48;
    coverage = read_u32(locale + 44);
    if (!(coverage & (UINT32_C(1) << (service ? 3 : 1))) ||
        !record_string(pool, locale, 0, 0, 0, &tag) || slice_equals_text(tag, "root"))
        return 0;
    if (service == 1) {
        return !first && !count && record_string(pool, r, 8, 0, 0, &value) &&
               optional_type_valid(value) && numbering_named(view, pool, value);
    }
    if (!record_string(pool, r, 8, 1, 0, &value) || value.length ||
        !span_valid(view, QJS_INTL_DATA_LIST, r + 16, 0))
        return 0;
    if (!count) return 1;
    if (qjs_intl_data_section(view, QJS_INTL_DATA_LIST, &list) != QJS_INTL_DATA_OK)
        return 0;
    for (i = 0; i < count; i++) {
        if (!list_string(pool, &list, first + i, &value) || !optional_type_valid(value) ||
            !value.length || slice_equals_text(value, "standard") || slice_equals_text(value, "search"))
            return 0;
        for (j = 0; j < i; j++) {
            QJSIntlDataSlice earlier;
            if (!list_string(pool, &list, first + j, &earlier) || !slice_compare(value, earlier))
                return 0;
        }
    }
    return 1;
}

/* Display codes follow the sealed owner contract: bytes1..127, nonempty.
 * Pool validation already excludes embedded NUL and invalid scalar UTF8. */
static int display_code_valid(QJSIntlDataSlice code)
{
    size_t i;
    if (!code.length) return 0;
    for (i = 0; i < code.length; i++)
        if (code.data[i] > 127 || !code.data[i]) return 0;
    return 1;
}

static int segment_range_valid(uint32_t id, const unsigned char *r,
                                const unsigned char *previous)
{
    static const uint32_t maxima[5] = {14, 19, 15, 4, 2};
    uint32_t first = read_u32(r), last = read_u32(r + 4);
    uint32_t value = read_u32(r + 8);
    if (first > last || !scalar_valid(first) || !scalar_valid(last) ||
        (first <= 0xdfff && last >= 0xd800) || !value ||
        value >= maxima[id - QJS_INTL_DATA_SEGMENT_GCB])
        return 0;
    if (previous) {
        uint32_t end = read_u32(previous + 4);
        if (first <= end ||
            (first == end + 1 && value == read_u32(previous + 8)))
            return 0;
    }
    return 1;
}

static int validate_records(const QJSIntlDataView *view, QJSIntlDataSlice pool,
                            const QJSIntlDataSection *section,
                            uint32_t locale_count)
{
    uint32_t i;
    const unsigned char *previous = NULL;
    for (i = 0; i < section->record_count; i++) {
        const unsigned char *r = section->bytes.data +
                                 (size_t)i * section->record_width;
        QJSIntlDataSlice a, b, c;
        int sorted = 1;
        switch (section->id) {
        case QJS_INTL_DATA_UTF8_POOL:
        case QJS_INTL_DATA_INPUT_SHA256:
        case QJS_INTL_DATA_PLURAL_LOCALE:
        case QJS_INTL_DATA_COLLATION_NODE:
        case QJS_INTL_DATA_COLLATION_CE:
        case QJS_INTL_DATA_COLLATION_IMPLICIT:
        case QJS_INTL_DATA_COLLATION_DIGIT:
        case QJS_INTL_DATA_COLLATION_CAPABILITY:
        case QJS_INTL_DATA_COLLATION_CONFIG:
        case QJS_INTL_DATA_PLURAL_RULE:
        case QJS_INTL_DATA_PLURAL_RANGE:
            /* Extension owners validate their whole optional groups below, including
             * every pooled relation and the adopted evaluator grammar. */
            return 1;
        case QJS_INTL_DATA_RELATIVE_PATTERN:
        case QJS_INTL_DATA_RELATIVE_LITERAL:
            /* Owner texts are scalar UTF8, including spaces and bidi marks.
             * Check canonical bounded references here; owner gates below
             * enforce nonemptiness and service-specific template grammar. */
            {
                uint32_t field = section->id == QJS_INTL_DATA_RELATIVE_PATTERN ? 8 : 12;
                if (!pool_string(pool, read_u32(r + field),
                                 read_u32(r + field + 4), &a)) return 0;
            }
            sorted = 0; /* owner checks the lattice and signed literal keys */
            break;
        case QJS_INTL_DATA_NUMBER_SYMBOL:
        case QJS_INTL_DATA_NUMBER_PATTERN:
        case QJS_INTL_DATA_NUMBER_CURRENCY:
        case QJS_INTL_DATA_NUMBER_UNIT:
        case QJS_INTL_DATA_NUMBER_CURRENCY_NAME:
        case QJS_INTL_DATA_NUMBER_CURRENCY_DIGITS:
        case QJS_INTL_DATA_NUMBER_MISC:
        case QJS_INTL_DATA_NUMBER_UNICODE_CLASS:
        case QJS_INTL_DATA_NUMBER_CURRENCY_SPACING: {
            uint32_t first = 0, refs = 0, ref;
            switch (section->id) {
            case QJS_INTL_DATA_NUMBER_SYMBOL: first = 16; refs = 10; break;
            case QJS_INTL_DATA_NUMBER_PATTERN: first = 16; refs = 3; break;
            case QJS_INTL_DATA_NUMBER_CURRENCY: first = 4; refs = 9; break;
            case QJS_INTL_DATA_NUMBER_UNIT:
                if (!pool_string(pool, read_u32(r + 4), read_u32(r + 8), &a)) return 0;
                first = 16; refs = 6; break;
            case QJS_INTL_DATA_NUMBER_CURRENCY_NAME: first = 8; refs = 6; break;
            case QJS_INTL_DATA_NUMBER_CURRENCY_DIGITS: refs = 1; break;
            case QJS_INTL_DATA_NUMBER_MISC:
            case QJS_INTL_DATA_NUMBER_CURRENCY_SPACING: first = 8; refs = 2; break;
            default: break;
            }
            /* Exact UTF8 text semantics belong to the Number owner.
             * ascii_nonempty would reject valid spaces/non-ASCII literals. */
            for (ref = 0; ref < refs; ref++) {
                uint32_t field = first + ref * 8;
                if (!pool_string(pool, read_u32(r + field),
                                 read_u32(r + field + 4), &a)) return 0;
            }
            sorted = 0; /* owner validates keys, grammar and cross-table pairs */
            break;
        }
        case QJS_INTL_DATA_DATE_PATTERN:
        case QJS_INTL_DATA_DATE_NAME:
        case QJS_INTL_DATA_DATE_SYMBOL:
        case QJS_INTL_DATA_DATE_PERIOD_RULE:
        case QJS_INTL_DATA_DATE_ZONE_NAME:
        case QJS_INTL_DATA_DATE_META_PERIOD:
        case QJS_INTL_DATA_DATE_ZONE_ALIAS:
        case QJS_INTL_DATA_DATE_RANGE_FALLBACK:
        case QJS_INTL_DATA_DURATION_CLOCK:
        case QJS_INTL_DATA_NUMBER_COMPACT:
        case QJS_INTL_DATA_NUMBER_DENOMINATOR:
        case QJS_INTL_DATA_NUMBER_COMPOUND_PER: {
            uint32_t offsets[8], refs = 0, ref;
            switch (section->id) {
            case QJS_INTL_DATA_DATE_PATTERN:
                offsets[refs++] = 4; offsets[refs++] = 16; offsets[refs++] = 24; break;
            case QJS_INTL_DATA_DATE_NAME:
                offsets[refs++] = 4; offsets[refs++] = 20; break;
            case QJS_INTL_DATA_DATE_SYMBOL:
                for (ref = 0; ref < 5; ref++) offsets[refs++] = 8 + ref * 8;
                break;
            case QJS_INTL_DATA_DATE_ZONE_NAME:
                for (ref = 0; ref < 8; ref++) offsets[refs++] = 8 + ref * 8;
                break;
            case QJS_INTL_DATA_DATE_META_PERIOD:
            case QJS_INTL_DATA_DATE_ZONE_ALIAS:
                offsets[refs++] = 0; offsets[refs++] = 8; break;
            case QJS_INTL_DATA_DATE_RANGE_FALLBACK:
                offsets[refs++] = 4; offsets[refs++] = 12; break;
            case QJS_INTL_DATA_DURATION_CLOCK:
                offsets[refs++] = 16; offsets[refs++] = 24; break;
            case QJS_INTL_DATA_NUMBER_COMPACT:
                for (ref = 0; ref < 7; ref++) offsets[refs++] = 20 + ref * 8;
                break;
            case QJS_INTL_DATA_NUMBER_DENOMINATOR:
                offsets[refs++] = 4; offsets[refs++] = 16; offsets[refs++] = 24; break;
            case QJS_INTL_DATA_NUMBER_COMPOUND_PER:
                offsets[refs++] = 8; break;
            default: break;
            }
            for (ref = 0; ref < refs; ref++) {
                uint32_t field = offsets[ref];
                if (!pool_string(pool, read_u32(r + field),
                                 read_u32(r + field + 4), &a)) return 0;
            }
            sorted = 0; /* Exact owner validators enforce keys and semantics. */
            break;
        }
        case QJS_INTL_DATA_ALIAS:
            if (read_u32(r) > 4 ||
                !record_string(pool, r, 4, 0, 0, &a) ||
                !span_valid(view, QJS_INTL_DATA_REPLACEMENT_LIST, r + 12, 1))
                return 0;
            break;
        case QJS_INTL_DATA_REPLACEMENT_LIST:
        case QJS_INTL_DATA_LIST:
            if (!record_string(pool, r, 0, 0, 0, &a))
                return 0;
            sorted = 0;
            break;
        case QJS_INTL_DATA_LIKELY:
            if (!record_string(pool, r, 0, 0, 0, &a) ||
                !record_string(pool, r, 8, 0, 0, &b))
                return 0;
            break;
        case QJS_INTL_DATA_BCP47:
            if (!record_string(pool, r, 0, 0, 0, &a) || a.length != 1 ||
                (a.data[0] != 'u' && a.data[0] != 't') ||
                !record_string(pool, r, 8, 0, 0, &b) || b.length != 2 ||
                !lower_alnum(b.data[0]) || !lower_alnum(b.data[1]) ||
                !record_string(pool, r, 16, 1, 0, &c) ||
                !record_string(pool, r, 24, 1, 1, &c) ||
                !record_string(pool, r, 32, 1, 0, &c) ||
                read_u32(r + 40) > 3 || read_u32(r + 44) > 1)
                return 0;
            break;
        case QJS_INTL_DATA_WEEK:
            if (!record_string(pool, r, 0, 0, 0, &a) ||
                r[8] < 1 || r[8] > 7 || (r[9] & 0x80) ||
                r[10] < 1 || r[10] > 7 || r[11])
                return 0;
            break;
        case QJS_INTL_DATA_PREFERENCE:
            if (!record_string(pool, r, 0, 0, 0, &a) ||
                read_u32(r + 8) > 1 ||
                !span_valid(view, QJS_INTL_DATA_LIST, r + 12, 0) ||
                !span_valid(view, QJS_INTL_DATA_LIST, r + 20, 0) ||
                !span_valid(view, QJS_INTL_DATA_LIST, r + 28, 0) ||
                (read_u16(view->data + QJS_INTL_H_SCHEMA_MINOR) >= 1 &&
                 !validate_hour_lists(view, pool, r)))
                return 0;
            break;
        case QJS_INTL_DATA_LOCALE:
            if (!record_string(pool, r, 0, 0, 0, &a) ||
                !index_valid(read_u32(r + 8), locale_count, 1) ||
                !record_string(pool, r, 12, 0, 0, &b) ||
                !record_string(pool, r, 20, 1, 0, &c) ||
                !record_string(pool, r, 28, 1, 0, &c) ||
                !record_string(pool, r, 36, 1, 0, &c) ||
                (read_u32(r + 44) & ~UINT32_C(0x3ff)))
                return 0;
            break;
        case QJS_INTL_DATA_NUMBERING: {
            uint32_t digit;
            if (!record_string(pool, r, 0, 0, 0, &a) ||
                r[8] != 10 || r[9] > 1 || read_u16(r + 10))
                return 0;
            for (digit = 0; digit < 10; digit++) {
                uint32_t cp = read_u32(r + 12 + 4 * digit);
                if ((r[9] && cp) || (!r[9] && !scalar_valid(cp)))
                    return 0;
                if (!r[9]) {
                    uint32_t earlier;
                    for (earlier = 0; earlier < digit; earlier++) {
                        if (read_u32(r + 12 + 4 * earlier) == cp)
                            return 0;
                    }
                }
            }
            break;
        }
        case QJS_INTL_DATA_COMPONENT_PARENT:
            if (!record_string(pool, r, 0, 0, 0, &a) ||
                !index_valid(read_u32(r + 8), locale_count, 0) ||
                !index_valid(read_u32(r + 12), locale_count, 1) ||
                read_u32(r + 16))
                return 0;
            break;
        case QJS_INTL_DATA_REGION_TIME_ZONE:
            if (!record_string(pool, r, 0, 0, 0, &a) ||
                !span_valid(view, QJS_INTL_DATA_LIST, r + 8, 0))
                return 0;
            break;
        case QJS_INTL_DATA_LOCALE_AUX:
            if (!index_valid(read_u32(r), locale_count, 0) ||
                (read_u32(r + 4) & ~UINT32_C(1)) || read_u32(r + 8) > 2 ||
                !record_string(pool, r, 12, 1, 0, &a) ||
                !numbering_named(view, pool, a) ||
                !record_string(pool, r, 20, 1, 0, &a) ||
                !numbering_named(view, pool, a) ||
                !record_string(pool, r, 28, 1, 0, &a) ||
                !numbering_named(view, pool, a) ||
                !record_string(pool, r, 36, 1, 0, &a) ||
                !optional_type_valid(a))
                return 0;
            break;
        case QJS_INTL_DATA_NUMBERING_RULE: {
            QJSIntlDataSection numbering;
            uint32_t index = read_u32(r);
            if (qjs_intl_data_section(view, QJS_INTL_DATA_NUMBERING, &numbering) !=
                QJS_INTL_DATA_OK || index >= numbering.record_count ||
                numbering.bytes.data[(size_t)index * 52 + 9] != 1 ||
                !record_string(pool, r, 4, 0, 1, &a))
                return 0;
            break;
        }
        case QJS_INTL_DATA_HOUR_DEFAULT:
            if (!record_string(pool, r, 0, 0, 0, &a) || read_u32(r + 8) > 1 ||
                !record_string(pool, r, 12, 0, 0, &b) ||
                !preferred_hour_allowed(view, pool, a, read_u32(r + 8), b))
                return 0;
            break;
        case QJS_INTL_DATA_SCRIPT_DIRECTION:
            if (!record_string(pool, r, 0, 0, 0, &a) || a.length != 4 ||
                a.data[0] < 'A' || a.data[0] > 'Z' ||
                a.data[1] < 'a' || a.data[1] > 'z' ||
                a.data[2] < 'a' || a.data[2] > 'z' ||
                a.data[3] < 'a' || a.data[3] > 'z' || read_u32(r + 8) > 2)
                return 0;
            break;
        case QJS_INTL_DATA_AVAILABLE_CALENDAR:
            if (!record_string(pool, r, 0, 0, 0, &a) || !optional_type_valid(a))
                return 0;
            break;
        case QJS_INTL_DATA_LOCALE_SERVICE_INFO:
            if (!locale_service_info_valid(view, pool, r)) return 0;
            break;
        case QJS_INTL_DATA_DISPLAY_LABEL:
            if (!index_valid(read_u32(r), locale_count, 0) ||
                read_u32(r + 4) > 6 || read_u32(r + 8) > 2 ||
                !pool_string(pool, read_u32(r + 12), read_u32(r + 16), &a) ||
                !display_code_valid(a) ||
                !pool_string(pool, read_u32(r + 20), read_u32(r + 24), &b) ||
                !b.length)
                return 0;
            break;
        case QJS_INTL_DATA_DISPLAY_PATTERN:
            if (!index_valid(read_u32(r), locale_count, 0) || read_u32(r + 4) > 6 ||
                (read_u32(r + 4) < 3 ? !pattern_string(pool, r, 8, &a) :
                 (!pool_string(pool, read_u32(r + 8), read_u32(r + 12), &a) || !a.length)))
                return 0;
            break;
        case QJS_INTL_DATA_SEGMENT_GCB:
        case QJS_INTL_DATA_SEGMENT_WB:
        case QJS_INTL_DATA_SEGMENT_SB:
        case QJS_INTL_DATA_SEGMENT_INCB:
        case QJS_INTL_DATA_SEGMENT_EP:
            if (!segment_range_valid(section->id, r, previous)) return 0;
            sorted = 0; /* scalar interval order checked above */
            break;
        case QJS_INTL_DATA_SEGMENT_RULE:
            if (section->record_count != 3 || read_u32(r) != i ||
                read_u32(r + 4) != (18u << 16) || read_u32(r + 8) != 49 ||
                read_u32(r + 12) != 1 ||
                (read_u32(r + 16) != 1 && read_u32(r + 16) != 2) ||
                (previous && read_u32(r + 16) != read_u32(previous + 16)) ||
                read_u32(r + 20))
                return 0;
            sorted = 0; /* exact granularity sequence checked above */
            break;
        case QJS_INTL_DATA_LIST_PATTERN:
            if (!index_valid(read_u32(r), locale_count, 0) ||
                r[4] > 2 || r[5] > 2 || r[6] > 3 || r[7] ||
                !pattern_string(pool, r, 8, &a) ||
                !pattern_string(pool, r, 16, &a) ||
                !pattern_string(pool, r, 24, &a) ||
                !pattern_string(pool, r, 32, &a))
                return 0;
            break;
        case QJS_INTL_DATA_LIST_HEBREW_SCRIPT: {
            uint32_t first = read_u32(r), last = read_u32(r + 4);
            if (first > last || !scalar_valid(first) || !scalar_valid(last) ||
                (first <= 0xdfff && last >= 0xd800) ||
                (previous && read_u32(previous + 4) >= first))
                return 0;
            break;
        }
        default:
            return 0;
        }
        if (sorted && previous && key_compare(section->id, pool, previous, r) >= 0)
            return 0;
        previous = r;
    }
    return 1;
}

static int validate_alias_self(const QJSIntlDataView *view, QJSIntlDataSlice pool)
{
    QJSIntlDataSection aliases, replacements;
    uint32_t i;
    if (qjs_intl_data_section(view, QJS_INTL_DATA_ALIAS, &aliases) !=
        QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_REPLACEMENT_LIST, &replacements) !=
        QJS_INTL_DATA_OK)
        return 0;
    for (i = 0; i < aliases.record_count; i++) {
        const unsigned char *r = aliases.bytes.data + (size_t)i * 20;
        QJSIntlDataSlice from;
        uint32_t j, first = read_u32(r + 12), count = read_u32(r + 16);
        if (!pool_string(pool, read_u32(r + 4), read_u32(r + 8), &from))
            return 0;
        for (j = 0; j < count; j++) {
            const unsigned char *ref = replacements.bytes.data +
                                       ((size_t)first + j) * 8;
            QJSIntlDataSlice to;
            if (!pool_string(pool, read_u32(ref), read_u32(ref + 4), &to) ||
                !slice_compare(from, to))
                return 0;
        }
    }
    return 1;
}

static int validate_parent_chains(const QJSIntlDataSection *locales)
{
    uint32_t i;
    for (i = 0; i < locales->record_count; i++) {
        uint32_t current = i, steps = 0;
        while (current != QJS_INTL_DATA_INDEX_NONE) {
            if (current >= locales->record_count || steps >= locales->record_count)
                return 0;
            steps++;
            current = read_u32(locales->bytes.data + (size_t)current * 48 + 8);
        }
    }
    return 1;
}

static uint32_t component_parent(QJSIntlDataSlice pool,
                                 const QJSIntlDataSection *components,
                                 QJSIntlDataSlice component, uint32_t child,
                                 const QJSIntlDataSection *locales)
{
    uint32_t lo = 0, hi = components->record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const unsigned char *r = components->bytes.data + (size_t)mid * 20;
        QJSIntlDataSlice name;
        int compare;
        uint32_t index;
        /* All references/indices have already passed validate_records. */
        if (!pool_string(pool, read_u32(r), read_u32(r + 4), &name))
            return child; /* deterministic cycle failure if invariants break */
        compare = slice_compare(name, component);
        index = read_u32(r + 8);
        if (compare < 0 || (!compare && index < child)) {
            lo = mid + 1;
        } else if (compare > 0 || index > child) {
            hi = mid;
        } else {
            return read_u32(r + 12);
        }
    }
    return read_u32(locales->bytes.data + (size_t)child * 48 + 8);
}

static int validate_component_chains(QJSIntlDataSlice pool,
                                     const QJSIntlDataSection *components,
                                     const QJSIntlDataSection *locales)
{
    uint32_t i;
    for (i = 0; i < components->record_count; i++) {
        const unsigned char *r = components->bytes.data + (size_t)i * 20;
        QJSIntlDataSlice name;
        uint32_t current = read_u32(r + 8), steps = 0;
        if (!pool_string(pool, read_u32(r), read_u32(r + 4), &name))
            return 0;
        while (current != QJS_INTL_DATA_INDEX_NONE) {
            if (current >= locales->record_count || steps >= locales->record_count)
                return 0;
            steps++;
            current = component_parent(pool, components, name, current, locales);
        }
    }
    return 1;
}

/* Patterns are local immutable metadata. Runtime predicates and formatting
 * remain ListFormat engine work; these checks seal the approved alternates.
 */
static int expected_list_context(QJSIntlDataSlice language,
                                  QJSIntlDataSlice pair, QJSIntlDataSlice end)
{
    if (slice_equals_text(language, "es")) {
        int and_match = slice_equals_text(pair, "{0} y {1}") ||
                        slice_equals_text(end, "{0} y {1}");
        int or_match = slice_equals_text(pair, "{0} o {1}") ||
                       slice_equals_text(end, "{0} o {1}");
        if (and_match && or_match)
            return -1; /* two required contexts cannot share one quartet */
        return and_match ? 1 : or_match ? 2 : 0;
    }
    if ((slice_equals_text(language, "he") || slice_equals_text(language, "iw")) &&
        (slice_equals_text(pair, "{0} \xd7\x95{1}") ||
         slice_equals_text(end, "{0} \xd7\x95{1}")))
        return 3;
    return 0;
}

static int alternate_pattern_matches(QJSIntlDataSlice base,
                                       QJSIntlDataSlice alternate, int context)
{
    const char *from, *to;
    switch (context) {
    case 1: from = "{0} y {1}"; to = "{0} e {1}"; break;
    case 2: from = "{0} o {1}"; to = "{0} u {1}"; break;
    case 3: from = "{0} \xd7\x95{1}"; to = "{0} \xd7\x95-{1}"; break;
    default: return 0;
    }
    return slice_equals_text(base, from) ? slice_equals_text(alternate, to) :
           !slice_compare(base, alternate);
}

static int same_list_group(const unsigned char *a, const unsigned char *b)
{
    return read_u32(a) == read_u32(b) && a[4] == b[4] && a[5] == b[5];
}

static int validate_list_contexts(const QJSIntlDataView *view,
                                  QJSIntlDataSlice pool,
                                  const QJSIntlDataSection *locales)
{
    QJSIntlDataSection patterns, hebrew;
    uint32_t i = 0;
    QJSIntlDataStatus status;
    status = qjs_intl_data_section(view, QJS_INTL_DATA_LIST_PATTERN, &patterns);
    if (status == QJS_INTL_DATA_NOT_FOUND)
        return 1;
    if (status != QJS_INTL_DATA_OK)
        return 0;
    memset(&hebrew, 0, sizeof(hebrew));
    (void)qjs_intl_data_section(view, QJS_INTL_DATA_LIST_HEBREW_SCRIPT, &hebrew);
    while (i < patterns.record_count) {
        const unsigned char *base = patterns.bytes.data + (size_t)i * 40;
        const unsigned char *locale;
        QJSIntlDataSlice language, base_pair, base_end;
        uint32_t j = i + 1, index = read_u32(base);
        int context;
        while (j < patterns.record_count &&
               same_list_group(base, patterns.bytes.data + (size_t)j * 40))
            j++;
        if (base[6] || j - i > 2 || index >= locales->record_count)
            return 0;
        locale = locales->bytes.data + (size_t)index * 48;
        if (!pool_string(pool, read_u32(locale + 12), read_u32(locale + 16), &language) ||
            !pattern_string(pool, base, 8, &base_pair) ||
            !pattern_string(pool, base, 32, &base_end))
            return 0;
        context = expected_list_context(language, base_pair, base_end);
        if (context < 0 || (context == 0 ? j - i != 1 : j - i != 2))
            return 0;
        if (context) {
            const unsigned char *alternate = base + 40;
            QJSIntlDataSlice a, b;
            size_t field;
            if (alternate[6] != (unsigned char)context ||
                (context == 3 && !hebrew.record_count))
                return 0;
            for (field = 8; field <= 32; field += 8) {
                if (!pattern_string(pool, base, field, &a) ||
                    !pattern_string(pool, alternate, field, &b))
                    return 0;
                if (field == 8 || field == 32) {
                    if (!alternate_pattern_matches(a, b, context))
                        return 0;
                } else if (slice_compare(a, b)) {
                    return 0;
                }
            }
        }
        i = j;
    }
    return 1;
}

static int validate_locale_info_inventory(const QJSIntlDataView *view,
                                          QJSIntlDataSlice pool,
                                          const QJSIntlDataSection *locales)
{
    QJSIntlDataSection calendars, info;
    uint32_t i, j;
    int gregory = 0;
    if (qjs_intl_data_section(view, QJS_INTL_DATA_AVAILABLE_CALENDAR, &calendars) == QJS_INTL_DATA_OK) {
        for (i = 0; i < calendars.record_count; i++) {
            const unsigned char *r = calendars.bytes.data + (size_t)i * 8;
            QJSIntlDataSlice name;
            if (!record_string(pool, r, 0, 0, 0, &name)) return 0;
            if (slice_equals_text(name, "gregory")) gregory = 1;
        }
        if (!gregory) return 0;
    }
    if (qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE_SERVICE_INFO, &info) != QJS_INTL_DATA_OK)
        return 1; /* Older service blobs need not supply the new LocaleInfo table. */
    for (i = 0; i < locales->record_count; i++) {
        const unsigned char *r = locales->bytes.data + (size_t)i * 48;
        uint32_t service, coverage = read_u32(r + 44);
        for (service = 0; service < 2; service++) {
            int found = 0;
            for (j = 0; j < info.record_count; j++) {
                const unsigned char *entry = info.bytes.data + (size_t)j * 28;
                if (read_u32(entry) == i && read_u32(entry + 4) == service) { found = 1; break; }
            }
            if (found != !!(coverage & (UINT32_C(1) << (service ? 3 : 1)))) return 0;
        }
    }
    return 1;
}

/* Presence is a schema fact even for empty sections. Required Segmenter
 * ranges/rules cannot be empty, and EP mode never supplies implicit data. */
static int validate_segment_group(const QJSIntlDataView *view)
{
    QJSIntlDataSection sections[6];
    uint32_t i, present = 0, mode;
    for (i = 0; i < 6; i++) {
        QJSIntlDataStatus status = qjs_intl_data_section(view,
            QJS_INTL_DATA_SEGMENT_GCB + i, &sections[i]);
        if (status == QJS_INTL_DATA_OK) present |= 1u << i;
        else if (status != QJS_INTL_DATA_NOT_FOUND) return 0;
    }
    if (!present) return 1;
    if ((present & 47u) != 47u ||
        read_u32(view->data + QJS_INTL_H_UNICODE_VERSION) != (18u << 16) ||
        read_u32(view->data + QJS_INTL_H_CLDR_VERSION) != (49u << 16) ||
        sections[5].record_count != 3)
        return 0;
    for (i = 0; i < 4; i++)
        if (!sections[i].record_count) return 0;
    mode = read_u32(sections[5].bytes.data + 16);
    if (mode == 1) return (present & 16u) && sections[4].record_count;
    return mode == 2 && !(present & 16u);
}

QJSIntlDataStatus qjs_intl_data_open(const void *data, size_t length,
                                    QJSIntlDataView *out)
{
    QJSIntlDataView view;
    QJSIntlDataSection pool, locales, components;
    size_t at;
    uint32_t i, count;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!data || !out)
        return QJS_INTL_DATA_INVALID_ARGUMENT;
    view.data = (const unsigned char *)data;
    view.length = length;
    if (length < QJS_INTL_DATA_HEADER_SIZE ||
        memcmp(view.data, QJS_INTL_DATA_MAGIC, 8))
        return QJS_INTL_DATA_INVALID;
    if (read_u16(view.data + QJS_INTL_H_SCHEMA_MAJOR) != QJS_INTL_DATA_SCHEMA_MAJOR ||
        read_u16(view.data + QJS_INTL_H_SCHEMA_MINOR) > 3u)
        return QJS_INTL_DATA_UNSUPPORTED_VERSION;
    if (!validate_layout(&view) ||
        !(read_u32(view.data + QJS_INTL_H_UNICODE_VERSION) >> 16) ||
        !(read_u32(view.data + QJS_INTL_H_CLDR_VERSION) >> 16) ||
        qjs_intl_data_section(&view, QJS_INTL_DATA_UTF8_POOL, &pool) !=
        QJS_INTL_DATA_OK || !validate_pool(pool.bytes))
        return QJS_INTL_DATA_INVALID;
    memset(&locales, 0, sizeof(locales));
    memset(&components, 0, sizeof(components));
    (void)qjs_intl_data_section(&view, QJS_INTL_DATA_LOCALE, &locales);
    (void)qjs_intl_data_section(&view, QJS_INTL_DATA_COMPONENT_PARENT, &components);
    if (!directory_bounds(&view, &at, &count))
        return QJS_INTL_DATA_INVALID;
    for (i = 0; i < count; i++) {
        QJSIntlDataSection section;
        if (!section_decode(&view, directory_record(&view, at, i), &section) ||
            !validate_records(&view, pool.bytes, &section, locales.record_count))
            return QJS_INTL_DATA_INVALID;
    }
    if (!validate_segment_group(&view) ||
        !validate_locale_info_inventory(&view, pool.bytes, &locales) ||
        !validate_alias_self(&view, pool.bytes) ||
        !validate_parent_chains(&locales) ||
        !validate_component_chains(pool.bytes, &components, &locales) ||
        !validate_list_contexts(&view, pool.bytes, &locales))
        return QJS_INTL_DATA_INVALID;
    if (qjs_intl_data_validate_plural_extension(&view) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_INVALID;
    {
        QJSIntlDataStatus relative = qjs_intl_data_validate_relative_extension(&view);
        if ((relative != QJS_INTL_DATA_OK && relative != QJS_INTL_DATA_NOT_FOUND) ||
            qjs_intl_number_extra_data_validate(&view) != QJS_INTL_DATA_OK ||
            qjs_intl_duration_data_validate(&view) != QJS_INTL_DATA_OK ||
            qjs_intl_date_data_validate(&view) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_INVALID;
    }
    if (qjs_intl_data_validate_collation_extension(&view) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_INVALID;
    *out = view;
    return QJS_INTL_DATA_OK;
}
