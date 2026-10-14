/* Report the selected library/data versions without changing ICU defaults. */
#include "../src/intl/icu-config.h"
#include <unicode/ucal.h>
#include <unicode/uclean.h>
#include <unicode/icudataver.h>
#include <unicode/uchar.h>
#include <unicode/ulocdata.h>
#include <stdio.h>

static void print_version(const char *name, const UVersionInfo version)
{
    printf("%s=%u.%u.%u.%u\n", name, (unsigned)version[0],
           (unsigned)version[1], (unsigned)version[2], (unsigned)version[3]);
}

int main(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UVersionInfo icu = { 0 }, unicode = { 0 }, data = { 0 }, cldr = { 0 };
    const char *tzdata;
    u_init(&status);
    u_getVersion(icu);
    u_getUnicodeVersion(unicode);
    u_getDataVersion(data, &status);
    ulocdata_getCLDRVersion(cldr, &status);
    tzdata = ucal_getTZDataVersion(&status);
    if (U_FAILURE(status) || !qjs_icu_version_supported(icu)) {
        fprintf(stderr, "Selected ICU initialization/version failed: %s\n",
                U_FAILURE(status) ? u_errorName(status) : "unsupported API version");
        return 1;
    }
    print_version("icu", icu);
    print_version("unicode", unicode);
    print_version("data", data);
    print_version("cldr", cldr);
    printf("tzdata=%s\n", tzdata);
    return 0;
}
