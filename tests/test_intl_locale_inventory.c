/* Exhaustive prepared formatter identity and required fallback validation. */
#include "../src/intl/locale/identifier.h"
#include "../src/intl/locale/native-id.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t live;
} Allocation;

static void *allocate(void *opaque, size_t length)
{
    Allocation *state = opaque;
    void *ptr;
    assert(length);
    ptr = malloc(length);
    if (ptr)
        state->live++;
    return ptr;
}
static void *resize(void *opaque, void *ptr, size_t length)
{
    Allocation *state = opaque;
    int had_ptr = ptr != NULL;
    void *result;
    assert(length);
    result = realloc(ptr, length);
    if (result && !had_ptr)
        state->live++;
    return result;
}
static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    if (ptr) {
        assert(state->live);
        state->live--;
    }
    free(ptr);
}

static int contains(const QJSIntlData *data, QJSIntlBytes key)
{
    uint32_t low = 0, high = data->locales.count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2, parent;
        QJSIntlBytes tag;
        size_t length;
        int order;
        assert(qjs_intl_data_locale(data, mid, &tag, &parent) == QJS_INTL_OK);
        length = key.length < tag.length ? key.length : tag.length;
        order = memcmp(key.data, tag.data, length);
        if (!order)
            order = (key.length > tag.length) - (key.length < tag.length);
        if (order < 0)
            high = mid;
        else if (order > 0)
            low = mid + 1;
        else
            return parent != UINT32_MAX;
    }
    return 0;
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlLocaleMetadata metadata;
    QJSIntlData data;
    uint32_t index;
    size_t count = 0;
    assert(qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator,
                              &data) == QJS_INTL_OK);
    assert(qjs_intl_locale_metadata_open(&data, &allocator, &metadata) == QJS_INTL_OK);
    for (index = 0; index < data.locales.count; index++) {
        QJSIntlBytes tag;
        QJSIntlLocaleComponents components;
        uint32_t parent;
        char *canonical;
        assert(qjs_intl_data_locale(&data, index, &tag, &parent) == QJS_INTL_OK);
        if (parent == UINT32_MAX)
            continue;
        count++;
        assert(qjs_intl_native_locale_canonicalize(&allocator, &metadata, tag,
                                                   &canonical) == QJS_INTL_OK);
        assert(strlen(canonical) == tag.length &&
               !memcmp(canonical, tag.data, tag.length));
        release(&state, canonical);
        assert(!state.live);
        assert(qjs_intl_locale_components(tag, &components) == QJS_INTL_OK);
        assert(components.base_name.length == tag.length);
        assert(contains(&data, components.language));
        if (components.script.data && components.region.data) {
            char key[16];
            size_t length = components.language.length + 1 + components.region.length;
            assert(length <= sizeof(key));
            memcpy(key, components.language.data, components.language.length);
            key[components.language.length] = '-';
            memcpy(key + components.language.length + 1, components.region.data,
                   components.region.length);
            assert(contains(&data, (QJSIntlBytes){key, length}));
        }
    }
    assert(data.pack.profile ? count > 1000 : count == 1);
    assert(!state.live);
    return 0;
}
