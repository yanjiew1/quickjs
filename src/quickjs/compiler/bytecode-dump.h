/*
 * QuickJS compiler bytecode diagnostic interfaces
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
#ifndef QUICKJS_BYTECODE_DUMP_H
#define QUICKJS_BYTECODE_DUMP_H

#include "compiler-state.h"

#ifdef DUMP_BYTECODE
void dump_byte_code(JSContext *ctx, int pass,
                    const uint8_t *tab, int len,
                    const JSBytecodeVarDef *vardefs, 
                    const JSVarDef *args, int arg_count,
                    const JSVarDef *vars, int var_count,
                    const JSClosureVar *closure_var, int closure_var_count,
                    const JSValue *cpool, uint32_t cpool_count,
                    const char *source,
                    const LabelSlot *label_slots, JSFunctionBytecode *b);
__maybe_unused void js_dump_function_bytecode(JSContext *ctx, JSFunctionBytecode *b);
#endif

#endif
