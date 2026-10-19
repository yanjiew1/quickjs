/* Lateral name alternatives, exact-path blocks and source provenance.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/display/select.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static size_t allocations, live;

static void *allocate(void *opaque, size_t size)
{
    void *ptr;
    (void)opaque;
    assert(size);
    allocations++;
    ptr = malloc(size);
    if (ptr)
        live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    (void)opaque;
    assert(ptr && live);
    live--;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    QJSIntlBytes result = {text, strlen(text)};
    return result;
}

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) &&
           (!text.length || !memcmp(text.data, expected, text.length));
}

static uint32_t locale_id(const QJSIntlData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_locale(data, i, &text, &parent));
        if (equal(text, name))
            return i;
    }
    abort();
}

static void check(const QJSIntlNameData *owner, const char *locale, QJSIntlNameGroup group,
                  QJSIntlNameStyle style, int standalone, const char *code,
                  const char *text, const char *origin)
{
    QJSIntlNameLabel label, zero = {0};
    memset(&label, 0xff, sizeof(label));
    assert(!qjs_intl_name_select(owner, locale_id(&owner->data, locale), group, style,
                                  standalone, bytes(code), &label));
    if (text) {
        assert(label.found && !label.entry.blocked && equal(label.entry.text, text));
        assert(equal(label.origin_locale, origin));
    } else {
        assert(!memcmp(&label, &zero, sizeof(label)));
    }
}

int main(void)
{
    QJSIntlAllocator allocator = {NULL, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlNameData owner;
    QJSIntlNameLabel label, zero = {0};
    QJSIntlBytes invalid = {NULL, 1};
    size_t calls;
    unsigned int style;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_name_data_open(&data, &allocator, &owner) && !live);
    calls = allocations;
    for (style = QJS_INTL_NAME_LONG; style <= QJS_INTL_NAME_NARROW; style++) {
        /* The child ordinary label wins over a parent's short alternative.
           Menu/core names do not enter ordinary DisplayNames selection. */
        check(&owner, "en-GB", QJS_INTL_NAME_LANGUAGE, style, 0, "en", "English", "en-GB");
        check(&owner, "en", QJS_INTL_NAME_LANGUAGE, style, 0, "en",
              style ? "Root EN" : "English", "root");
        check(&owner, "en", QJS_INTL_NAME_LANGUAGE, style, 0, "fr", "", "en");
        check(&owner, "en", QJS_INTL_NAME_LANGUAGE, style, 0, "es", "", "en");
        check(&owner, "en", QJS_INTL_NAME_LANGUAGE, style, 0, "xy", "xy", "root");
        check(&owner, "en", QJS_INTL_NAME_LANGUAGE, style, 0, "absent", NULL, NULL);
        /* The default-path BLOCK cannot suppress a distinct short-path label;
           the short-path BLOCK cannot resurrect the parent's short label. */
        check(&owner, "en", QJS_INTL_NAME_LANGUAGE, style, 0, "de",
              style ? "Germ." : NULL, style ? "root" : NULL);
        check(&owner, "en", QJS_INTL_NAME_LANGUAGE, style, 0, "it", "Italian", "root");
        check(&owner, "en", QJS_INTL_NAME_CALENDAR, style, 0, "gregorian", "Gregorian Calendar", "root");
        check(&owner, "en", QJS_INTL_NAME_VARIANT, style, 0, "valencia", "Valencian variant", "root");
        check(&owner, "en", QJS_INTL_NAME_PATTERN, style, 0, "localePattern", "{0} ({1})", "root");
        check(&owner, "en", QJS_INTL_NAME_BRACKET, style, 0, "(", "[", "root");
        check(&owner, "de", QJS_INTL_NAME_REGION, style, 0, "us", "external region", "en-1901-1996");
        check(&owner, "en", QJS_INTL_NAME_SCRIPT, style, 0, "latn", style ? "Lat." : "Latin", "root");
        check(&owner, "en", QJS_INTL_NAME_SCRIPT, style, 1, "latn",
              "Child standalone Latin", "en");
        check(&owner, "en-GB", QJS_INTL_NAME_SCRIPT, style, 1, "latn", "British Latin", "en-GB");
        check(&owner, "root", QJS_INTL_NAME_SCRIPT, style, 1, "latn",
              style ? "Lat." : "Standalone Latin", "root");
    }
    /* Each independent lookup starts with fresh masks and alternatives. */
    check(&owner, "root", QJS_INTL_NAME_LANGUAGE, QJS_INTL_NAME_SHORT, 0, "it", "IT", "root");
    check(&owner, "root", QJS_INTL_NAME_LANGUAGE, QJS_INTL_NAME_LONG, 0, "de", "German", "root");
    memset(&label, 0xff, sizeof(label));
    assert(qjs_intl_name_select(&owner, 0, QJS_INTL_NAME_LANGUAGE, -1, 0, bytes("en"), &label) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&label, &zero, sizeof(label)));
    assert(qjs_intl_name_select(&owner, 0, QJS_INTL_NAME_LANGUAGE, QJS_INTL_NAME_LONG, 1,
                                 bytes("en"), &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_select(&owner, 0, QJS_INTL_NAME_SCRIPT, QJS_INTL_NAME_LONG, 2,
                                 bytes("latn"), &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_select(&owner, 0, -1, QJS_INTL_NAME_LONG, 0,
                                 bytes("en"), &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_select(&owner, UINT32_MAX, QJS_INTL_NAME_LANGUAGE, QJS_INTL_NAME_LONG, 0,
                                 bytes("en"), &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_select(&owner, 0, QJS_INTL_NAME_LANGUAGE, QJS_INTL_NAME_LONG, 0,
                                 invalid, &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_select(NULL, 0, QJS_INTL_NAME_LANGUAGE, QJS_INTL_NAME_LONG, 0,
                                 bytes("en"), &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_select(&owner, 0, QJS_INTL_NAME_LANGUAGE, QJS_INTL_NAME_LONG, 0,
                                 bytes("en"), NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(allocations == calls && !live);
    return 0;
}
