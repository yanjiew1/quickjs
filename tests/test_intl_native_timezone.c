/* Native standalone default identifier, byte lifetime and failure ownership. */
#include "../src/intl/provider-native.h"
#include "../src/timezone/timezone.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocator { size_t live; } Allocator;
static void *allocate(void *opaque, size_t size)
{ void *p = malloc(size); if (p) ((Allocator *)opaque)->live++; return p; }
static void *resize(void *opaque, void *pointer, size_t size)
{
    int was_null = pointer == NULL;
    void *p = realloc(pointer, size);
    if (p && was_null) ((Allocator *)opaque)->live++;
    return p;
}
static void deallocate(void *opaque, void *pointer)
{
    if (pointer) { assert(((Allocator *)opaque)->live); ((Allocator *)opaque)->live--; }
    free(pointer);
}
static void equals(QJSIntlBytes value, const char *expected)
{ assert(value.length == strlen(expected) && !memcmp(value.data, expected, value.length)); }
int main(void)
{
    Allocator a = {0};
    QJSIntlProviderConfig config = {0}; QJSIntlProvider *provider;
    const char *static_id;
    char input[] = "aMeRiCa/NeW_yOrK";
    static const char *const bad[] = {"Missing/Zone", "../UTC", "EST5EDT,M3.2.0,M11.1.0", ""};
    size_t i;
    config.backend = QJS_INTL_BACKEND_NATIVE;
    config.allocator = (QJSIntlAllocator){&a, allocate, resize, deallocate};
    config.default_locale = (QJSIntlBytes){"en-US", 5};
    assert(!qjs_intl_provider_new(&config, &provider));
    equals(qjs_intl_provider_default_time_zone(provider), "UTC");
    qjs_intl_provider_free(provider); assert(!a.live);
    config.default_time_zone = (QJSIntlBytes){input, strlen(input)};
    assert(!qjs_intl_provider_new(&config, &provider));
    assert(!qjs_tz_resolve(input, strlen(input), &static_id, NULL));
    assert(qjs_intl_provider_default_time_zone(provider).data == static_id);
    memset(input, 'X', sizeof input - 1);
    equals(qjs_intl_provider_default_time_zone(provider), "America/New_York");
    qjs_intl_provider_free(provider); assert(!a.live);
    config.default_time_zone = (QJSIntlBytes){"us/eastern", 10};
    assert(!qjs_intl_provider_new(&config, &provider));
    equals(qjs_intl_provider_default_time_zone(provider), "US/Eastern");
    qjs_intl_provider_free(provider); assert(!a.live);
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        config.default_time_zone = (QJSIntlBytes){bad[i], strlen(bad[i])};
        provider = (void *)1;
        assert(qjs_intl_provider_new(&config, &provider) == QJS_INTL_UNSUPPORTED);
        assert(!provider && !a.live);
    }
    config.default_time_zone = (QJSIntlBytes){NULL, 1};
    assert(qjs_intl_provider_new(&config, &provider) == QJS_INTL_INVALID_ARGUMENT);
    assert(!provider && !a.live);
    return 0;
}
