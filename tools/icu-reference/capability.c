/* Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 * Public ICU decimal precision witness for the optional reference SDK. */
#include <unicode/ucal.h>
#include <unicode/uchar.h>
#include <unicode/ulocdata.h>
#include <unicode/unumberformatter.h>
#include <unicode/upluralrules.h>
#include <unicode/uversion.h>
#include <stdio.h>
#include <string.h>

static int scalar(const char *locale, UPluralType type, const char *decimal,
                  const char *expected)
{
    const UChar skeleton[] = {'p','r','e','c','i','s','i','o','n','-','u','n','l','i','m','i','t','e','d',0};
    UChar actual[16] = {0};
    char narrow[16] = {0};
    UErrorCode status = U_ZERO_ERROR;
    UPluralRules *rules = uplrules_openForType(locale, type, &status);
    UNumberFormatter *formatter = unumf_openForSkeletonAndLocale(skeleton, -1, locale, &status);
    UFormattedNumber *number = unumf_openResult(&status);
    if (U_SUCCESS(status))
        unumf_formatDecimal(formatter, decimal, (int32_t)strlen(decimal), number, &status);
    int32_t length = 0;
    if (U_SUCCESS(status))
        length = uplrules_selectFormatted(rules, number, actual, 16, &status);
    if (U_SUCCESS(status) && length > 0 && length < 16)
        for (int32_t i = 0; i < length; ++i) narrow[i] = actual[i] < 128 ? (char)actual[i] : '?';
    int failed = U_FAILURE(status) || strcmp(narrow, expected) != 0;
    printf("precision locale=%s type=%d decimal=%s expected=%s actual=%s status=%s failed=%d\n",
           locale, (int)type, decimal, expected, narrow, u_errorName(status), failed);
    if (number) unumf_closeResult(number);
    if (formatter) unumf_close(formatter);
    if (rules) uplrules_close(rules);
    return failed;
}

int main(void)
{
    UVersionInfo version;
    char text[U_MAX_VERSION_STRING_LENGTH];
    UErrorCode status = U_ZERO_ERROR;
    u_getVersion(version); u_versionToString(version, text); printf("ICU=%s\n", text);
    int failed = strcmp(text, "78.3") != 0;
    u_getUnicodeVersion(version); u_versionToString(version, text); printf("UCD=%s\n", text);
    failed += strcmp(text, "17.0") != 0;
    ulocdata_getCLDRVersion(version, &status); u_versionToString(version, text); printf("CLDR=%s\n", text);
    failed += U_FAILURE(status) || strcmp(text, "48.0") != 0;
    status = U_ZERO_ERROR;
    const char *tz = ucal_getTZDataVersion(&status);
    printf("tzdata=%s\n", tz ? tz : "<error>");
    failed += U_FAILURE(status) || !tz || strcmp(tz, "2026a") != 0;
    failed += scalar("ru", UPLURAL_TYPE_CARDINAL, "90071992547409921", "one");
    failed += scalar("ru", UPLURAL_TYPE_CARDINAL, "90071992547409922", "few");
    failed += scalar("en", UPLURAL_TYPE_ORDINAL, "90071992547409921", "one");
    failed += scalar("bs", UPLURAL_TYPE_CARDINAL, "0.90071992547409921", "one");
    printf("CAPABILITY failures=%d\n", failed);
    return failed ? 1 : 0;
}
