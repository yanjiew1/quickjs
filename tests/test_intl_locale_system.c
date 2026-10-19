/* Host discovery does not mutate the libc locale or claim service support. */
#include "../src/intl/locale/system.h"
#include "environment.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <pthread.h>

static void *change_locale(void *opaque)
{
    int i;
    (void)opaque;
    for (i = 0; i < 1000; i++) {
        js_host_environment_lock();
        assert(!setenv("LC_ALL", i & 1 ? "en_US.UTF-8" : "fr_FR.UTF-8", 1));
        js_host_environment_unlock();
    }
    return NULL;
}
#endif

static void parsed(const char *source, const char *expected)
{
    char buffer[128];
    assert(qjs_intl_locale_parse_environment(buffer, sizeof(buffer), source) ==
           QJS_INTL_OK);
    assert(!strcmp(buffer, expected));
}

int main(void)
{
    char buffer[128] = "unchanged";
    parsed("en_US.UTF-8", "en-US");
    parsed("sr_RS@latin", "sr-RS");
    parsed("zh_Hant_TW.UTF8@custom", "zh-Hant-TW");
    parsed("en-US", "en-US");
    parsed("C.UTF-8", "en");
    parsed("POSIX", "en");
    assert(qjs_intl_locale_parse_environment(buffer, sizeof(buffer), "") ==
           QJS_INTL_UNSUPPORTED);
    assert(!strcmp(buffer, "unchanged"));
    assert(qjs_intl_locale_parse_environment(buffer, 2, "C") == QJS_INTL_UNSUPPORTED);
    assert(!strcmp(buffer, "unchanged"));
    assert(qjs_intl_locale_parse_environment(buffer, 3, "en_US") ==
           QJS_INTL_UNSUPPORTED);
    assert(!strcmp(buffer, "unchanged"));
    assert(qjs_intl_locale_parse_environment(NULL, 3, "en") ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_locale_parse_environment(buffer, 0, "en") ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_locale_parse_environment(buffer, sizeof(buffer), NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
#ifndef _WIN32
    {
        const char *names[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
        char *saved[3];
        size_t i;
        for (i = 0; i < 3; i++) {
            const char *old = getenv(names[i]);
            saved[i] = old ? strdup(old) : NULL;
            assert(!old || saved[i]);
            assert(!unsetenv(names[i]));
        }
        assert(qjs_intl_locale_system_default(buffer, sizeof(buffer)) ==
               QJS_INTL_UNSUPPORTED);
        assert(!setenv("LANG", "de_DE.UTF-8", 1));
        assert(qjs_intl_locale_system_default(buffer, sizeof(buffer)) == QJS_INTL_OK);
        assert(!strcmp(buffer, "de-DE"));
        assert(!setenv("LC_MESSAGES", "fr_CA.UTF-8", 1));
        assert(qjs_intl_locale_system_default(buffer, sizeof(buffer)) == QJS_INTL_OK);
        assert(!strcmp(buffer, "fr-CA"));
        assert(!setenv("LC_ALL", "C", 1));
        assert(qjs_intl_locale_system_default(buffer, sizeof(buffer)) == QJS_INTL_OK);
        assert(!strcmp(buffer, "en"));
        {
            pthread_t worker;
            assert(!setenv("LC_ALL", "en_US.UTF-8", 1));
            assert(!pthread_create(&worker, NULL, change_locale, NULL));
            for (i = 0; i < 1000; i++) {
                assert(qjs_intl_locale_system_default(buffer, sizeof(buffer)) == QJS_INTL_OK);
                assert(!strcmp(buffer, "en-US") || !strcmp(buffer, "fr-FR"));
            }
            assert(!pthread_join(worker, NULL));
        }
        for (i = 0; i < 3; i++) {
            if (saved[i]) {
                assert(!setenv(names[i], saved[i], 1));
                free(saved[i]);
            } else {
                assert(!unsetenv(names[i]));
            }
        }
    }
#endif
    return 0;
}
