/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "grammar-data.h"
#include "../common/source-locale.h"
#include "../native/wire.h"
#include <string.h>

enum { HEADER = 2, WIDTH = 6 };

static uint32_t field(const QJSIntlUnitGrammarData *data, uint32_t row, unsigned int column)
{
    return qjs_intl_read_u32(data->rows + ((size_t)row * WIDTH + column) * 4);
}

static int token(QJSIntlBytes text, int plural)
{
    static const char *const plurals[] = {"compound", "zero", "one", "two", "few", "many", "other"};
    size_t i;
    if (plural) {
        for (i = 0; i < sizeof(plurals) / sizeof(*plurals); i++) {
            if (text.length == strlen(plurals[i]) && !memcmp(text.data, plurals[i], text.length))
                return 1;
        }
        return 0;
    }
    if (!text.length)
        return 0;
    for (i = 0; i < text.length; i++) {
        if (text.data[i] < 'a' || text.data[i] > 'z')
            return 0;
    }
    return 1;
}

QJSIntlStatus qjs_intl_unit_grammar_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                             QJSIntlUnitGrammarData *out)
{
    QJSIntlUnitGrammarData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    size_t words;
    uint32_t i, j;
    uint8_t *checked, *used;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_UNIT_GRAMMAR, &result.section) ||
        result.section.count != data->locales.count || !result.section.count ||
        result.section.length < HEADER * 4 || result.section.length % 4 ||
        qjs_intl_read_u32(result.section.bytes) != 1)
        return status;
    result.row_count = qjs_intl_read_u32(result.section.bytes + 4);
    words = result.section.length / 4 - HEADER;
    if (!result.row_count || result.row_count > result.section.count ||
        words < result.section.count || (words - result.section.count) % WIDTH ||
        (words - result.section.count) / WIDTH != result.row_count)
        return status;
    result.indices = result.section.bytes + HEADER * 4;
    result.rows = result.indices + (size_t)result.section.count * 4;
    if (data->strings.count > SIZE_MAX - (size_t)result.row_count)
        return QJS_INTL_OVERFLOW;
    checked = allocator->malloc(allocator->opaque, (size_t)data->strings.count + result.row_count);
    if (!checked)
        return QJS_INTL_NO_MEMORY;
    memset(checked, 0, (size_t)data->strings.count + result.row_count);
    used = checked + data->strings.count;
    for (i = 0; i < result.section.count; i++) {
        uint32_t row = qjs_intl_read_u32(result.indices + (size_t)i * 4);
        if (row >= result.row_count)
            goto done;
        used[row] = 1;
    }
    for (i = 0; i < result.row_count; i++) {
        int order = 0;
        if (!used[i])
            goto done;
        for (j = 0; j < WIDTH; j++) {
            uint32_t id = field(&result, i, j);
            unsigned int role = j % 3 == 0 ? 1 : j < 3 ? 2 : 4;
            QJSIntlBytes text;
            if (qjs_intl_data_string(data, id, &text))
                goto done;
            if (i && !order) {
                uint32_t previous = field(&result, i - 1, j);
                order = (id > previous) - (id < previous);
            }
            if (!(checked[id] & role)) {
                if (role == 1) {
                    QJSIntlStatus origin_status = qjs_intl_source_locale_validate(allocator, text);
                    if (origin_status) {
                        status = origin_status;
                        goto done;
                    }
                } else if (!token(text, role == 2)) {
                    goto done;
                }
                checked[id] |= role;
            }
        }
        if (i && order <= 0)
            goto done;
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, checked);
    return status;
}

QJSIntlStatus qjs_intl_unit_grammar_at(const QJSIntlUnitGrammarData *data, uint32_t locale,
                                      QJSIntlUnitGrammarFeature feature, QJSIntlUnitGrammarRule *out)
{
    uint32_t row;
    unsigned int column;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        (unsigned int)feature >= QJS_INTL_UNIT_GRAMMAR_FEATURE_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    row = qjs_intl_read_u32(data->indices + (size_t)locale * 4);
    column = (unsigned int)feature * 3;
    qjs_intl_data_string(&data->data, field(data, row, column), &out->locale);
    qjs_intl_data_string(&data->data, field(data, row, column + 1), &out->first);
    qjs_intl_data_string(&data->data, field(data, row, column + 2), &out->second);
    return QJS_INTL_OK;
}
