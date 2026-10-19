/* Exact/prefix locale matching, extension removal and priority regressions.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/locale/matching.h"
#include <assert.h>
#include <string.h>

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static void match(const char *request, const char *expected, const char *extension)
{
    static const QJSIntlBytes available[] = {
        {"de", 2},    {"de-DE", 5},          {"en", 2},
        {"en-GB", 5}, {"sl-1994-rozaj", 13}, {"zh-Hant", 7},
    };
    QJSIntlBytes input = bytes(request);
    QJSIntlLocaleMatch result;
    assert(!qjs_intl_lookup_matching_locale(available, 6, &input, 1, &result));
    assert(result.found == (expected != NULL));
    if (expected) {
        QJSIntlBytes name = available[result.available_index];
        assert(result.requested_index == 0);
        assert(name.length == strlen(expected) &&
               !memcmp(name.data, expected, name.length));
        if (extension) {
            assert(result.extension.length == strlen(extension));
            assert(!memcmp(result.extension.data, extension, result.extension.length));
        } else {
            assert(!result.extension.data && !result.extension.length);
        }
    } else {
        assert(!result.extension.data && !result.extension.length);
    }
}

int main(void)
{
    QJSIntlLocaleMatch result;
    QJSIntlBytes available[] = {bytes("de"), bytes("en")};
    QJSIntlBytes requested[] = {bytes("fr"), bytes("en-US-u-kn"), bytes("de")};
    const char raw[] = {'e', 'n', '-', 'u', '-', 'k', 'n', '-', 'v', '-', 'f', 'o', 'o'};
    QJSIntlBytes slice = {raw, sizeof(raw)};

    match("en-GB", "en-GB", NULL);
    match("en-AU", "en", NULL);
    match("en-GB-u-ca-buddhist", "en-GB", "-u-ca-buddhist");
    match("en-GB-u-kn-x-private", "en-GB", "-u-kn");
    match("en-GB-a-foo-u-kn-x-private", "en-GB", "-u-kn");
    match("en-x-u-kn", "en", NULL);
    match("de-DE-t-en-a0-foo-u-kn", "de-DE", "-u-kn");
    match("sl-1994-rozaj-u-nu-latn", "sl-1994-rozaj", "-u-nu-latn");
    match("zh-Hant-HK", "zh-Hant", NULL);
    match("zh-Hans", NULL, NULL);
    match("fr-u-ca-gregory", NULL, NULL);
    assert(!qjs_intl_lookup_matching_locale(available, 2, requested, 3, &result));
    assert(result.found && result.available_index == 1 && result.requested_index == 1);
    assert(!qjs_intl_lookup_matching_locale(available, 2, &slice, 1, &result));
    assert(result.found && result.available_index == 1 && result.requested_index == 0);
    assert(result.extension.data == raw + 2 && result.extension.length == 5);
    assert(!qjs_intl_lookup_matching_locale(NULL, 0, requested, 3, &result));
    assert(!result.found && !result.available_index && !result.requested_index &&
           !result.extension.data && !result.extension.length);
    assert(!qjs_intl_lookup_matching_locale(available, 2, NULL, 0, &result));
    assert(!result.found);
    assert(!qjs_intl_lookup_matching_locale(available, 2, &slice, 1, &result));
    assert(result.found);
    assert(qjs_intl_lookup_matching_locale(NULL, 1, requested, 3, &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!result.found && !result.extension.data);
    assert(qjs_intl_lookup_matching_locale(available, 2, NULL, 1, &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_lookup_matching_locale(available, 2, requested, 3, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    return 0;
}
