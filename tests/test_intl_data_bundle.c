/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/data/native-data-reader.h"
#include "../src/intl/data/locale-metadata.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    QJSIntlDataView view;
    QJSIntlDataSection pool, locales;
    unsigned char *allocation = malloc(qjs_intl_locale_metadata_blob_size + 1);
    unsigned char *copy;
    assert(allocation);
    copy = allocation + 1;
    assert(qjs_intl_data_open(qjs_intl_locale_metadata_blob,
                qjs_intl_locale_metadata_blob_size, &view) == QJS_INTL_DATA_OK);
    assert(qjs_intl_data_section(&view, QJS_INTL_DATA_UTF8_POOL, &pool) == QJS_INTL_DATA_OK);
    assert(pool.bytes.length);
    assert(qjs_intl_data_section(&view, QJS_INTL_DATA_LOCALE, &locales) == QJS_INTL_DATA_OK);
    assert(locales.record_count >= 2);
    memcpy(copy, qjs_intl_locale_metadata_blob, qjs_intl_locale_metadata_blob_size);
    assert(qjs_intl_data_open(copy, qjs_intl_locale_metadata_blob_size,
                             &view) == QJS_INTL_DATA_OK);
    /* All reference bounds are checked before any output view is published. */
    memset(copy + QJS_INTL_H_SECTION_COUNT, 0xff, 4);
    assert(qjs_intl_data_open(copy, qjs_intl_locale_metadata_blob_size,
                             &view) == QJS_INTL_DATA_INVALID);
    assert(!view.data && !view.length);
    memcpy(copy, qjs_intl_locale_metadata_blob, qjs_intl_locale_metadata_blob_size);
    copy[QJS_INTL_H_RESERVED] = 1;
    assert(qjs_intl_data_open(copy, qjs_intl_locale_metadata_blob_size,
                             &view) == QJS_INTL_DATA_INVALID);
    assert(!view.data && !view.length);
    free(allocation);
    return 0;
}
