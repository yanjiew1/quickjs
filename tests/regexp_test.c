/*
 * Regular Expression Engine Test
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cutils.h"
#include "libregexp.h"

BOOL lre_check_stack_overflow(void *opaque, size_t alloca_size)
{
    return FALSE;
}

void *lre_realloc(void *opaque, void *ptr, size_t size)
{
    return realloc(ptr, size);
}

int lre_check_timeout(void *opaque)
{
    return 0;
}

int main(int argc, char **argv)
{
    int len, flags, ret, i;
    uint8_t *bc;
    char error_msg[64];
    uint8_t **capture;
    const char *input;
    int input_len, capture_count;

    if (argc < 4) {
        printf("usage: %s regexp flags input\n", argv[0]);
        return 1;
    }
    flags = atoi(argv[2]);
    bc = lre_compile(&len, error_msg, sizeof(error_msg), argv[1],
                     strlen(argv[1]), flags, NULL);
    if (!bc) {
        fprintf(stderr, "error: %s\n", error_msg);
        exit(1);
    }

    input = argv[3];
    input_len = strlen(input);

    capture = malloc(sizeof(capture[0]) * lre_get_alloc_count(bc));
    ret = lre_exec(capture, bc, (uint8_t *)input, 0, input_len, 0, NULL);
    printf("ret=%d\n", ret);
    if (ret == 1) {
        capture_count = lre_get_capture_count(bc);
        for(i = 0; i < 2 * capture_count; i++) {
            uint8_t *ptr;
            ptr = capture[i];
            printf("%d: ", i);
            if (!ptr)
                printf("<nil>");
            else
                printf("%u", (int)(ptr - (uint8_t *)input));
            printf("\n");
        }
    }
    free(capture);
    return 0;
}
