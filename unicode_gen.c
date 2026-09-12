/*
 * Unicode table generator for QuickJS
 * 
 * Copyright (c) 2017-2023 Fabrice Bellard
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
#include "src/unicode_gen/unicode_gen_common.h"
#include "src/unicode_gen/unicode_gen_parser.h"
#include "src/unicode_gen/unicode_gen_case.h"
#include "src/unicode_gen/unicode_gen_prop.h"
#include "src/unicode_gen/unicode_gen_norm.h"

int main(int argc, char *argv[])
{
    const char *unicode_db_path, *outfilename;
    char filename[1024];
    int arg = 1;

    if (arg >= argc || (!strcmp(argv[arg], "-h") || !strcmp(argv[arg], "--help"))) {
        printf("usage: %s PATH [OUTPUT]\n"
               "  PATH    path to the Unicode database directory\n"
               "  OUTPUT  name of the output file.  If omitted, a self test is performed\n"
               "          using the files from the Unicode library\n"
               , argv[0]);
        return 1;
    }
    unicode_db_path = argv[arg++];
    outfilename = NULL;
    if (arg < argc)
        outfilename = argv[arg++];

    unicode_db = mallocz(sizeof(unicode_db[0]) * (CHARCODE_MAX + 1));
    re_string_list_init(&rgi_emoji_zwj_sequence);
    dbuf_init(&rgi_emoji_tag_sequence);

    snprintf(filename, sizeof(filename), "%s/UnicodeData.txt", unicode_db_path);
    parse_unicode_data(filename);

    snprintf(filename, sizeof(filename), "%s/SpecialCasing.txt", unicode_db_path);
    parse_special_casing(unicode_db, filename);

    snprintf(filename, sizeof(filename), "%s/CaseFolding.txt", unicode_db_path);
    parse_case_folding(unicode_db, filename);

    snprintf(filename, sizeof(filename), "%s/CompositionExclusions.txt", unicode_db_path);
    parse_composition_exclusions(filename);

    snprintf(filename, sizeof(filename), "%s/DerivedCoreProperties.txt", unicode_db_path);
    parse_derived_core_properties(filename);

    snprintf(filename, sizeof(filename), "%s/DerivedNormalizationProps.txt", unicode_db_path);
    parse_derived_norm_properties(filename);

    snprintf(filename, sizeof(filename), "%s/PropList.txt", unicode_db_path);
    parse_prop_list(filename);

    snprintf(filename, sizeof(filename), "%s/Scripts.txt", unicode_db_path);
    parse_scripts(filename);

    snprintf(filename, sizeof(filename), "%s/ScriptExtensions.txt", unicode_db_path);
    parse_script_extensions(filename);

    snprintf(filename, sizeof(filename), "%s/emoji-data.txt", unicode_db_path);
    parse_prop_list(filename);

    snprintf(filename, sizeof(filename), "%s/emoji-sequences.txt", unicode_db_path);
    parse_sequence_prop_list(filename);

    snprintf(filename, sizeof(filename), "%s/emoji-zwj-sequences.txt", unicode_db_path);
    parse_sequence_prop_list(filename);

    build_conv_table(unicode_db);

#ifdef DUMP_CASE_FOLDING_SPECIAL_CASES
    dump_case_folding_special_cases(unicode_db);
#endif

    if (!outfilename) {
#ifdef USE_TEST
        check_case_conv();
        check_flags();
        check_decompose_table();
        check_compose_table();
        check_cc_table();
        snprintf(filename, sizeof(filename), "%s/NormalizationTest.txt", unicode_db_path);
        normalization_test(filename);
#else
        fprintf(stderr, "Tests are not compiled\n");
        exit(1);
#endif
    } else {
        FILE *fo = fopen(outfilename, "wb");

        if (!fo) {
            perror(outfilename);
            exit(1);
        }
        fprintf(fo,
                "/* Compressed unicode tables */\n"
                "/* Automatically generated file - do not edit */\n"
                "\n"
                "#include <stdint.h>\n"
                "\n");
        dump_case_conv_table(fo);
        compute_internal_props();
        build_flags_tables(fo);
        fprintf(fo, "#ifdef CONFIG_ALL_UNICODE\n\n");
        build_cc_table(fo);
        build_decompose_table(fo);
        build_general_category_table(fo);
        build_script_table(fo);
        build_script_ext_table(fo);
        build_prop_list_table(fo);
        build_sequence_prop_list_table(fo);
        fprintf(fo, "#endif /* CONFIG_ALL_UNICODE */\n");
        fprintf(fo, "/* %u tables / %u bytes, %u index / %u bytes */\n",
                total_tables, total_table_bytes, total_index, total_index_bytes);
        fclose(fo);
    }
    re_string_list_free(&rgi_emoji_zwj_sequence);
    return 0;
}
