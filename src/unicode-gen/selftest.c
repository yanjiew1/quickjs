/*
 * Generation of Unicode tables
 *
 * Copyright (c) 2017-2018 Fabrice Bellard
 * Copyright (c) 2017-2018 Charlie Gordon
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>

#include "internal.h"
#include "../../libunicode.h"
#include "../libunicode/internal-test.h"

static int tabcmp(const int *tab1, const int *tab2, int n)
{
    int i;
    for(i = 0; i < n; i++) {
        if (tab1[i] != tab2[i])
            return -1;
    }
    return 0;
}

static void dump_str(const char *str, const int *buf, int len)
{
    int i;
    printf("%s=", str);
    for(i = 0; i < len; i++)
        printf(" %05x", buf[i]);
    printf("\n");
}
static int check_conv(uint32_t *res, uint32_t c, int conv_type)
{
    return lre_case_conv(res, c, conv_type);
}

static void check_case_conv(UnicodeGenState *s)
{
    CCInfo *tab = s->db;
    uint32_t res[3];
    int l, error;
    CCInfo ci_s, *ci1, *ci = &ci_s;
    int code;

    for(code = 0; code <= CHARCODE_MAX; code++) {
        ci1 = &tab[code];
        *ci = *ci1;
        if (ci->l_len == 0) {
            ci->l_len = 1;
            ci->l_data[0] = code;
        }
        if (ci->u_len == 0) {
            ci->u_len = 1;
            ci->u_data[0] = code;
        }
        if (ci->f_len == 0) {
            ci->f_len = 1;
            ci->f_data[0] = code;
        }

        error = 0;
        l = check_conv(res, code, 0);
        if (l != ci->u_len || tabcmp((int *)res, ci->u_data, l)) {
            printf("ERROR: L\n");
            error++;
        }
        l = check_conv(res, code, 1);
        if (l != ci->l_len || tabcmp((int *)res, ci->l_data, l)) {
            printf("ERROR: U\n");
            error++;
        }
        l = check_conv(res, code, 2);
        if (l != ci->f_len || tabcmp((int *)res, ci->f_data, l)) {
            printf("ERROR: F\n");
            error++;
        }
        if (error) {
            unicode_gen_dump_cc_info(ci, code);
            exit(1);
        }
    }
}

#ifdef PROFILE
static int64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}
#endif


static void check_flags(UnicodeGenState *s)
{
    int c;
    BOOL flag_ref, flag;
    for(c = 0; c <= CHARCODE_MAX; c++) {
        flag_ref = unicode_gen_get_prop(s, c, PROP_Cased);
        flag = !!lre_is_cased(c);
        if (flag != flag_ref) {
            printf("ERROR: c=%05x cased=%d ref=%d\n",
                   c, flag, flag_ref);
            exit(1);
        }

        flag_ref = unicode_gen_get_prop(s, c, PROP_Case_Ignorable);
        flag = !!lre_is_case_ignorable(c);
        if (flag != flag_ref) {
            printf("ERROR: c=%05x case_ignorable=%d ref=%d\n",
                   c, flag, flag_ref);
            exit(1);
        }

        flag_ref = unicode_gen_get_prop(s, c, PROP_ID_Start);
        flag = !!lre_is_id_start(c);
        if (flag != flag_ref) {
            printf("ERROR: c=%05x id_start=%d ref=%d\n",
                   c, flag, flag_ref);
            exit(1);
        }

        flag_ref = unicode_gen_get_prop(s, c, PROP_ID_Continue);
        flag = !!lre_is_id_continue(c);
        if (flag != flag_ref) {
            printf("ERROR: c=%05x id_cont=%d ref=%d\n",
                   c, flag, flag_ref);
            exit(1);
        }
    }
#ifdef PROFILE
    {
        int64_t ti, count;
        ti = get_time_ns();
        count = 0;
        for(c = 0x20; c <= 0xffff; c++) {
            flag_ref = unicode_gen_get_prop(s, c, PROP_ID_Start);
            flag = !!lre_is_id_start(c);
            assert(flag == flag_ref);
            count++;
        }
        ti = get_time_ns() - ti;
        printf("flags time=%0.1f ns/char\n",
               (double)ti / count);
    }
#endif
}

static void check_decompose_table(UnicodeGenState *s)
{
    int c;
    CCInfo *ci;
    int res[UNICODE_DECOMP_LEN_MAX], *ref;
    int len, ref_len, is_compat;

    for(is_compat = 0; is_compat <= 1; is_compat++) {
        for(c = 0; c < CHARCODE_MAX; c++) {
            ci = &s->db[c];
            ref_len = ci->decomp_len;
            ref = ci->decomp_data;
            if (!is_compat && ci->is_compat) {
                ref_len = 0;
            }
            len = qjs_unicode_test_decomp_char((uint32_t *)res, c, is_compat);
            if (len != ref_len ||
                tabcmp(res, ref, ref_len) != 0) {
                printf("ERROR c=%05x compat=%d\n", c, is_compat);
                dump_str("res", res, len);
                dump_str("ref", ref, ref_len);
                exit(1);
            }
        }
    }
}

static void check_compose_table(UnicodeGenState *s)
{
    int i, p;
    /* XXX: we don't test all the cases */

    for(i = 0; i <= CHARCODE_MAX; i++) {
        CCInfo *ci = &s->db[i];
        if (ci->decomp_len == 2 && !ci->is_compat &&
            !ci->is_excluded) {
            p = qjs_unicode_test_compose_pair(ci->decomp_data[0],
                                              ci->decomp_data[1]);
            if (p != i) {
                printf("ERROR compose: c=%05x %05x -> %05x ref=%05x\n",
                       ci->decomp_data[0], ci->decomp_data[1], p, i);
                exit(1);
            }
        }
    }



}

static void check_str(const char *msg, int num, const int *in_buf, int in_len,
                      const int *buf1, int len1,
                      const int *buf2, int len2)
{
    if (len1 != len2 || tabcmp(buf1, buf2, len1) != 0) {
        printf("%d: ERROR %s:\n", num, msg);
        dump_str(" in", in_buf, in_len);
        dump_str("res", buf1, len1);
        dump_str("ref", buf2, len2);
        exit(1);
    }
}

static void check_cc_table(UnicodeGenState *s)
{
    int cc, cc_ref, c;

    for(c = 0; c <= CHARCODE_MAX; c++) {
        cc_ref = s->db[c].combining_class;
        cc = qjs_unicode_test_get_cc(c);
        if (cc != cc_ref) {
            printf("ERROR: c=%04x cc=%d cc_ref=%d\n",
                   c, cc, cc_ref);
            exit(1);
        }
    }
#ifdef PROFILE
    {
        int64_t ti, count;

        ti = get_time_ns();
        count = 0;
        /* only do it on meaningful chars */
        for(c = 0x20; c <= 0xffff; c++) {
            cc_ref = s->db[c].combining_class;
            cc = qjs_unicode_test_get_cc(c);
            count++;
        }
        ti = get_time_ns() - ti;
        printf("cc time=%0.1f ns/char\n",
               (double)ti / count);
    }
#endif
}

static void normalization_test(const char *filename)
{
    FILE *f;
    char line[4096], *p;
    int *in_str, *nfc_str, *nfd_str, *nfkc_str, *nfkd_str;
    int in_len, nfc_len, nfd_len, nfkc_len, nfkd_len;
    int *buf, buf_len, pos;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }
    pos = 0;
    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        pos++;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@')
            continue;
        in_str = unicode_gen_get_field_str(&in_len, p, 0);
        nfc_str = unicode_gen_get_field_str(&nfc_len, p, 1);
        nfd_str = unicode_gen_get_field_str(&nfd_len, p, 2);
        nfkc_str = unicode_gen_get_field_str(&nfkc_len, p, 3);
        nfkd_str = unicode_gen_get_field_str(&nfkd_len, p, 4);

        //        dump_str("in", in_str, in_len);

        buf_len = unicode_normalize((uint32_t **)&buf, (uint32_t *)in_str, in_len, UNICODE_NFD, NULL, NULL);
        check_str("nfd", pos, in_str, in_len, buf, buf_len, nfd_str, nfd_len);
        free(buf);

        buf_len = unicode_normalize((uint32_t **)&buf, (uint32_t *)in_str, in_len, UNICODE_NFKD, NULL, NULL);
        check_str("nfkd", pos, in_str, in_len, buf, buf_len, nfkd_str, nfkd_len);
        free(buf);

        buf_len = unicode_normalize((uint32_t **)&buf, (uint32_t *)in_str, in_len, UNICODE_NFC, NULL, NULL);
        check_str("nfc", pos, in_str, in_len, buf, buf_len, nfc_str, nfc_len);
        free(buf);

        buf_len = unicode_normalize((uint32_t **)&buf, (uint32_t *)in_str, in_len, UNICODE_NFKC, NULL, NULL);
        check_str("nfkc", pos, in_str, in_len, buf, buf_len, nfkc_str, nfkc_len);
        free(buf);

        free(in_str);
        free(nfc_str);
        free(nfd_str);
        free(nfkc_str);
        free(nfkd_str);
    }
    fclose(f);
}

void unicode_gen_run_self_tests(UnicodeGenState *s, const char *db_path)
{
    char filename[1024];

    unicode_gen_check_case_tables(s);
    check_case_conv(s);
    check_flags(s);
    check_decompose_table(s);
    check_compose_table(s);
    check_cc_table(s);
    snprintf(filename, sizeof(filename), "%s/NormalizationTest.txt", db_path);
    normalization_test(filename);
}
