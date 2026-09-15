/*
 * QuickJS Javascript Engine
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
#ifndef QUICKJS_INTERNAL_FRONTEND_H
#define QUICKJS_INTERNAL_FRONTEND_H

#include "internal-module.h"

typedef struct {
    int count;
    uint32_t hash_size;
    struct JSONParseRecordEntry *entries;
    uint32_t *hash_table;
} JSONParseRecordObject;

typedef struct JSONParseRecord {
    JSValue value;
    union {
        JSONParseRecordObject obj;
        struct {
            int count;
            struct JSONParseRecord *elements;
        } array;
        struct {
            uint32_t source_pos;
            uint32_t source_len;
        } primitive;
    } u;
} JSONParseRecord;

typedef struct JSONParseRecordEntry {
    JSAtom atom;
    uint32_t hash_next;
    JSONParseRecord parse_record;
} JSONParseRecordEntry;

QJS_INTERNAL void free_function_bytecode(JSRuntime *rt,
                                              JSFunctionBytecode *bytecode);
QJS_INTERNAL JSValue __JS_EvalInternal(JSContext *ctx,
                                       JSValueConst this_obj,
                                       const char *input, size_t input_len,
                                       const char *filename, int flags,
                                       int scope_idx);
QJS_INTERNAL JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj,
                                     JSValueConst value, int flags,
                                     int scope_idx);
QJS_INTERNAL void json_parse_record_init_obj(JSContext *ctx,
                                                 JSONParseRecord *record,
                                                 JSValueConst value);
QJS_INTERNAL JSONParseRecord *json_parse_record_add(
    JSContext *ctx, JSONParseRecord *record, JSAtom key, int *psize);
QJS_INTERNAL JSONParseRecord *json_parse_record_find(
    JSONParseRecord *record, JSAtom key);
QJS_INTERNAL void json_free_parse_record(JSContext *ctx,
                                             JSONParseRecord *record);
JSValue JS_ParseJSON3(JSContext *ctx, const char *buf,
                                     size_t buf_len, const char *filename,
                                     int flags, JSONParseRecord *record);

#endif /* QUICKJS_INTERNAL_FRONTEND_H */
