/*
 * Unicode table encoding
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
#ifndef UNICODE_ENCODING_H
#define UNICODE_ENCODING_H

enum {
    RUN_TYPE_U,
    RUN_TYPE_L,
    RUN_TYPE_UF,
    RUN_TYPE_LF,
    RUN_TYPE_UL,
    RUN_TYPE_LSU,
    RUN_TYPE_U2L_399_EXT2,
    RUN_TYPE_UF_D20,
    RUN_TYPE_UF_D1_EXT,
    RUN_TYPE_U_EXT,
    RUN_TYPE_LF_EXT,
    RUN_TYPE_UF_EXT2,
    RUN_TYPE_LF_EXT2,
    RUN_TYPE_UF_EXT3,
};

#define UNICODE_DECOMP_LEN_MAX 18

typedef enum {
    DECOMP_TYPE_C1, /* 16 bit char */
    DECOMP_TYPE_L1, /* 16 bit char table */
    DECOMP_TYPE_L2,
    DECOMP_TYPE_L3,
    DECOMP_TYPE_L4,
    DECOMP_TYPE_L5, /* XXX: not used */
    DECOMP_TYPE_L6, /* XXX: could remove */
    DECOMP_TYPE_L7, /* XXX: could remove */
    DECOMP_TYPE_LL1, /* 18 bit char table */
    DECOMP_TYPE_LL2,
    DECOMP_TYPE_S1, /* 8 bit char table */
    DECOMP_TYPE_S2,
    DECOMP_TYPE_S3,
    DECOMP_TYPE_S4,
    DECOMP_TYPE_S5,
    DECOMP_TYPE_I1, /* increment 16 bit char value */
    DECOMP_TYPE_I2_0,
    DECOMP_TYPE_I2_1,
    DECOMP_TYPE_I3_1,
    DECOMP_TYPE_I3_2,
    DECOMP_TYPE_I4_1,
    DECOMP_TYPE_I4_2,
    DECOMP_TYPE_B1, /* 16 bit base + 8 bit offset */
    DECOMP_TYPE_B2,
    DECOMP_TYPE_B3,
    DECOMP_TYPE_B4,
    DECOMP_TYPE_B5,
    DECOMP_TYPE_B6,
    DECOMP_TYPE_B7,
    DECOMP_TYPE_B8,
    DECOMP_TYPE_B18,
    DECOMP_TYPE_LS2,
    DECOMP_TYPE_PAT3,
    DECOMP_TYPE_S2_UL,
    DECOMP_TYPE_LS2_UL,
} DecompTypeEnum;

/* Compressed range encoding:
   initial byte:
   bits 0..4: category number (special case 31)
   bits 5..7: range length (add 1)
   special case bits 5..7 == 7: read an extra byte
   - 00..7F: range length (add 7 + 1)
   - 80..BF: 6-bits plus extra byte for range length (add 7 + 128)
   - C0..FF: 6-bits plus 2 extra bytes for range length (add 7 + 128 + 16384)
 */
/* Decode into distinct local variables: pointer p, byte b, encoded length n
   (range length minus one), category v. Keep the original statement sequence:
   outlining/restructuring it changes Clang's range-builder code generation. */
#define UNICODE_GC_READ(p, b, n, v) do {                                       \
    (b) = *(p)++;                                                              \
    (n) = (b) >> 5;                                                            \
    (v) = (b) & 0x1f;                                                          \
    if ((n) == 7) {                                                            \
        (n) = *(p)++;                                                          \
        if ((n) < 128) {                                                       \
            (n) += 7;                                                          \
        } else if ((n) < 128 + 64) {                                           \
            (n) = ((n) - 128) << 8;                                            \
            (n) |= *(p)++;                                                     \
            (n) += 7 + 128;                                                    \
        } else {                                                               \
            (n) = ((n) - 128 - 64) << 16;                                      \
            (n) |= *(p)++ << 8;                                                \
            (n) |= *(p)++;                                                     \
            (n) += 7 + 128 + (1 << 14);                                        \
        }                                                                      \
    }                                                                          \
} while (0)

#endif
