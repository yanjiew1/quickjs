/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "system.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

QJSIntlStatus qjs_intl_locale_parse_environment(char *out, size_t capacity,
                                                const char *value)
{
    size_t length, i;
    if (!out || !capacity || !value)
        return QJS_INTL_INVALID_ARGUMENT;
    length = strcspn(value, ".@");
    if (!length)
        return QJS_INTL_UNSUPPORTED;
    if ((length == 1 && value[0] == 'C') ||
        (length == 5 && !memcmp(value, "POSIX", 5))) {
        value = "en";
        length = 2;
    }
    if (length >= capacity)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < length; i++)
        out[i] = value[i] == '_' ? '-' : value[i];
    out[length] = 0;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_locale_system_default(char *out, size_t capacity)
{
#ifdef _WIN32
    WCHAR name[LOCALE_NAME_MAX_LENGTH];
    char ascii[LOCALE_NAME_MAX_LENGTH];
    int length, i;
    if (!out || !capacity)
        return QJS_INTL_INVALID_ARGUMENT;
    /* This older API also works on systems where LOCALE_SNAME is unsupported:
       failure selects the caller's fallback rather than importing a new API. */
    length =
        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SNAME, name, LOCALE_NAME_MAX_LENGTH);
    if (length <= 1 || length > LOCALE_NAME_MAX_LENGTH)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < length; i++) {
        if (name[i] > 127)
            return QJS_INTL_UNSUPPORTED;
        ascii[i] = name[i];
    }
    return qjs_intl_locale_parse_environment(out, capacity, ascii);
#else
    static const char *const variables[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    size_t i;
    if (!out || !capacity)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < sizeof(variables) / sizeof(*variables); i++) {
        const char *value = getenv(variables[i]);
        if (value && *value)
            return qjs_intl_locale_parse_environment(out, capacity, value);
    }
    return QJS_INTL_UNSUPPORTED;
#endif
}
