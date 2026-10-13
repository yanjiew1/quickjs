/* Record active versions before the full unit can fail or abort. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cutils.h"
#include "libunicode.h"

int main(int argc, char **argv)
{
    const char *error = NULL;
    uint8_t data[4], provider[4];
    assert(lre_unicode_initialize(&error) == 0 && !error);
    lre_unicode_version(data);
    lre_unicode_provider_version(provider);
    assert(data[0] != 0 && provider[0] != 0);
    if (argc > 1) {
        assert(argc == 2 && !strcmp(argv[1], "--test-args"));
        assert(!strcmp(lre_unicode_backend(), "icu") && data[0] >= 15);
        printf("--icu-unicode-major=%u\n", data[0]);
        return 0;
    }
    printf("Unicode backend=%s data=%u.%u.%u.%u provider=%u.%u.%u.%u native=%d.%d.%d\n",
           lre_unicode_backend(), data[0], data[1], data[2], data[3],
           provider[0], provider[1], provider[2], provider[3],
           LIBUNICODE_NATIVE_UNICODE_VERSION_MAJOR, LIBUNICODE_NATIVE_UNICODE_VERSION_MINOR,
           LIBUNICODE_NATIVE_UNICODE_VERSION_PATCH);
    fflush(stdout);
    return 0;
}
