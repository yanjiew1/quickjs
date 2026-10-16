/* Exercise the real Unicode table emitters with supplementary characters.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#define main unicode_gen_tool_main
#include "../tools/unicode_gen.c"
#undef main

static uint32_t read_first_word(FILE *file, const char *table)
{
    char line[256];
    int found = 0;
    uint32_t value;

    rewind(file);
    while (fgets(line, sizeof(line), file)) {
        char *hex;
        if (strstr(line, table))
            found = 1;
        hex = strstr(line, "0x");
        if (found && hex) {
            assert(sscanf(hex, "0x%" SCNx32, &value) == 1);
            return value;
        }
    }
    abort();
}

int main(int argc, char **argv)
{
    int decomposition = 0x4e3d;
    uint32_t packed;
    FILE *file = argc > 1 ? fopen(argv[1], "w+b") : tmpfile();

    assert(file);
    unicode_db = calloc(CHARCODE_MAX + 1, sizeof(*unicode_db));
    assert(unicode_db);
    conv_table_len = 1;
    conv_table[0] = (TableEntry){ .code = 0x10400, .len = 1, .type = RUN_TYPE_U };
    dump_case_conv_table(file);
    unicode_db[0x2f800].decomp_len = 1;
    unicode_db[0x2f800].decomp_data = &decomposition;
    unicode_db[0x2f800].is_compat = 1;
    build_decompose_table(file);
    assert(fflush(file) == 0);
    packed = read_first_word(file, "case_conv_table1");
    assert((packed >> 15) == 0x10400 && ((packed >> 8) & 127) == 1);
    packed = read_first_word(file, "unicode_decomp_table1");
    assert((packed >> 14) == 0x2f800 && ((packed >> 7) & 127) == 1);
    assert(packed & 1);
    assert(fclose(file) == 0);
    free(unicode_db);
    return 0;
}
