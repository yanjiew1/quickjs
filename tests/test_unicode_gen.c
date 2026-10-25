/* Exercise the real Unicode table emitters with supplementary characters.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#define main unicode_gen_tool_main
#include "../tools/unicode_gen.c"
#undef main

static uint32_t read_table_word(FILE *file, const char *table, int index)
{
    char line[256];
    int found = 0;
    uint32_t value;

    rewind(file);
    while (fgets(line, sizeof(line), file)) {
        char *hex;
        if (strstr(line, table))
            found = 1;
        hex = line;
        while (found && (hex = strstr(hex, "0x")) != NULL) {
            if (!index--) {
                assert(sscanf(hex, "0x%" SCNx32, &value) == 1);
                return value;
            }
            hex += 2;
        }
    }
    abort();
}

static void check_declarations(FILE *file)
{
    char line[256];
    int conversion = 0, folding = 0, decomposition = 0;

    assert(fflush(file) == 0);
    rewind(file);
    while (fgets(line, sizeof(line), file)) {
        assert(!strchr(line, '=') && !strchr(line, '{'));
        conversion |= strstr(line, "case_conv_table1[2];") != NULL;
        folding |= strstr(line, "case_conv_simple_folding[1][2];") != NULL;
        decomposition |= strstr(line, "unicode_decomp_table1[1];") != NULL;
    }
    assert(conversion && folding && decomposition);
}

int main(int argc, char **argv)
{
    int decomposition = 0x4e3d;
    uint32_t packed;
    UnicodeTableOutput output;
    FILE *file = argc > 1 ? fopen(argv[1], "w+b") : tmpfile();

    assert(file);
    output.source = file;
    output.header = tmpfile();
    assert(output.header);
    unicode_db = calloc(CHARCODE_MAX + 1, sizeof(*unicode_db));
    assert(unicode_db);
    conv_table_len = 2;
    conv_table[0] = (TableEntry){ .code = 0x1f88, .len = 1, .type = RUN_TYPE_UF_EXT2 };
    unicode_db[0x1f88].f_len = 1;
    unicode_db[0x1f88].f_data[0] = 0x1f80;
    unicode_db[0x1f88].full_f_len = 2;
    conv_table[1] = (TableEntry){ .code = 0x10400, .len = 1, .type = RUN_TYPE_U };
    dump_case_conv_table(&output);
    unicode_db[0x2f800].decomp_len = 1;
    unicode_db[0x2f800].decomp_data = &decomposition;
    unicode_db[0x2f800].is_compat = 1;
    build_decompose_table(&output);
    assert(fflush(file) == 0);
    packed = read_table_word(file, "case_conv_table1", 1);
    assert((packed >> 15) == 0x10400 && ((packed >> 8) & 127) == 1);
    packed = read_table_word(file, "unicode_decomp_table1", 0);
    assert((packed >> 14) == 0x2f800 && ((packed >> 7) & 127) == 1);
    assert(packed & 1);
    check_declarations(output.header);
    assert(fclose(output.header) == 0);
    assert(fclose(file) == 0);
    free(unicode_db);
    return 0;
}
