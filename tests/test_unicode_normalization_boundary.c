/* NFC prefix boundaries and per-codepoint NFD quick checks.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/cutils/cutils.h"
#include "../src/unicode/libunicode.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_boundaries(void)
{
    const uint32_t yes[] = {0, 'a', 0x212b, 0x1f82, 0xac00, 0xd7a3,
                            0xfb7, 0xf77, 0x11a7, 0x1176, 0x11c3};
    const uint32_t no[] = {0x301, 0x344, 0xf73, 0x9be, 0x113c5, 0x16121, 0x16d68,
                           0x1161, 0x1175, 0x11a8, 0x11c2, 0xd800, 0xdfff,
                           0x110000, UINT32_MAX};
    size_t i;
    for (i = 0; i < countof(yes); i++)
        assert(unicode_nfc_boundary_before(yes[i]));
    for (i = 0; i < countof(no); i++)
        assert(!unicode_nfc_boundary_before(no[i]));
}

static void test_nfd_quick_check(void)
{
    const uint32_t yes[] = {0, 'a', 0x80, 0xa0, 0x301, 0x327, 0x1100, 0x1161,
                            0x11a8, 0xfb00, 0x20000, 0x10ffff, 0xd800, 0xdfff};
    const uint32_t no[] = {0xc0, 0xc5, 0x212b, 0x344, 0xac00, 0xd7a3, 0xf900,
                           0x1d15e, 0x110000, UINT32_MAX};
    size_t i;
    for (i = 0; i < countof(yes); i++)
        assert(unicode_nfd_quick_check(yes[i]));
    for (i = 0; i < countof(no); i++)
        assert(!unicode_nfd_quick_check(no[i]));
}

static FILE *open_source(const char *directory, const char *name)
{
    char path[4096];
    int length = snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE *file;
    assert(length > 0 && (size_t)length < sizeof(path));
    file = fopen(path, "rb");
    assert(file);
    return file;
}

/* This optional CI oracle reads source CCC/decomposition and NFC/NFD_Quick_Check,
   independently of the compressed tables and composition-pair generator. */
static void test_source(const char *directory)
{
    uint8_t *ccc = calloc(0x110000, 1), *qc = calloc(0x110000, 1), *nfd_no = calloc(0x110000, 1);
    uint32_t *first = calloc(0x110000, sizeof(*first)), code, records = 0, maybes = 0;
    FILE *file = open_source(directory, "UnicodeData.txt");
    char line[8192];
    assert(ccc && qc && first && nfd_no);
    while (fgets(line, sizeof(line), file)) {
        char *fields[15], *at = line, *separator;
        unsigned int i;
        assert(strchr(line, '\n') || feof(file));
        for (i = 0; i < 15; i++) {
            fields[i] = at;
            if (i < 14) {
                separator = strchr(at, ';');
                assert(separator);
                *separator = 0;
                at = separator + 1;
            }
        }
        code = strtoul(fields[0], NULL, 16);
        assert(code <= 0x10ffff);
        i = strtoul(fields[3], NULL, 10);
        assert(i <= 255);
        ccc[code] = i;
        /* Unicode18's algorithmic First/Last ranges have CCC0 and no explicit
           decomposition. Their default oracle entries are therefore complete. */
        if (strstr(fields[1], ", First>") || strstr(fields[1], ", Last>"))
            assert(!i && !*fields[5]);
        if (*fields[5] && *fields[5] != '<') {
            uint32_t value = strtoul(fields[5], NULL, 16);
            assert(value <= 0x10ffff);
            first[code] = value + 1;
        }
        records++;
    }
    assert(records && !ferror(file) && !fclose(file));
    file = open_source(directory, "DerivedNormalizationProps.txt");
    while (fgets(line, sizeof(line), file)) {
        char *separator, *at;
        unsigned int start, end;
        int nfd;
        if (*line == '#' || !(separator = strchr(line, ';')))
            continue;
        *separator = 0;
        at = separator + 1;
        while (*at == ' ' || *at == '\t')
            at++;
        nfd = !strncmp(at, "NFD_QC", 6);
        if ((!nfd && strncmp(at, "NFC_QC", 6)) || (at[6] != ';' && at[6] != ' ' && at[6] != '\t'))
            continue;
        at = strchr(at, ';');
        assert(at);
        do { at++; } while (*at == ' ' || *at == '\t');
        assert(*at == 'N' || *at == 'M');
        assert(!nfd || *at == 'N');
        if (sscanf(line, "%x..%x", &start, &end) != 2) {
            assert(sscanf(line, "%x", &start) == 1);
            end = start;
        }
        assert(start <= end && end <= 0x10ffff);
        for (code = start; code <= end; code++) {
            if (nfd) {
                nfd_no[code] = 1;
            } else {
                qc[code] = *at == 'M' ? 1 : 2;
                maybes += *at == 'M';
            }
        }
    }
    assert(maybes && !ferror(file) && !fclose(file));
    for (code = 0; code <= 0x10ffff; code++) {
        uint32_t lead = code;
        int expected;
        if (unicode_nfd_quick_check(code) != !nfd_no[code]) {
            fprintf(stderr, "NFD quick-check mismatch at U+%04x\n", code);
            abort();
        }
        if (code >= 0xd800 && code <= 0xdfff) {
            expected = 0;
        } else {
            for (;;) {
                if (lead >= 0xac00 && lead < 0xd7a4) {
                    lead = 0x1100 + (lead - 0xac00) / 588;
                    break;
                }
                if (!first[lead])
                    break;
                lead = first[lead] - 1;
            }
            assert(qc[lead] != 2); /* A fully decomposed lead is never NFC_QC=No. */
            expected = !ccc[lead] && qc[lead] != 1;
        }
        if (unicode_nfc_boundary_before(code) != expected) {
            fprintf(stderr, "NFC boundary mismatch at U+%04x\n", code);
            abort();
        }
    }
    free(first);
    free(nfd_no);
    free(qc);
    free(ccc);
    puts("NFC boundary / NFD quick-check oracle: all Unicode18 code points passed");
}

int main(int argc, char **argv)
{
    assert(argc == 1 || argc == 2);
    test_boundaries();
    test_nfd_quick_check();
    if (argc == 2)
        test_source(argv[1]);
    return 0;
}
