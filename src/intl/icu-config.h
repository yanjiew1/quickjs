/* Shared ICU backend version contract, independent of JS Intl exposure. */
#ifndef QUICKJS_ICU_CONFIG_H
#define QUICKJS_ICU_CONFIG_H
#ifndef CONFIG_ICU
#error This private backend header requires CONFIG_ICU
#endif
#include <unicode/utypes.h>
#include <unicode/uversion.h>
#if U_ICU_VERSION_MAJOR_NUM < 78 || \
    (U_ICU_VERSION_MAJOR_NUM == 78 && U_ICU_VERSION_MINOR_NUM < 3)
#error CONFIG_ICU requires ICU4C 78.3 or later
#endif
#endif
