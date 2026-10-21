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

static int parse_break_fixture(unsigned int kind, const char *text)
{
    FILE *file = tmpfile();
    assert(file);
    assert(fputs(text, file) >= 0);
    assert(fflush(file) == 0);
    rewind(file);
    return parse_break_properties_stream(file, "authored break fixture", kind);
}

static void check_break_properties(UnicodeTableOutput *output)
{
    memset(unicode_db, 0, (CHARCODE_MAX + 1) * sizeof(*unicode_db));
    assert(!parse_break_fixture(BREAK_GCB,
        "# @missing: 0000..10FFFF; Other\n"
        "0000..0001 ; Control\n0002 ; Extend\n10000..10001 ; Regional_Indicator\n"));
    assert(!parse_break_fixture(BREAK_INCB,
        "# @missing: 0000..10FFFF; InCB; None\n"
        "0002 ; InCB; Linker\n10001 ; InCB; Consonant\n"));
    assert(!parse_break_fixture(BREAK_EP,
        "10000..10001 ; Extended_Pictographic# including reserved characters\n"));
    assert(unicode_db[0].break_properties == UNICODE_GCB_Control);
    assert(unicode_db[2].break_properties ==
        (UNICODE_GCB_Extend | (UNICODE_INCB_Linker << UNICODE_BREAK_INCB_SHIFT)));
    assert(unicode_db[0x10001].break_properties == (UNICODE_GCB_Regional_Indicator |
        (UNICODE_INCB_Consonant << UNICODE_BREAK_INCB_SHIFT) | UNICODE_BREAK_EP));
    assert(!unicode_db[3].break_properties && !unicode_db[CHARCODE_MAX].break_properties);
    /* The earlier checks read these update streams. Position them explicitly
       before emitting more tables; read-to-write switching requires a seek. */
    assert(fseek(output->source, 0, SEEK_END) == 0);
    assert(fseek(output->header, 0, SEEK_END) == 0);
    build_break_tables(output);
    assert(fflush(output->source) == 0);
    assert(read_table_word(output->source, "unicode_break_starts", 0) == 0);
    assert(read_table_word(output->source, "unicode_break_starts", 3) == 0x10000);
    assert(read_table_word(output->source, "unicode_break_values", 1) ==
        (UNICODE_GCB_Extend | (UNICODE_INCB_Linker << UNICODE_BREAK_INCB_SHIFT)));
    /* A bucket with no transition inherits the preceding value. The terminal
       index is valid too, so the reader can bound its final bucket search. */
    assert(read_table_word(output->source, "unicode_break_index", 63) == 2);
    assert(read_table_word(output->source, "unicode_break_index", 64) == 3);
    assert(read_table_word(output->source, "unicode_break_index", 1088) == 5);

    assert(parse_break_fixture(BREAK_GCB,
        "# @missing: 0000..10FFFF; Other\n0001..0002 ; Extend\n") < 0);
    assert(parse_break_fixture(BREAK_WB,
        "# @missing: 0000..10FFFF; Other\n0041 ; Unknown\n") < 0);
    assert(parse_break_fixture(BREAK_SB,
        "# @missing: 0000..10FFFF; Upper\n") < 0);
    assert(parse_break_fixture(BREAK_WB, "0041 ; ALetter\n") < 0);
    assert(parse_break_fixture(BREAK_WB,
        "# @missing: 0000..10FFFF; Other\n110000 ; ALetter\n") < 0);
    assert(parse_break_fixture(BREAK_WB,
        "# @missing: 0000..10FFFF; Other\n0042..0041 ; ALetter\n") < 0);
    assert(parse_break_fixture(BREAK_WB,
        "# @missing: 0000..10FFFF; Other\n0041 ; ALetter; unexpected\n") < 0);
    assert(parse_break_fixture(BREAK_INCB,
        "# @missing: 0000..10FFFF; InCB; None\n0041 ; InCB\n") < 0);
}

static int parse_block_fixture(const char *text)
{
    FILE *file = tmpfile();
    assert(file);
    assert(fputs(text, file) >= 0);
    assert(fflush(file) == 0);
    rewind(file);
    return parse_blocks_stream(file, "authored block fixture");
}

static void check_blocks(UnicodeTableOutput *output)
{
    static const char *const invalid[] = {
        "0000..000F; Block A\n", /* Missing neutral default. */
        "# @missing: 0000..10FFFF; Block A\n0000..000F; Block A\n",
        "# @missing: 0000..10FFFF; No_Block\n", /* No ranges. */
        "# @missing: 0000..10FFFF; No_Block\n0000..000F; No Block\n",
        "# @missing: 0000..10FFFF; No_Block\n0000..000F; A\n0000..001F; B\n",
        "# @missing: 0000..10FFFF; No_Block\n0000..000F; Latin-A\n0010..001F; latin a\n",
        "# @missing: 0000..10FFFF; No_Block\n0001..000F; A\n",
        "# @missing: 0000..10FFFF; No_Block\n0010..000F; A\n",
        "# @missing: 0000..10FFFF; No_Block\n110000..11000F; A\n",
        "# @missing: 0000..10FFFF; No_Block\n0000..000F; A; B\n"
    };
    size_t i;
    assert(!parse_block_fixture(
        "# @missing: 0000..10FFFF; No_Block\n"
        "0000..000F; Basic Latin\n10000..1001F; Supplementary-Test # comment\n"));
    assert(block_count == 2 && block_ranges[1].first == 0x10000);
    assert(!strcmp(block_ranges[1].name, "Supplementary_Test"));
    assert(fseek(output->source, 0, SEEK_END) == 0);
    assert(fseek(output->header, 0, SEEK_END) == 0);
    build_block_table(output);
    assert(fflush(output->source) == 0);
    assert(read_table_word(output->source, "unicode_block_ranges", 2) == 0x10000);
    assert(read_table_word(output->source, "unicode_block_ranges", 3) == 0x1001f);
    for (i = 0; i < countof(invalid); i++) {
        assert(parse_block_fixture(invalid[i]) < 0);
        assert(!block_ranges && !block_count);
    }
}

static void check_nfc_backward_table(void)
{
    const ComposeEntry entries[] = {
        {{0x61, 0x9be}, 0}, {{0x62, 0x11127}, 0}, {{0x63, 0x9be}, 0},
        {{0x64, 0x301}, 0}, {{0x301, 0x9d7}, 0},
    };
    UnicodeTableOutput output = {tmpfile(), tmpfile()};
    char declaration[256];
    assert(output.source && output.header);
    unicode_db[0x301].combining_class = 230;
    build_nfc_backward_table(&output, entries, countof(entries));
    assert(!fflush(output.source) && !fflush(output.header));
    assert(read_table_word(output.source, "unicode_nfc_backward_zero_cc", 0) == 0x9be);
    assert(read_table_word(output.source, "unicode_nfc_backward_zero_cc", 1) == 0x11127);
    rewind(output.header);
    assert(fgets(declaration, sizeof(declaration), output.header));
    assert(strstr(declaration, "uint32_t unicode_nfc_backward_zero_cc[2]"));
    assert(!fclose(output.source) && !fclose(output.header));
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
    check_break_properties(&output);
    check_blocks(&output);
    check_nfc_backward_table();
    assert(fclose(output.header) == 0);
    assert(fclose(file) == 0);
    free(unicode_db);
    return 0;
}
