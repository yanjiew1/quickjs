/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/data/native-data-reader.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    unsigned char storage[QJS_INTL_DATA_HEADER_SIZE + 1] = {0};
    unsigned char *header = storage + 1; /* Deliberately unaligned. */
    QJSIntlDataView view;
    size_t length;
    assert(qjs_intl_data_open(NULL, 0, &view) == QJS_INTL_DATA_INVALID_ARGUMENT);
    assert(!view.data && !view.length);
    assert(qjs_intl_data_open(header, 64, NULL) == QJS_INTL_DATA_INVALID_ARGUMENT);
    memcpy(header, QJS_INTL_DATA_MAGIC, 8);
    header[QJS_INTL_H_SCHEMA_MAJOR] = QJS_INTL_DATA_SCHEMA_MAJOR;
    for (length = 0; length <= QJS_INTL_DATA_HEADER_SIZE; length++) {
        assert(qjs_intl_data_open(header, length, &view) == QJS_INTL_DATA_INVALID);
        assert(!view.data && !view.length);
    }
    header[QJS_INTL_H_SCHEMA_MAJOR]++;
    assert(qjs_intl_data_open(header, 64, &view) == QJS_INTL_DATA_UNSUPPORTED_VERSION);
    assert(!view.data && !view.length);
    return 0;
}
