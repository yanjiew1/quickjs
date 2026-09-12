#include "unicode_gen_parser.h"

void parse_unicode_data(const char *filename)
{
    FILE *f;
    char line[1024];
    char buf1[256];
    const char *p;
    int code, lc, uc, last_code;
    CCInfo *ci, *tab = unicode_db;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    last_code = 0;
    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#')
            continue;

        p = get_field(line, 0);
        if (!p)
            continue;
        code = strtoul(p, NULL, 16);
        lc = 0;
        uc = 0;

        p = get_field(line, 12);
        if (p && *p != ';') {
            uc = strtoul(p, NULL, 16);
        }

        p = get_field(line, 13);
        if (p && *p != ';') {
            lc = strtoul(p, NULL, 16);
        }
        ci = &tab[code];
        if (uc > 0 || lc > 0) {
            assert(code <= CHARCODE_MAX);
            if (uc > 0) {
                assert(ci->u_len == 0);
                ci->u_len = 1;
                ci->u_data[0] = uc;
            }
            if (lc > 0) {
                assert(ci->l_len == 0);
                ci->l_len = 1;
                ci->l_data[0] = lc;
            }
        }

        {
            int i;
            get_field_buf(buf1, sizeof(buf1), line, 2);
            i = find_name(unicode_gc_name, countof(unicode_gc_name), buf1);
            if (i < 0) {
                fprintf(stderr, "General category '%s' not found\n",
                        buf1);
                exit(1);
            }
            ci->general_category = i;
        }

        p = get_field(line, 3);
        if (p && *p != ';' && *p != '\0') {
            int cc;
            cc = strtoul(p, NULL, 0);
            if (cc != 0) {
                assert(code <= CHARCODE_MAX);
                ci->combining_class = cc;
                //                printf("%05x: %d\n", code, ci->combining_class);
            }
        }

        p = get_field(line, 5);
        if (p && *p != ';' && *p != '\0') {
            int size;
            assert(code <= CHARCODE_MAX);
            ci->is_compat = 0;
            if (*p == '<') {
                while (*p != '\0' && *p != '>')
                    p++;
                if (*p == '>')
                    p++;
                ci->is_compat = 1;
            }
            size = 0;
            for(;;) {
                while (isspace(*p))
                    p++;
                if (!isxdigit(*p))
                    break;
                add_char(&ci->decomp_data, &size, &ci->decomp_len, strtoul(p, (char **)&p, 16));
            }
#if 0
            {
                int i;
                static int count, d_count;

                printf("%05x: %c", code, ci->is_compat ? 'C': ' ');
                for(i = 0; i < ci->decomp_len; i++)
                    printf(" %05x", ci->decomp_data[i]);
                printf("\n");
                count++;
                d_count += ci->decomp_len;
                //                printf("%d %d\n", count, d_count);
            }
#endif
        }

        p = get_field(line, 9);
        if (p && *p == 'Y') {
            set_prop(code, PROP_Bidi_Mirrored, 1);
        }

        /* handle ranges */
        get_field_buf(buf1, sizeof(buf1), line, 1);
        if (strstr(buf1, " Last>")) {
            int i;
            //            printf("range: 0x%x-%0x\n", last_code, code);
            assert(ci->decomp_len == 0);
            assert(ci->script_ext_len == 0);
            for(i = last_code + 1; i < code; i++) {
                unicode_db[i] = *ci;
            }
        }
        last_code = code;
    }

    fclose(f);
}

void parse_special_casing(CCInfo *tab, const char *filename)
{
    FILE *f;
    char line[1024];
    const char *p;
    int code;
    CCInfo *ci;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#')
            continue;

        p = get_field(line, 0);
        if (!p)
            continue;
        code = strtoul(p, NULL, 16);
        assert(code <= CHARCODE_MAX);
        ci = &tab[code];

        p = get_field(line, 4);
        if (p) {
            /* locale dependent casing */
            while (isspace(*p))
                p++;
            if (*p != '#' && *p != '\0')
                continue;
        }


        p = get_field(line, 1);
        if (p && *p != ';') {
            ci->l_len = 0;
            for(;;) {
                while (isspace(*p))
                    p++;
                if (*p == ';')
                    break;
                assert(ci->l_len < CC_LEN_MAX);
                ci->l_data[ci->l_len++] = strtoul(p, (char **)&p, 16);
            }

            if (ci->l_len == 1 && ci->l_data[0] == code)
                ci->l_len = 0;
        }

        p = get_field(line, 3);
        if (p && *p != ';') {
            ci->u_len = 0;
            for(;;) {
                while (isspace(*p))
                    p++;
                if (*p == ';')
                    break;
                assert(ci->u_len < CC_LEN_MAX);
                ci->u_data[ci->u_len++] = strtoul(p, (char **)&p, 16);
            }

            if (ci->u_len == 1 && ci->u_data[0] == code)
                ci->u_len = 0;
        }
    }

    fclose(f);
}

void parse_case_folding(CCInfo *tab, const char *filename)
{
    FILE *f;
    char line[1024];
    const char *p;
    int code, status;
    CCInfo *ci;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#')
            continue;

        p = get_field(line, 0);
        if (!p)
            continue;
        code = strtoul(p, NULL, 16);
        assert(code <= CHARCODE_MAX);
        ci = &tab[code];

        p = get_field(line, 1);
        if (!p)
            continue;
        /* locale dependent casing */
        while (isspace(*p))
            p++;
        status = *p;
        if (status != 'C' && status != 'S' && status != 'F')
            continue;

        p = get_field(line, 2);
        assert(p != NULL);
        if (status == 'S') {
            /* we always select the simple case folding and assume it
             * comes after the full case folding case */
            assert(ci->f_len >= 2);
            ci->f_len = 0;
        } else {
            assert(ci->f_len == 0);
        }
        for(;;) {
            while (isspace(*p))
                p++;
            if (*p == ';')
                break;
            assert(ci->l_len < CC_LEN_MAX);
            ci->f_data[ci->f_len++] = strtoul(p, (char **)&p, 16);
        }
    }

    fclose(f);
}

void parse_composition_exclusions(const char *filename)
{
    FILE *f;
    char line[4096], *p;
    uint32_t c0;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        assert(c0 > 0 && c0 <= CHARCODE_MAX);
        unicode_db[c0].is_excluded = TRUE;
    }
    fclose(f);
}

void parse_derived_core_properties(const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q;
    uint32_t c0, c1, c;
    int i;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
        } else {
            c1 = c0;
        }
        assert(c1 <= CHARCODE_MAX);
        p += strspn(p, " \t");
        if (*p == ';') {
            p++;
            p += strspn(p, " \t");
            q = buf;
            while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t' && *p != ';') {
                if ((q - buf) < sizeof(buf) - 1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            i = find_name(unicode_prop_name,
                          countof(unicode_prop_name), buf);
            if (i < 0) {
                if (!strcmp(buf, "Grapheme_Link"))
                    goto next;
                fprintf(stderr, "Property not found: %s\n", buf);
                exit(1);
            }
            for(c = c0; c <= c1; c++) {
                set_prop(c, i, 1);
            }
next: ;
        }
    }
    fclose(f);
}

void parse_derived_norm_properties(const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q;
    uint32_t c0, c1, c;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
        } else {
            c1 = c0;
        }
        assert(c1 <= CHARCODE_MAX);
        p += strspn(p, " \t");
        if (*p == ';') {
            p++;
            p += strspn(p, " \t");
            q = buf;
            while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t') {
                if ((q - buf) < sizeof(buf) - 1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            if (!strcmp(buf, "Changes_When_NFKC_Casefolded")) {
                for(c = c0; c <= c1; c++) {
                    set_prop(c, PROP_Changes_When_NFKC_Casefolded, 1);
                }
            }
        }
    }
    fclose(f);
}

void parse_prop_list(const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q;
    uint32_t c0, c1, c;
    int i;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
        } else {
            c1 = c0;
        }
        assert(c1 <= CHARCODE_MAX);
        p += strspn(p, " \t");
        if (*p == ';') {
            p++;
            p += strspn(p, " \t");
            q = buf;
            while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t') {
                if ((q - buf) < sizeof(buf) - 1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            i = find_name(unicode_prop_name,
                          countof(unicode_prop_name), buf);
            if (i < 0) {
                fprintf(stderr, "Property not found: %s\n", buf);
                exit(1);
            }
            for(c = c0; c <= c1; c++) {
                set_prop(c, i, 1);
            }
        }
    }
    fclose(f);
}

static void add_sequence_prop(int idx, int seq_len, int *seq)
{
    int i;
    
    assert(idx < SEQUENCE_PROP_COUNT);
    switch(idx) {
    case SEQUENCE_PROP_Basic_Emoji:
        /* convert to 2 properties lists */
        if (seq_len == 1) {
            set_prop(seq[0], PROP_Basic_Emoji1, 1);
        } else if (seq_len == 2 && seq[1] == 0xfe0f) {
            set_prop(seq[0], PROP_Basic_Emoji2, 1);
        } else {
            abort();
        }
        break;
    case SEQUENCE_PROP_RGI_Emoji_Modifier_Sequence:
        assert(seq_len == 2);
        assert(is_emoji_modifier(seq[1]));
        assert(get_prop(seq[0], PROP_Emoji_Modifier_Base));
        set_prop(seq[0], PROP_RGI_Emoji_Modifier_Sequence, 1);
        break;
    case SEQUENCE_PROP_RGI_Emoji_Flag_Sequence:
        {
            int code;
            assert(seq_len == 2);
            assert(seq[0] >= 0x1F1E6 && seq[0] <= 0x1F1FF);
            assert(seq[1] >= 0x1F1E6 && seq[1] <= 0x1F1FF);
            code = (seq[0] - 0x1F1E6) * 26 + (seq[1] - 0x1F1E6);
            /* XXX: would be more compact with a simple bitmap -> 676 bits */
            set_prop(code, PROP_RGI_Emoji_Flag_Sequence, 1);
        }
        break;
    case SEQUENCE_PROP_RGI_Emoji_ZWJ_Sequence:
        re_string_add(&rgi_emoji_zwj_sequence, seq_len, (uint32_t *)seq);
        break;
    case SEQUENCE_PROP_RGI_Emoji_Tag_Sequence:
        {
            assert(seq_len >= 3);
            assert(seq[0] == 0x1F3F4);
            assert(seq[seq_len - 1] == 0xE007F);
            for(i = 1; i < seq_len - 1; i++) {
                assert(seq[i] >= 0xe0001 && seq[i] <= 0xe007e);
                dbuf_putc(&rgi_emoji_tag_sequence, seq[i] - 0xe0000);
            }
            dbuf_putc(&rgi_emoji_tag_sequence, 0);
        }
        break;
    case SEQUENCE_PROP_Emoji_Keycap_Sequence:
        assert(seq_len == 3);
        assert(seq[1] == 0xfe0f);
        assert(seq[2] == 0x20e3);
        set_prop(seq[0], PROP_Emoji_Keycap_Sequence, 1);
        break;
    default:
        assert(0);
    }
}

void parse_sequence_prop_list(const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q, *p_start;
    uint32_t c0, c1, c;
    int idx, seq_len;
    int seq[SEQ_MAX_LEN];
    
    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        p_start = p;

        /* find the sequence property name */
        p = strchr(p, ';');
        if (!p)
            continue;
        p++;
        p += strspn(p, " \t");
        q = buf;
        while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t' && *p != ';') {
            if ((q - buf) < sizeof(buf) - 1)
                *q++ = *p;
            p++;
        }
        *q = '\0';
        idx = find_name(unicode_sequence_prop_name,
                      countof(unicode_sequence_prop_name), buf);
        if (idx < 0) {
            fprintf(stderr, "Property not found: %s\n", buf);
            exit(1);
        }
        
        p = p_start;
        c0 = strtoul(p, (char **)&p, 16);
        assert(c0 <= CHARCODE_MAX);
        
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
            assert(c1 <= CHARCODE_MAX);
            for(c = c0; c <= c1; c++) {
                seq[0] = c;
                add_sequence_prop(idx, 1, seq);
            }
        } else {
            seq_len = 0;
            seq[seq_len++] = c0;
            for(;;) {
                while (isspace(*p))
                    p++;
                if (*p == ';' || *p == '\0')
                    break;
                c0 = strtoul(p, (char **)&p, 16);
                assert(c0 <= CHARCODE_MAX);
                assert(seq_len < countof(seq));
                seq[seq_len++] = c0;
            }
            add_sequence_prop(idx, seq_len, seq);
        }
    }
    fclose(f);
}

void parse_scripts(const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q;
    uint32_t c0, c1, c;
    int i;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
        } else {
            c1 = c0;
        }
        assert(c1 <= CHARCODE_MAX);
        p += strspn(p, " \t");
        if (*p == ';') {
            p++;
            p += strspn(p, " \t");
            q = buf;
            while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t') {
                if ((q - buf) < sizeof(buf) - 1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            i = find_name(unicode_script_name,
                          countof(unicode_script_name), buf);
            if (i < 0) {
                fprintf(stderr, "Unknown script: '%s'\n", buf);
                exit(1);
            }
            for(c = c0; c <= c1; c++)
                unicode_db[c].script = i;
        }
    }
    fclose(f);
}

void parse_script_extensions(const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q;
    uint32_t c0, c1, c;
    int i;
    uint8_t script_ext[255];
    int script_ext_len;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
        } else {
            c1 = c0;
        }
        assert(c1 <= CHARCODE_MAX);
        p += strspn(p, " \t");
        script_ext_len = 0;
        if (*p == ';') {
            p++;
            for(;;) {
                p += strspn(p, " \t");
                q = buf;
                while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t') {
                    if ((q - buf) < sizeof(buf) - 1)
                        *q++ = *p;
                    p++;
                }
                *q = '\0';
                if (buf[0] == '\0')
                    break;
                i = find_name(unicode_script_short_name,
                              countof(unicode_script_short_name), buf);
                if (i < 0) {
                    fprintf(stderr, "Script not found: %s\n", buf);
                    exit(1);
                }
                assert(script_ext_len < sizeof(script_ext));
                script_ext[script_ext_len++] = i;
            }
            for(c = c0; c <= c1; c++) {
                CCInfo *ci = &unicode_db[c];
                ci->script_ext_len = script_ext_len;
                ci->script_ext = malloc(sizeof(ci->script_ext[0]) * script_ext_len);
                for(i = 0; i < script_ext_len; i++)
                    ci->script_ext[i] = script_ext[i];
            }
        }
    }
    fclose(f);
}
