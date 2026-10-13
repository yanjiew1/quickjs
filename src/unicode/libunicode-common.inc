/*
 * Unicode utilities
 *
 * Copyright (c) 2017-2018 Fabrice Bellard
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
/* character range */

static __maybe_unused void cr_dump(CharRange *cr)
{
    int i;
    for(i = 0; i < cr->len; i++)
        printf("%d: 0x%04x\n", i, cr->points[i]);
}

static void *cr_default_realloc(void *opaque, void *ptr, size_t size)
{
    (void)opaque;
    if (!size) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

void cr_init(CharRange *cr, void *mem_opaque, DynBufReallocFunc *realloc_func)
{
    cr->len = cr->size = 0;
    cr->points = NULL;
    cr->mem_opaque = mem_opaque;
    cr->realloc_func = realloc_func ? realloc_func : cr_default_realloc;
}

void cr_free(CharRange *cr)
{
    cr->realloc_func(cr->mem_opaque, cr->points, 0);
}

int cr_realloc(CharRange *cr, int size)
{
    int new_size;
    uint32_t *new_buf;

    if (size < 0 || (size_t)size > SIZE_MAX / sizeof(cr->points[0]))
        return -1;
    if (size > cr->size) {
        new_size = cr->size <= INT_MAX / 3 ? cr->size * 3 / 2 : size;
        new_size = max_int(size, new_size);
        new_buf = cr->realloc_func(cr->mem_opaque, cr->points,
                                   new_size * sizeof(cr->points[0]));
        if (!new_buf)
            return -1;
        cr->points = new_buf;
        cr->size = new_size;
    }
    return 0;
}

int cr_copy(CharRange *cr, const CharRange *cr1)
{
    if (cr_realloc(cr, cr1->len))
        return -1;
    if (cr1->len != 0)
        memcpy(cr->points, cr1->points, sizeof(cr->points[0]) * cr1->len);
    cr->len = cr1->len;
    return 0;
}

/* merge consecutive intervals and remove empty intervals */
static void cr_compress(CharRange *cr)
{
    int i, j, k, len;
    uint32_t *pt;

    pt = cr->points;
    len = cr->len;
    i = 0;
    j = 0;
    k = 0;
    while ((i + 1) < len) {
        if (pt[i] == pt[i + 1]) {
            /* empty interval */
            i += 2;
        } else {
            j = i;
            while ((j + 3) < len && pt[j + 1] == pt[j + 2])
                j += 2;
            /* just copy */
            pt[k] = pt[i];
            pt[k + 1] = pt[j + 1];
            k += 2;
            i = j + 2;
        }
    }
    cr->len = k;
}

/* union or intersection */
int cr_op(CharRange *cr, const uint32_t *a_pt, int a_len,
          const uint32_t *b_pt, int b_len, int op)
{
    int a_idx, b_idx, is_in;
    uint32_t v;

    a_idx = 0;
    b_idx = 0;
    for(;;) {
        /* get one more point from a or b in increasing order */
        if (a_idx < a_len && b_idx < b_len) {
            if (a_pt[a_idx] < b_pt[b_idx]) {
                goto a_add;
            } else if (a_pt[a_idx] == b_pt[b_idx]) {
                v = a_pt[a_idx];
                a_idx++;
                b_idx++;
            } else {
                goto b_add;
            }
        } else if (a_idx < a_len) {
        a_add:
            v = a_pt[a_idx++];
        } else if (b_idx < b_len) {
        b_add:
            v = b_pt[b_idx++];
        } else {
            break;
        }
        /* add the point if the in/out status changes */
        switch(op) {
        case CR_OP_UNION:
            is_in = (a_idx & 1) | (b_idx & 1);
            break;
        case CR_OP_INTER:
            is_in = (a_idx & 1) & (b_idx & 1);
            break;
        case CR_OP_XOR:
            is_in = (a_idx & 1) ^ (b_idx & 1);
            break;
        case CR_OP_SUB:
            is_in = (a_idx & 1) & ((b_idx & 1) ^ 1);
            break;
        default:
            abort();
        }
        if (is_in != (cr->len & 1)) {
            if (cr_add_point(cr, v))
                return -1;
        }
    }
    cr_compress(cr);
    return 0;
}

int cr_op1(CharRange *cr, const uint32_t *b_pt, int b_len, int op)
{
    CharRange a = *cr;
    int ret;
    cr->len = 0;
    cr->size = 0;
    cr->points = NULL;
    ret = cr_op(cr, a.points, a.len, b_pt, b_len, op);
    cr_free(&a);
    return ret;
}

int cr_invert(CharRange *cr)
{
    int len;
    len = cr->len;
    if (cr_realloc(cr, len + 2))
        return -1;
    memmove(cr->points + 1, cr->points, len * sizeof(cr->points[0]));
    cr->points[0] = 0;
    cr->points[len + 1] = UINT32_MAX;
    cr->len = len + 2;
    cr_compress(cr);
    return 0;
}

static int point_cmp(const void *p1, const void *p2, void *arg)
{
    uint32_t v1 = *(uint32_t *)p1;
    uint32_t v2 = *(uint32_t *)p2;
    (void)arg;
    return (v1 > v2) - (v1 < v2);
}

static void cr_sort_and_remove_overlap(CharRange *cr)
{
    uint32_t start, end, start1, end1, i, j;

    if (!cr->len)
        return;

    /* the resulting ranges are not necessarily sorted and may overlap */
    rqsort(cr->points, cr->len / 2, sizeof(cr->points[0]) * 2, point_cmp, NULL);
    j = 0;
    for(i = 0; i < cr->len; ) {
        start = cr->points[i];
        end = cr->points[i + 1];
        i += 2;
        while (i < cr->len) {
            start1 = cr->points[i];
            end1 = cr->points[i + 1];
            if (start1 > end) {
                /* |------|
                 *           |-------| */
                break;
            } else if (end1 <= end) {
                /* |------|
                 *    |--| */
                i += 2;
            } else {
                /* |------|
                 *     |-------| */
                end = end1;
                i += 2;
            }
        }
        cr->points[j] = start;
        cr->points[j + 1] = end;
        j += 2;
    }
    cr->len = j;
}

#define S  UNICODE_C_SPACE
#define D  UNICODE_C_DIGIT
#define X  UNICODE_C_XDIGIT
#define U  UNICODE_C_UPPER
#define L  UNICODE_C_LOWER
#define _  UNICODE_C_UNDER
#define d  UNICODE_C_DOLLAR

uint8_t const lre_ctype_bits[256] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, S, S, S, S, S, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    S, 0, 0, 0, d, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    X|D, X|D, X|D, X|D, X|D, X|D, X|D, X|D,
    X|D, X|D, 0, 0, 0, 0, 0, 0,

    0, X|U, X|U, X|U, X|U, X|U, X|U, U,
    U, U, U, U, U, U, U, U,
    U, U, U, U, U, U, U, U,
    U, U, U, 0, 0, 0, 0, _,

    0, X|L, X|L, X|L, X|L, X|L, X|L, L,
    L, L, L, L, L, L, L, L,
    L, L, L, L, L, L, L, L,
    L, L, L, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    S, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

#undef S
#undef D
#undef X
#undef U
#undef L
#undef _
#undef d
