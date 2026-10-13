/* Explicit en development bundle. Independent compiled released CLDR English witnesses.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT */
#include "../src/intl/list-native-data.h"
#include "../src/intl/data/locale-metadata.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void *am(void *opaque, size_t n) { (void)opaque; return malloc(n); }
static void af(void *opaque, void *p) { (void)opaque; free(p); }
static const QJSIntlAllocator allocator = {NULL, am, NULL, af};
static uint32_t english(const QJSIntlDataView *view)
{
    QJSIntlDataSection section;
    QJSIntlDataSlice text;
    uint32_t i;
    assert(qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &section) == QJS_INTL_DATA_OK);
    for (i = 0; i < section.record_count; i++) {
        assert(qjs_intl_data_record_string(view, &section, i, 0, &text) == QJS_INTL_DATA_OK);
        if (text.length == 2 && !memcmp(text.data, "en", 2)) return i;
    }
    assert(!"missing actual English locale"); return 0;
}
int main(void)
{
    static const char *const expected[3][3] = {
        {"A, B, and C", "A, B, & C", "A, B, C"},
        {"A, B, or C", "A, B, or C", "A, B, or C"},
        {"A, B, C", "A, B, C", "A B C"}
    };
    QJSIntlDataView view;
    uint16_t a = 'A', b = 'B', c = 'C';
    QJSIntlUTF16 items[] = {{&a, 1}, {&b, 1}, {&c, 1}};
    uint32_t index;
    unsigned int type, style;
    size_t i;
    assert(qjs_intl_data_open(qjs_intl_locale_metadata_blob,
                qjs_intl_locale_metadata_blob_size, &view) == QJS_INTL_DATA_OK);
    index = english(&view);
    for (type = 0; type < 3; type++) for (style = 0; style < 3; style++) {
        QJSIntlNativeList *list = NULL;
        QJSIntlFormatted out;
        assert(qjs_intl_native_list_open_data(&allocator, &view, index,
            (QJSIntlListType)type, (QJSIntlListStyle)style, &list) == QJS_INTL_OK);
        assert(qjs_intl_native_list_format(list, items, 3, &out) == QJS_INTL_OK);
        assert(out.length == strlen(expected[type][style]));
        for (i = 0; i < out.length; i++)
            assert(out.text[i] == (unsigned char)expected[type][style][i]);
        qjs_intl_native_list_close(list);
        qjs_intl_native_list_result_clear(&allocator, &out);
        assert(!out.text && !out.parts && !out.length && !out.part_count);
    }
    return 0;
}
