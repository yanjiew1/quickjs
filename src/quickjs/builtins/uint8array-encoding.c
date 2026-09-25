/*
 * QuickJS Uint8Array base64/hex encoding
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
#include "../internal/base.h"
#include "../internal/string.h"
#include "../internal/object.h"
#include "typed-array.h"
#include "uint8array-encoding.h"
#include "array-buffer.h"

/* Uint8Array base64/hex (tc39 proposal-arraybuffer-base64) */

enum {
    B64_ALPHABET_BASE64 = 0,
    B64_ALPHABET_BASE64URL = 1,
};

enum {
    B64_LAST_LOOSE = 0,
    B64_LAST_STRICT = 1,
    B64_LAST_STOP_BEFORE_PARTIAL = 2,
};

static const unsigned char b64_enc[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p',
    'q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9',
    '+','/'
};

static const unsigned char b64url_enc[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p',
    'q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9',
    '-','_'
};

#define K_WS 64
#define K_ER 65

static const uint8_t b64_dec[256] = {
 [  0]=K_ER, [  1]=K_ER, [  2]=K_ER, [  3]=K_ER, [  4]=K_ER, [  5]=K_ER, [  6]=K_ER, [  7]=K_ER,
 [  8]=K_ER, [  9]=K_WS, [ 10]=K_WS, [ 11]=K_ER, [ 12]=K_WS, [ 13]=K_WS, [ 14]=K_ER, [ 15]=K_ER,
 [ 16]=K_ER, [ 17]=K_ER, [ 18]=K_ER, [ 19]=K_ER, [ 20]=K_ER, [ 21]=K_ER, [ 22]=K_ER, [ 23]=K_ER,
 [ 24]=K_ER, [ 25]=K_ER, [ 26]=K_ER, [ 27]=K_ER, [ 28]=K_ER, [ 29]=K_ER, [ 30]=K_ER, [ 31]=K_ER,
 [' ']=K_WS, ['!']=K_ER, ['"']=K_ER, ['#']=K_ER, ['$']=K_ER, ['%']=K_ER, ['&']=K_ER, [ 39]=K_ER,
 ['(']=K_ER, [')']=K_ER, ['*']=K_ER, ['+']=  62, [',']=K_ER, ['-']=K_ER, ['.']=K_ER, ['/']=  63,
 ['0']=  52, ['1']=  53, ['2']=  54, ['3']=  55, ['4']=  56, ['5']=  57, ['6']=  58, ['7']=  59,
 ['8']=  60, ['9']=  61, [':']=K_ER, [';']=K_ER, ['<']=K_ER, ['=']=K_ER, ['>']=K_ER, ['?']=K_ER,
 ['@']=K_ER, ['A']=   0, ['B']=   1, ['C']=   2, ['D']=   3, ['E']=   4, ['F']=   5, ['G']=   6,
 ['H']=   7, ['I']=   8, ['J']=   9, ['K']=  10, ['L']=  11, ['M']=  12, ['N']=  13, ['O']=  14,
 ['P']=  15, ['Q']=  16, ['R']=  17, ['S']=  18, ['T']=  19, ['U']=  20, ['V']=  21, ['W']=  22,
 ['X']=  23, ['Y']=  24, ['Z']=  25, ['[']=K_ER, [ 92]=K_ER, [']']=K_ER, ['^']=K_ER, ['_']=K_ER,
 ['`']=K_ER, ['a']=  26, ['b']=  27, ['c']=  28, ['d']=  29, ['e']=  30, ['f']=  31, ['g']=  32,
 ['h']=  33, ['i']=  34, ['j']=  35, ['k']=  36, ['l']=  37, ['m']=  38, ['n']=  39, ['o']=  40,
 ['p']=  41, ['q']=  42, ['r']=  43, ['s']=  44, ['t']=  45, ['u']=  46, ['v']=  47, ['w']=  48,
 ['x']=  49, ['y']=  50, ['z']=  51, ['{']=K_ER, ['|']=K_ER, ['}']=K_ER, ['~']=K_ER, [127]=K_ER,
 [128]=K_ER, [129]=K_ER, [130]=K_ER, [131]=K_ER, [132]=K_ER, [133]=K_ER, [134]=K_ER, [135]=K_ER,
 [136]=K_ER, [137]=K_ER, [138]=K_ER, [139]=K_ER, [140]=K_ER, [141]=K_ER, [142]=K_ER, [143]=K_ER,
 [144]=K_ER, [145]=K_ER, [146]=K_ER, [147]=K_ER, [148]=K_ER, [149]=K_ER, [150]=K_ER, [151]=K_ER,
 [152]=K_ER, [153]=K_ER, [154]=K_ER, [155]=K_ER, [156]=K_ER, [157]=K_ER, [158]=K_ER, [159]=K_ER,
 [160]=K_ER, [161]=K_ER, [162]=K_ER, [163]=K_ER, [164]=K_ER, [165]=K_ER, [166]=K_ER, [167]=K_ER,
 [168]=K_ER, [169]=K_ER, [170]=K_ER, [171]=K_ER, [172]=K_ER, [173]=K_ER, [174]=K_ER, [175]=K_ER,
 [176]=K_ER, [177]=K_ER, [178]=K_ER, [179]=K_ER, [180]=K_ER, [181]=K_ER, [182]=K_ER, [183]=K_ER,
 [184]=K_ER, [185]=K_ER, [186]=K_ER, [187]=K_ER, [188]=K_ER, [189]=K_ER, [190]=K_ER, [191]=K_ER,
 [192]=K_ER, [193]=K_ER, [194]=K_ER, [195]=K_ER, [196]=K_ER, [197]=K_ER, [198]=K_ER, [199]=K_ER,
 [200]=K_ER, [201]=K_ER, [202]=K_ER, [203]=K_ER, [204]=K_ER, [205]=K_ER, [206]=K_ER, [207]=K_ER,
 [208]=K_ER, [209]=K_ER, [210]=K_ER, [211]=K_ER, [212]=K_ER, [213]=K_ER, [214]=K_ER, [215]=K_ER,
 [216]=K_ER, [217]=K_ER, [218]=K_ER, [219]=K_ER, [220]=K_ER, [221]=K_ER, [222]=K_ER, [223]=K_ER,
 [224]=K_ER, [225]=K_ER, [226]=K_ER, [227]=K_ER, [228]=K_ER, [229]=K_ER, [230]=K_ER, [231]=K_ER,
 [232]=K_ER, [233]=K_ER, [234]=K_ER, [235]=K_ER, [236]=K_ER, [237]=K_ER, [238]=K_ER, [239]=K_ER,
 [240]=K_ER, [241]=K_ER, [242]=K_ER, [243]=K_ER, [244]=K_ER, [245]=K_ER, [246]=K_ER, [247]=K_ER,
 [248]=K_ER, [249]=K_ER, [250]=K_ER, [251]=K_ER, [252]=K_ER, [253]=K_ER, [254]=K_ER, [255]=K_ER,
};

static const uint8_t b64url_dec[256] = {
 [  0]=K_ER, [  1]=K_ER, [  2]=K_ER, [  3]=K_ER, [  4]=K_ER, [  5]=K_ER, [  6]=K_ER, [  7]=K_ER,
 [  8]=K_ER, [  9]=K_WS, [ 10]=K_WS, [ 11]=K_ER, [ 12]=K_WS, [ 13]=K_WS, [ 14]=K_ER, [ 15]=K_ER,
 [ 16]=K_ER, [ 17]=K_ER, [ 18]=K_ER, [ 19]=K_ER, [ 20]=K_ER, [ 21]=K_ER, [ 22]=K_ER, [ 23]=K_ER,
 [ 24]=K_ER, [ 25]=K_ER, [ 26]=K_ER, [ 27]=K_ER, [ 28]=K_ER, [ 29]=K_ER, [ 30]=K_ER, [ 31]=K_ER,
 [' ']=K_WS, ['!']=K_ER, ['"']=K_ER, ['#']=K_ER, ['$']=K_ER, ['%']=K_ER, ['&']=K_ER, [ 39]=K_ER,
 ['(']=K_ER, [')']=K_ER, ['*']=K_ER, ['+']=K_ER, [',']=K_ER, ['-']=  62, ['.']=K_ER, ['/']=K_ER,
 ['0']=  52, ['1']=  53, ['2']=  54, ['3']=  55, ['4']=  56, ['5']=  57, ['6']=  58, ['7']=  59,
 ['8']=  60, ['9']=  61, [':']=K_ER, [';']=K_ER, ['<']=K_ER, ['=']=K_ER, ['>']=K_ER, ['?']=K_ER,
 ['@']=K_ER, ['A']=   0, ['B']=   1, ['C']=   2, ['D']=   3, ['E']=   4, ['F']=   5, ['G']=   6,
 ['H']=   7, ['I']=   8, ['J']=   9, ['K']=  10, ['L']=  11, ['M']=  12, ['N']=  13, ['O']=  14,
 ['P']=  15, ['Q']=  16, ['R']=  17, ['S']=  18, ['T']=  19, ['U']=  20, ['V']=  21, ['W']=  22,
 ['X']=  23, ['Y']=  24, ['Z']=  25, ['[']=K_ER, [ 92]=K_ER, [']']=K_ER, ['^']=K_ER, ['_']=  63,
 ['`']=K_ER, ['a']=  26, ['b']=  27, ['c']=  28, ['d']=  29, ['e']=  30, ['f']=  31, ['g']=  32,
 ['h']=  33, ['i']=  34, ['j']=  35, ['k']=  36, ['l']=  37, ['m']=  38, ['n']=  39, ['o']=  40,
 ['p']=  41, ['q']=  42, ['r']=  43, ['s']=  44, ['t']=  45, ['u']=  46, ['v']=  47, ['w']=  48,
 ['x']=  49, ['y']=  50, ['z']=  51, ['{']=K_ER, ['|']=K_ER, ['}']=K_ER, ['~']=K_ER, [127]=K_ER,
 [128]=K_ER, [129]=K_ER, [130]=K_ER, [131]=K_ER, [132]=K_ER, [133]=K_ER, [134]=K_ER, [135]=K_ER,
 [136]=K_ER, [137]=K_ER, [138]=K_ER, [139]=K_ER, [140]=K_ER, [141]=K_ER, [142]=K_ER, [143]=K_ER,
 [144]=K_ER, [145]=K_ER, [146]=K_ER, [147]=K_ER, [148]=K_ER, [149]=K_ER, [150]=K_ER, [151]=K_ER,
 [152]=K_ER, [153]=K_ER, [154]=K_ER, [155]=K_ER, [156]=K_ER, [157]=K_ER, [158]=K_ER, [159]=K_ER,
 [160]=K_ER, [161]=K_ER, [162]=K_ER, [163]=K_ER, [164]=K_ER, [165]=K_ER, [166]=K_ER, [167]=K_ER,
 [168]=K_ER, [169]=K_ER, [170]=K_ER, [171]=K_ER, [172]=K_ER, [173]=K_ER, [174]=K_ER, [175]=K_ER,
 [176]=K_ER, [177]=K_ER, [178]=K_ER, [179]=K_ER, [180]=K_ER, [181]=K_ER, [182]=K_ER, [183]=K_ER,
 [184]=K_ER, [185]=K_ER, [186]=K_ER, [187]=K_ER, [188]=K_ER, [189]=K_ER, [190]=K_ER, [191]=K_ER,
 [192]=K_ER, [193]=K_ER, [194]=K_ER, [195]=K_ER, [196]=K_ER, [197]=K_ER, [198]=K_ER, [199]=K_ER,
 [200]=K_ER, [201]=K_ER, [202]=K_ER, [203]=K_ER, [204]=K_ER, [205]=K_ER, [206]=K_ER, [207]=K_ER,
 [208]=K_ER, [209]=K_ER, [210]=K_ER, [211]=K_ER, [212]=K_ER, [213]=K_ER, [214]=K_ER, [215]=K_ER,
 [216]=K_ER, [217]=K_ER, [218]=K_ER, [219]=K_ER, [220]=K_ER, [221]=K_ER, [222]=K_ER, [223]=K_ER,
 [224]=K_ER, [225]=K_ER, [226]=K_ER, [227]=K_ER, [228]=K_ER, [229]=K_ER, [230]=K_ER, [231]=K_ER,
 [232]=K_ER, [233]=K_ER, [234]=K_ER, [235]=K_ER, [236]=K_ER, [237]=K_ER, [238]=K_ER, [239]=K_ER,
 [240]=K_ER, [241]=K_ER, [242]=K_ER, [243]=K_ER, [244]=K_ER, [245]=K_ER, [246]=K_ER, [247]=K_ER,
 [248]=K_ER, [249]=K_ER, [250]=K_ER, [251]=K_ER, [252]=K_ER, [253]=K_ER, [254]=K_ER, [255]=K_ER,
};

static size_t b64_encode(const uint8_t *src, size_t len, char *dst,
                         const unsigned char *alpha)
{
    size_t i, j;

    for (i = 0, j = 0; i + 3 <= len; i += 3, j += 4) {
        uint32_t v = 65536*src[i] + 256*src[i + 1] + src[i + 2];
        dst[j + 0] = alpha[(v >> 18) & 63];
        dst[j + 1] = alpha[(v >> 12) & 63];
        dst[j + 2] = alpha[(v >> 6) & 63];
        dst[j + 3] = alpha[v & 63];
    }

    size_t rem = len - i;
    if (rem == 1) {
        uint32_t v = 65536*src[i];
        dst[j++] = alpha[(v >> 18) & 63];
        dst[j++] = alpha[(v >> 12) & 63];
        dst[j++] = '=';
        dst[j++] = '=';
    } else if (rem == 2) {
        uint32_t v = 65536*src[i] + 256*src[i + 1];
        dst[j++] = alpha[(v >> 18) & 63];
        dst[j++] = alpha[(v >> 12) & 63];
        dst[j++] = alpha[(v >> 6) & 63];
        dst[j++] = '=';
    }
    return j;
}

static size_t b64_skip_ws(const char *src, size_t len, size_t index,
                          const uint8_t *dec_table)
{
    while (index < len && dec_table[(unsigned char)src[index]] == K_WS)
        index++;
    return index;
}

/* Implements the FromBase64 abstract operation.
   src/src_len: the input string (must be ASCII/latin1)
   dst/max_len: output buffer
   flags: b64_flags or b64_flags_url (selects valid characters)
   last_chunk: B64_LAST_LOOSE, B64_LAST_STRICT, or B64_LAST_STOP_BEFORE_PARTIAL
   *p_read: set to number of input characters consumed
   *p_err: set to 1 on error, 0 on success
   Returns: number of bytes written to dst */
static size_t from_base64(const char *src, size_t src_len,
                          uint8_t *dst, size_t max_len,
                          const uint8_t *dec_table, int last_chunk,
                          size_t *p_read, int *p_err)
{
    size_t read = 0, written = 0;
    uint32_t v, acc = 0;
    int seen = 0;
    size_t index = 0;
    uint8_t ch;

    *p_err = 0;

    if (max_len == 0) {
        *p_read = 0;
        return 0;
    }

    for (;;) {
        if (seen == 0) {
            /* Fast path: decode complete groups of 4 valid characters.
               Breaks out on whitespace, padding, invalid chars, or capacity. */
            while (index + 4 <= src_len && written + 3 <= max_len) {
                uint32_t v0, v1, v2, v3;
                v0 = dec_table[(unsigned char)src[index]];
                v1 = dec_table[(unsigned char)src[index + 1]];
                v2 = dec_table[(unsigned char)src[index + 2]];
                v3 = dec_table[(unsigned char)src[index + 3]];
                if ((v0 | v1 | v2 | v3) >= 64)
                    break;
                v = (v0 << 18) | (v1 << 12) | (v2 << 6) | v3;
                dst[written]     = (uint8_t)(v >> 16);
                dst[written + 1] = (uint8_t)(v >> 8);
                dst[written + 2] = (uint8_t)(v);
                written += 3;
                index += 4;
            }
            read = index;

            if (written >= max_len) {
                *p_read = read;
                return written;
            }
        }

        /* Slow path: handle whitespace, padding, partial groups, capacity. */
        index = b64_skip_ws(src, src_len, index, dec_table);

        if (index == src_len) {
            if (seen > 0) {
                if (last_chunk == B64_LAST_STOP_BEFORE_PARTIAL) {
                    *p_read = read;
                    return written;
                }
                if (last_chunk == B64_LAST_STRICT) {
                    *p_err = 1;
                    return 0;
                }
                /* loose */
                if (seen == 1) {
                    *p_err = 1;
                    return 0;
                }
                break;
            }
            *p_read = src_len;
            return written;
        }

        ch = src[index++];

        if (ch == '=') {
            if (seen < 2) {
                *p_err = 1;
                return 0;
            }
            index = b64_skip_ws(src, src_len, index, dec_table);
            if (seen == 2) {
                if (index == src_len) {
                    if (last_chunk == B64_LAST_STOP_BEFORE_PARTIAL) {
                        *p_read = read;
                        return written;
                    }
                    *p_err = 1;
                    return 0;
                }
                if (src[index] == '=') {
                    index++;
                    index = b64_skip_ws(src, src_len, index, dec_table);
                } else {
                    *p_err = 1;
                    return 0;
                }
            }
            /* After padding, only whitespace is allowed */
            if (index != src_len) {
                *p_err = 1;
                return 0;
            }
            if (last_chunk == B64_LAST_STRICT) {
                uint32_t mask = (seen == 2) ? 0xF : 0x3;
                if (acc & mask) {
                    *p_err = 1;
                    return 0;
                }
            }
            break;
        }

        v = dec_table[ch];
        if (v >= 64) {
            *p_err = 1;
            return 0;
        }

        /* Check remaining capacity before committing to this group */
        {
            size_t remaining = max_len - written;
            if ((remaining == 1 && seen == 2) ||
                    (remaining == 2 && seen == 3)) {
                *p_read = read;
                return written;
            }
        }

        acc = (acc << 6) | v;
        seen++;

        if (seen == 4) {
            dst[written]     = (uint8_t)(acc >> 16);
            dst[written + 1] = (uint8_t)(acc >> 8);
            dst[written + 2] = (uint8_t)(acc);
            written += 3;
            acc = 0;
            seen = 0;
            read = index;
            if (written >= max_len) {
                *p_read = read;
                return written;
            }
        }
    }

    if (seen == 2) {
        dst[written++] = (uint8_t)(acc >> 4);
    } else if (seen == 3) {
        dst[written]     = (uint8_t)(acc >> 10);
        dst[written + 1] = (uint8_t)(acc >> 2);
        written += 2;
    }
    *p_read = src_len;
    return written;
}

/* Hex helpers */
static const char u8a_hex_digits[] = "0123456789abcdef";

static size_t u8a_hex_encode(const uint8_t *src, size_t len, char *dst)
{
    for (size_t i = 0; i < len; i++) {
        dst[i * 2]     = u8a_hex_digits[src[i] >> 4];
        dst[i * 2 + 1] = u8a_hex_digits[src[i] & 0xF];
    }
    return len * 2;
}

/* Decode hex string to bytes.
   Returns bytes written. Sets *p_read to chars consumed, *p_err on error. */
static size_t u8a_hex_decode(const char *src, size_t src_len,
                             uint8_t *dst, size_t max_len,
                             size_t *p_read, int *p_err)
{
    size_t written = 0, i = 0;
    *p_err = 0;

    if (src_len & 1) {
        *p_err = 1;
        return 0;
    }

    while (i < src_len && written < max_len) {
        int hi = from_hex(src[i]);
        int lo = from_hex(src[i + 1]);
        if (hi < 0 || lo < 0) {
            *p_err = 1;
            return 0;
        }
        dst[written++] = (uint8_t)((hi << 4) | lo);
        i += 2;
    }

    *p_read = i;
    return written;
}

/* Validate that this_val is a Uint8Array (type check only, no detach check).
   Returns the JSObject pointer or NULL on error (throws). */
static JSObject *check_uint8array(JSContext *ctx, JSValueConst this_val)
{
    JSObject *p;

    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        goto fail;
    p = JS_VALUE_GET_OBJ(this_val);
    if (p->class_id != JS_CLASS_UINT8_ARRAY)
        goto fail;
    return p;
fail:
    JS_ThrowTypeError(ctx, "not a Uint8Array");
    return NULL;
}

/* Get the data pointer and length of a Uint8Array, checking for detached
   buffers. Must be called after options are read (per spec ordering).
   Returns 0 on success, -1 on error (throws). */
static int get_uint8array_bytes(JSContext *ctx, JSObject *p,
                                uint8_t **pdata, size_t *plen)
{
    if (typed_array_is_oob(p)) {
        JS_ThrowTypeErrorArrayBufferOOB(ctx);
        *pdata = NULL; /* fail safe */
        *plen = 0;
        return -1;
    }
    *pdata = p->u.array.u.uint8_ptr;
    *plen = p->u.array.count;
    return 0;
}

/* Validate options is undefined or an object (GetOptionsObject).
   Returns 0 on success, -1 on error (throws). */
static int check_options_object(JSContext *ctx, JSValueConst options)
{
    if (JS_IsUndefined(options))
        return 0;
    if (!JS_IsObject(options)) {
        JS_ThrowTypeError(ctx, "options must be an object");
        return -1;
    }
    return 0;
}

/* Parse the 'alphabet' option from an options object.
   Returns B64_ALPHABET_BASE64 or B64_ALPHABET_BASE64URL, or -1 on error. */
static int parse_alphabet_option(JSContext *ctx, JSValueConst options)
{
    JSValue val;
    const char *str;
    int ret;

    if (JS_IsUndefined(options))
        return B64_ALPHABET_BASE64;

    val = JS_GetProperty(ctx, options, JS_ATOM_alphabet);
    if (JS_IsException(val))
        return -1;
    if (JS_IsUndefined(val))
        return B64_ALPHABET_BASE64;
    if (!JS_IsString(val)) {
        JS_FreeValue(ctx, val);
        JS_ThrowTypeError(ctx, "expected string for alphabet");
        return -1;
    }

    str = JS_ToCString(ctx, val);
    JS_FreeValue(ctx, val);
    if (!str)
        return -1;

    if (!strcmp(str, "base64"))
        ret = B64_ALPHABET_BASE64;
    else if (!strcmp(str, "base64url"))
        ret = B64_ALPHABET_BASE64URL;
    else {
        JS_ThrowTypeError(ctx, "invalid alphabet");
        ret = -1;
    }
    JS_FreeCString(ctx, str);
    return ret;
}

/* Parse the 'lastChunkHandling' option. Returns mode or -1 on error. */
static int parse_last_chunk_option(JSContext *ctx, JSValueConst options)
{
    JSValue val;
    const char *str;
    int ret;

    if (JS_IsUndefined(options))
        return B64_LAST_LOOSE;

    val = JS_GetProperty(ctx, options, JS_ATOM_lastChunkHandling);
    if (JS_IsException(val))
        return -1;
    if (JS_IsUndefined(val))
        return B64_LAST_LOOSE;
    if (!JS_IsString(val)) {
        JS_FreeValue(ctx, val);
        JS_ThrowTypeError(ctx, "expected string for lastChunkHandling");
        return -1;
    }

    str = JS_ToCString(ctx, val);
    JS_FreeValue(ctx, val);
    if (!str)
        return -1;

    if (!strcmp(str, "loose"))
        ret = B64_LAST_LOOSE;
    else if (!strcmp(str, "strict"))
        ret = B64_LAST_STRICT;
    else if (!strcmp(str, "stop-before-partial"))
        ret = B64_LAST_STOP_BEFORE_PARTIAL;
    else {
        JS_ThrowTypeError(ctx, "invalid lastChunkHandling option");
        ret = -1;
    }
    JS_FreeCString(ctx, str);
    return ret;
}

/* Uint8Array.prototype.toBase64([options]) */
static JSValue js_uint8array_to_base64(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    uint8_t *data;
    size_t len;
    JSValueConst options;
    JSObject *p;
    int alphabet, omit_padding;
    size_t out_len, written;
    JSString *ostr;
    char *dst;

    p = check_uint8array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;

    options = argc > 0 ? argv[0] : JS_UNDEFINED;
    if (check_options_object(ctx, options))
        return JS_EXCEPTION;
    alphabet = parse_alphabet_option(ctx, options);
    if (alphabet < 0)
        return JS_EXCEPTION;

    omit_padding = 0;
    if (!JS_IsUndefined(options)) {
        JSValue op_val = JS_GetProperty(ctx, options, JS_ATOM_omitPadding);
        if (JS_IsException(op_val))
            return JS_EXCEPTION;
        omit_padding = JS_ToBool(ctx, op_val);
        JS_FreeValue(ctx, op_val);
    }

    if (get_uint8array_bytes(ctx, p, &data, &len))
        return JS_EXCEPTION;

    out_len = 4 * ((len + 2) / 3);

    if (unlikely(out_len > JS_STRING_LEN_MAX))
        return JS_ThrowRangeError(ctx, "output too large");

    ostr = js_alloc_string(ctx, out_len, 0);
    if (!ostr)
        return JS_EXCEPTION;

    dst = (char *)ostr->u.str8;
    written = b64_encode(data, len, dst,
                         alphabet == B64_ALPHABET_BASE64URL ? b64url_enc : b64_enc);
    if (omit_padding) {
        while (written > 0 && dst[written - 1] == '=')
            written--;
    }
    dst[written] = '\0';

    ostr->len = written;
    return JS_MKPTR(JS_TAG_STRING, ostr);
}

/* Uint8Array.prototype.toHex() */
static JSValue js_uint8array_to_hex(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    uint8_t *data;
    size_t len, out_len;
    JSObject *p;
    JSString *ostr;

    p = check_uint8array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;
    if (get_uint8array_bytes(ctx, p, &data, &len))
        return JS_EXCEPTION;

    out_len = len * 2;
    if (unlikely(out_len > JS_STRING_LEN_MAX))
        return JS_ThrowRangeError(ctx, "output too large");

    ostr = js_alloc_string(ctx, out_len, 0);
    if (!ostr)
        return JS_EXCEPTION;

    u8a_hex_encode(data, len, (char *)ostr->u.str8);
    ostr->u.str8[out_len] = '\0';
    return JS_MKPTR(JS_TAG_STRING, ostr);
}

/* Uint8Array.fromBase64(string[, options]) */
static JSValue js_uint8array_from_base64(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    const char *str;
    size_t str_len, read_pos, decoded_len, out_cap;
    int alphabet, last_chunk, err;
    uint8_t *buf;
    JSValue result;
    JSValueConst options;

    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "expected string");

    str = JS_ToCStringLen(ctx, &str_len, argv[0]);
    if (!str)
        return JS_EXCEPTION;

    options = argc > 1 ? argv[1] : JS_UNDEFINED;
    if (check_options_object(ctx, options)) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }
    alphabet = parse_alphabet_option(ctx, options);
    if (alphabet < 0) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }
    last_chunk = parse_last_chunk_option(ctx, options);
    if (last_chunk < 0) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    out_cap = (str_len / 4) * 3 + 3;
    buf = js_malloc(ctx, out_cap);
    if (!buf) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    decoded_len = from_base64(str, str_len, buf, out_cap,
                              alphabet == B64_ALPHABET_BASE64URL
                                  ? b64url_dec : b64_dec,
                              last_chunk, &read_pos, &err);
    JS_FreeCString(ctx, str);

    if (err) {
        js_free(ctx, buf);
        return JS_ThrowSyntaxError(ctx, "invalid base64 string");
    }

    result = JS_NewUint8ArrayCopy(ctx, buf, decoded_len);
    js_free(ctx, buf);
    return result;
}

/* Uint8Array.fromHex(string) */
static JSValue js_uint8array_from_hex(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    const char *str;
    size_t str_len, read_pos, decoded_len, out_cap;
    int err;
    uint8_t *buf;
    JSValue result;

    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "expected string");

    str = JS_ToCStringLen(ctx, &str_len, argv[0]);
    if (!str)
        return JS_EXCEPTION;

    out_cap = str_len / 2 + 1;
    buf = js_malloc(ctx, out_cap);
    if (!buf) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    decoded_len = u8a_hex_decode(str, str_len, buf, out_cap, &read_pos, &err);
    JS_FreeCString(ctx, str);

    if (err) {
        js_free(ctx, buf);
        return JS_ThrowSyntaxError(ctx, "invalid hex string");
    }

    /* XXX: could avoid the copy */
    result = JS_NewUint8ArrayCopy(ctx, buf, decoded_len);
    js_free(ctx, buf);
    return result;
}

/* Return a { read, written } result object */
static JSValue js_make_read_written(JSContext *ctx, size_t read, size_t written)
{
    JSValue obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    if (JS_DefinePropertyValueStr(ctx, obj, "read",
                                  JS_NewUint32(ctx, read), JS_PROP_C_W_E) < 0)
        goto fail;
    if (JS_DefinePropertyValueStr(ctx, obj, "written",
                                  JS_NewUint32(ctx, written), JS_PROP_C_W_E) < 0)
        goto fail;
    return obj;
fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

/* Uint8Array.prototype.setFromBase64(string[, options]) */
static JSValue js_uint8array_set_from_base64(JSContext *ctx,
                                             JSValueConst this_val,
                                             int argc, JSValueConst *argv)
{
    uint8_t *data;
    size_t len;
    const char *str;
    size_t str_len, read_pos, decoded_len;
    JSObject *p;
    int alphabet, last_chunk, err;
    JSValueConst options;

    p = check_uint8array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;

    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "expected string");

    str = JS_ToCStringLen(ctx, &str_len, argv[0]);
    if (!str)
        return JS_EXCEPTION;

    options = argc > 1 ? argv[1] : JS_UNDEFINED;
    if (check_options_object(ctx, options)) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }
    alphabet = parse_alphabet_option(ctx, options);
    if (alphabet < 0) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }
    last_chunk = parse_last_chunk_option(ctx, options);
    if (last_chunk < 0) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    if (get_uint8array_bytes(ctx, p, &data, &len)) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    decoded_len = from_base64(str, str_len, data, len,
                              alphabet == B64_ALPHABET_BASE64URL
                                  ? b64url_dec : b64_dec,
                              last_chunk, &read_pos, &err);
    JS_FreeCString(ctx, str);

    if (err)
        return JS_ThrowSyntaxError(ctx, "invalid base64 string");

    return js_make_read_written(ctx, read_pos, decoded_len);
}

/* Uint8Array.prototype.setFromHex(string) */
static JSValue js_uint8array_set_from_hex(JSContext *ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    uint8_t *data;
    size_t len;
    const char *str;
    size_t str_len, read_pos, decoded_len;
    JSObject *p;
    int err;

    p = check_uint8array(ctx, this_val);
    if (!p)
        return JS_EXCEPTION;

    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "expected string");

    str = JS_ToCStringLen(ctx, &str_len, argv[0]);
    if (!str)
        return JS_EXCEPTION;

    if (get_uint8array_bytes(ctx, p, &data, &len)) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    decoded_len = u8a_hex_decode(str, str_len, data, len, &read_pos, &err);
    JS_FreeCString(ctx, str);

    if (err)
        return JS_ThrowSyntaxError(ctx, "invalid hex string");

    return js_make_read_written(ctx, read_pos, decoded_len);
}

const JSCFunctionListEntry js_uint8array_proto_funcs[] = {
    JS_PROP_INT32_DEF("BYTES_PER_ELEMENT", 1, 0),
    JS_CFUNC_DEF("toBase64", 0, js_uint8array_to_base64),
    JS_CFUNC_DEF("toHex", 0, js_uint8array_to_hex),
    JS_CFUNC_DEF("setFromBase64", 1, js_uint8array_set_from_base64),
    JS_CFUNC_DEF("setFromHex", 1, js_uint8array_set_from_hex),
};

const JSCFunctionListEntry js_uint8array_funcs[] = {
    JS_PROP_INT32_DEF("BYTES_PER_ELEMENT", 1, 0),
    JS_CFUNC_DEF("fromBase64", 1, js_uint8array_from_base64),
    JS_CFUNC_DEF("fromHex", 1, js_uint8array_from_hex),
};
