/* Shared ICU API contract, independent of JS Intl exposure.
 * ICU 78.3 is the validated data baseline, not the minimum SDK version.
 * Runtime Unicode/CLDR/tzdata versions describe the selected ICU data.
 */
#ifndef QUICKJS_ICU_CONFIG_H
#define QUICKJS_ICU_CONFIG_H
#ifndef CONFIG_ICU
#error This private backend header requires CONFIG_ICU
#endif
#include <unicode/utypes.h>
#include <unicode/uversion.h>
#define QJS_ICU_MIN_VERSION_MAJOR 74
#define QJS_ICU_MIN_VERSION_MINOR 0
#if U_ICU_VERSION_MAJOR_NUM < QJS_ICU_MIN_VERSION_MAJOR
#error CONFIG_ICU requires ICU4C 74 or later development APIs
#endif

static inline int qjs_icu_version_supported(const UVersionInfo version)
{
    return version[0] >= QJS_ICU_MIN_VERSION_MAJOR;
}
#endif
