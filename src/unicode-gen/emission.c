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
#include <stdint.h>
#include <stdio.h>

#include "internal.h"

void unicode_gen_dump_byte_table(UnicodeGenOutput *out, const char *cname,
                                 const uint8_t *tab, int len)
{
    int i;
    FILE *f = out->file;

    out->total_tables++;
    out->total_table_bytes += len;
    fprintf(f, "static const uint8_t %s[%d] = {", cname, len);
    for(i = 0; i < len; i++) {
        if (i % 8 == 0)
            fprintf(f, "\n   ");
        fprintf(f, " 0x%02x,", tab[i]);
    }
    fprintf(f, "\n};\n\n");
}

void unicode_gen_dump_index_table(UnicodeGenOutput *out, const char *cname,
                                  const uint8_t *tab, int len)
{
    int i, code, offset;
    FILE *f = out->file;

    out->total_index++;
    out->total_index_bytes += len;
    fprintf(f, "static const uint8_t %s[%d] = {\n", cname, len);
    for(i = 0; i < len; i += 3) {
        code = tab[i] + (tab[i + 1] << 8) +
            ((tab[i + 2] & 0x1f) << 16);
        offset = ((i / 3) + 1) * 32 + (tab[i + 2] >> 5);
        fprintf(f, "    0x%02x, 0x%02x, 0x%02x,",
                tab[i], tab[i + 1], tab[i + 2]);
        fprintf(f, "  // %6.5X at %d%s\n", code, offset,
                i == len - 3 ? " (upper bound)" : "");
    }
    fprintf(f, "};\n\n");
}
