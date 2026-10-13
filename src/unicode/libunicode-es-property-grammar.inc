#include "libunicode-es-binary-properties.h"

int lre_unicode_property_allowed(const char *name)
{
#define UNICODE_ES_NAME(names, value) names,
    static const char *const aliases[] = {
        "ASCII", "Any", "Assigned",
        UNICODE_ES_BINARY_PROPERTIES(UNICODE_ES_NAME)
    };
#undef UNICODE_ES_NAME
    size_t i, length;
    if (!name || !*name)
        return 0;
    length = strlen(name);
    for (i = 0; i < countof(aliases); i++) {
        const char *start = aliases[i], *end;
        do {
            end = strchr(start, ',');
            if (length == (end ? (size_t)(end - start) : strlen(start)) &&
                !memcmp(name, start, length))
                return 1;
            if (!end)
                break;
            start = end + 1;
        } while (*start);
    }
    return 0;
}
