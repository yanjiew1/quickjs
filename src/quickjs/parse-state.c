/*
 * QuickJS shared parser state and diagnostics
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
#include "internal/base.h"
#include "internal/runtime.h"
#include "internal/allocator.h"
#include "internal/error.h"
#include "internal/parse-state.h"

/* return the zero based line and column number in the source. */
/* Note: we no longer support '\r' as line terminator */
int get_line_col(int *pcol_num, const uint8_t *buf, size_t len)
{
    int line_num, col_num, c;
    size_t i;

    line_num = 0;
    col_num = 0;
    for(i = 0; i < len; i++) {
        c = buf[i];
        if (c == '\n') {
            line_num++;
            col_num = 0;
        } else if (c < 0x80 || c >= 0xc0) {
            col_num++;
        }
    }
    *pcol_num = col_num;
    return line_num;
}

/* 'ptr' is the position of the error in the source */
static int js_parse_error_v(JSParseState *s, const uint8_t *ptr, const char *fmt, va_list ap)
{
    JSContext *ctx = s->ctx;
    int line_num, col_num;
    line_num = get_line_col(&col_num, s->buf_start, ptr - s->buf_start);
    JS_ThrowError2(ctx, JS_SYNTAX_ERROR, fmt, ap, FALSE);
    build_backtrace(ctx, ctx->rt->current_exception, s->filename,
                    line_num + 1, col_num + 1, 0);
    return -1;
}

__attribute__((format(printf, 3, 4))) int js_parse_error_pos(JSParseState *s, const uint8_t *ptr, const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = js_parse_error_v(s, ptr, fmt, ap);
    va_end(ap);
    return ret;
}

__attribute__((format(printf, 2, 3))) int js_parse_error(JSParseState *s, const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = js_parse_error_v(s, s->token.ptr, fmt, ap);
    va_end(ap);
    return ret;
}

void free_token(JSParseState *s, JSToken *token)
{
    switch(token->val) {
    case TOK_NUMBER:
        JS_FreeValue(s->ctx, token->u.num.val);
        break;
    case TOK_STRING:
    case TOK_TEMPLATE:
        JS_FreeValue(s->ctx, token->u.str.str);
        break;
    case TOK_REGEXP:
        JS_FreeValue(s->ctx, token->u.regexp.body);
        JS_FreeValue(s->ctx, token->u.regexp.flags);
        break;
    case TOK_IDENT:
    case TOK_PRIVATE_NAME:
        JS_FreeAtom(s->ctx, token->u.ident.atom);
        break;
    default:
        if (token->val >= TOK_FIRST_KEYWORD &&
            token->val <= TOK_LAST_KEYWORD) {
            JS_FreeAtom(s->ctx, token->u.ident.atom);
        }
        break;
    }
}

__exception int ident_realloc(JSContext *ctx, char **pbuf, size_t *psize,
                              char *static_buf)
{
    char *buf, *new_buf;
    size_t size, new_size;

    buf = *pbuf;
    size = *psize;
    if (size >= (SIZE_MAX / 3) * 2)
        new_size = SIZE_MAX;
    else
        new_size = size + (size >> 1);
    if (buf == static_buf) {
        new_buf = js_malloc(ctx, new_size);
        if (!new_buf)
            return -1;
        memcpy(new_buf, buf, size);
    } else {
        new_buf = js_realloc(ctx, buf, new_size);
        if (!new_buf)
            return -1;
    }
    *pbuf = new_buf;
    *psize = new_size;
    return 0;
}

void js_parse_init(JSContext *ctx, JSParseState *s,
                   const char *input, size_t input_len,
                   const char *filename)
{
    memset(s, 0, sizeof(*s));
    s->ctx = ctx;
    s->filename = filename;
    s->buf_start = s->buf_ptr = (const uint8_t *)input;
    s->buf_end = s->buf_ptr + input_len;
    s->token.val = ' ';
    s->token.ptr = s->buf_ptr;

    s->get_line_col_cache.ptr = s->buf_start;
    s->get_line_col_cache.buf_start = s->buf_start;
    s->get_line_col_cache.line_num = 0;
    s->get_line_col_cache.col_num = 0;
}
