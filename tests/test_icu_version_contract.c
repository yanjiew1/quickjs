/* Compile with the selected SDK; this fixture does not initialize ICU. */
#include <assert.h>
#include "../src/intl/icu-config.h"

int main(void)
{
    const UVersionInfo old_system = { 60, 3, 0, 0 };
    const UVersionInfo below_floor = { 73, 2, 0, 0 };
    const UVersionInfo api_floor = { 74, 0, 0, 0 };
    const UVersionInfo validated = { 78, 3, 0, 0 };
    const UVersionInfo newer = { 79, 1, 0, 0 };
    assert(!qjs_icu_version_supported(old_system));
    assert(!qjs_icu_version_supported(below_floor));
    assert(qjs_icu_version_supported(api_floor));
    assert(qjs_icu_version_supported(validated));
    assert(qjs_icu_version_supported(newer));
    return 0;
}
