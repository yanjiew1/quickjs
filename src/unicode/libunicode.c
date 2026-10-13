/* Unicode algorithm/data provider selection. The native implementation and
   its checked callback behavior are retained in one included owner. Exactly
   one selected provider defines the private library interface. */
#include "cutils.h"
#include "libunicode.h"
#ifdef CONFIG_UNICODE_ICU
#include "libunicode-icu.c"
#else
#include "libunicode-native.c"

const char *lre_unicode_backend(void)
{
    return "native";
}

void lre_unicode_version(uint8_t version[4])
{
    version[0] = LIBUNICODE_NATIVE_UNICODE_VERSION_MAJOR;
    version[1] = LIBUNICODE_NATIVE_UNICODE_VERSION_MINOR;
    version[2] = LIBUNICODE_NATIVE_UNICODE_VERSION_PATCH;
    version[3] = 0;
}

void lre_unicode_provider_version(uint8_t version[4])
{
    /* The native provider is versioned by its generated Unicode data. */
    lre_unicode_version(version);
}

int lre_unicode_initialize(const char **error)
{
    if (error)
        *error = NULL;
    return 0;
}

#endif

#include "libunicode-es-property-grammar.inc"
