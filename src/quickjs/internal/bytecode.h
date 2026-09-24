/*
 * QuickJS internal bytecode interfaces
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
#ifndef QUICKJS_PRIVATE_BYTECODE_H
#define QUICKJS_PRIVATE_BYTECODE_H

/* Internal implementation details; not part of the public QuickJS API. */
void dbuf_put_leb128(DynBuf *s, uint32_t v);
void dbuf_put_sleb128(DynBuf *s, int32_t v1);
int get_leb128(uint32_t *pval, const uint8_t *buf,
                      const uint8_t *buf_end);
int get_sleb128(int32_t *pval, const uint8_t *buf,
                       const uint8_t *buf_end);

#define GLOBAL_VAR_OFFSET 0x40000000
#define ARGUMENT_VAR_OFFSET 0x20000000

/* Internal implementation detail; not part of the public QuickJS API. */
void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);

/* Internal implementation detail; not part of the public QuickJS API. */
JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical);



/* Internal implementation detail; not part of the public QuickJS API. */
BOOL js_class_has_bytecode(JSClassID class_id);



#endif /* QUICKJS_PRIVATE_BYTECODE_H */
