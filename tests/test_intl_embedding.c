/* Check compiled prepared-data bytes, not the generator's textual spelling.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/native/data.h"
#include <assert.h>

extern const size_t qjs_test_embedding_storage_length;

int main(void)
{
    QJSIntlPack pack;
    QJSIntlSection section;
    size_t i;

    assert(qjs_test_embedding_storage_length == qjs_intl_native_length);
    assert(!qjs_intl_pack_open(qjs_intl_native_bytes, qjs_intl_native_length, &pack));
    assert(!qjs_intl_pack_section(&pack, 123, &section));
    assert(section.count == 1 && section.length == 256 * 17 + 1);
    for (i = 0; i + 1 < section.length; i++)
        assert(section.bytes[i] == i % 256);
    assert(!section.bytes[section.length - 1]);
    return 0;
}
