/*
 * QuickJS class registration and prototypes
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
#include "internal/class.h"
#include "internal/runtime.h"
#include "internal/allocator.h"
#include "internal/atom.h"
#include "internal/string.h"
#include "internal/object.h"

/* JSClass support */

static JSClassID js_class_id_alloc = JS_CLASS_INIT_COUNT;

#ifdef CONFIG_ATOMICS
static pthread_mutex_t js_class_id_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* a new class ID is allocated if *pclass_id != 0 */
JSClassID JS_NewClassID(JSClassID *pclass_id)
{
    JSClassID class_id;
#ifdef CONFIG_ATOMICS
    pthread_mutex_lock(&js_class_id_mutex);
#endif
    class_id = *pclass_id;
    if (class_id == 0) {
        class_id = js_class_id_alloc++;
        *pclass_id = class_id;
    }
#ifdef CONFIG_ATOMICS
    pthread_mutex_unlock(&js_class_id_mutex);
#endif
    return class_id;
}

JSClassID JS_GetClassID(JSValue v)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(v) != JS_TAG_OBJECT)
        return JS_INVALID_CLASS_ID;
    p = JS_VALUE_GET_OBJ(v);
    return p->class_id;
}

BOOL JS_IsRegisteredClass(JSRuntime *rt, JSClassID class_id)
{
    return (class_id < rt->class_count &&
            rt->class_array[class_id].class_id != 0);
}

/* create a new object internal class. Return -1 if error, 0 if
   OK. The finalizer can be NULL if none is needed. */
static int JS_NewClass1(JSRuntime *rt, JSClassID class_id,
                        const JSClassDef *class_def, JSAtom name)
{
    int new_size, i;
    JSClass *cl, *new_class_array;
    struct list_head *el;

    if (class_id >= (1 << 16))
        return -1;
    if (class_id < rt->class_count &&
        rt->class_array[class_id].class_id != 0)
        return -1;

    if (class_id >= rt->class_count) {
        new_size = max_int(JS_CLASS_INIT_COUNT,
                           max_int(class_id + 1, rt->class_count * 3 / 2));

        /* reallocate the context class prototype array, if any */
        list_for_each(el, &rt->context_list) {
            JSContext *ctx = list_entry(el, JSContext, link);
            JSValue *new_tab;
            new_tab = js_realloc_rt(rt, ctx->class_proto,
                                    sizeof(ctx->class_proto[0]) * new_size);
            if (!new_tab)
                return -1;
            for(i = rt->class_count; i < new_size; i++)
                new_tab[i] = JS_NULL;
            ctx->class_proto = new_tab;
        }
        /* reallocate the class array */
        new_class_array = js_realloc_rt(rt, rt->class_array,
                                        sizeof(JSClass) * new_size);
        if (!new_class_array)
            return -1;
        memset(new_class_array + rt->class_count, 0,
               (new_size - rt->class_count) * sizeof(JSClass));
        rt->class_array = new_class_array;
        rt->class_count = new_size;
    }
    cl = &rt->class_array[class_id];
    cl->class_id = class_id;
    cl->class_name = JS_DupAtomRT(rt, name);
    cl->finalizer = class_def->finalizer;
    cl->gc_mark = class_def->gc_mark;
    cl->call = class_def->call;
    cl->exotic = class_def->exotic;
    return 0;
}

int JS_NewClass(JSRuntime *rt, JSClassID class_id, const JSClassDef *class_def)
{
    int ret, len;
    JSAtom name;

    len = strlen(class_def->class_name);
    name = __JS_FindAtom(rt, class_def->class_name, len, JS_ATOM_TYPE_STRING);
    if (name == JS_ATOM_NULL) {
        name = __JS_NewAtomInit(rt, class_def->class_name, len, JS_ATOM_TYPE_STRING);
        if (name == JS_ATOM_NULL)
            return -1;
    }
    ret = JS_NewClass1(rt, class_id, class_def, name);
    JS_FreeAtomRT(rt, name);
    return ret;
}

int init_class_range(JSRuntime *rt, JSClassShortDef const *tab,
                     int start, int count)
{
    JSClassDef cm_s, *cm = &cm_s;
    int i, class_id;

    for(i = 0; i < count; i++) {
        class_id = i + start;
        memset(cm, 0, sizeof(*cm));
        cm->finalizer = tab[i].finalizer;
        cm->gc_mark = tab[i].gc_mark;
        if (JS_NewClass1(rt, class_id, cm, tab[i].class_name) < 0)
            return -1;
    }
    return 0;
}


void JS_SetClassProto(JSContext *ctx, JSClassID class_id, JSValue obj)
{
    JSRuntime *rt = ctx->rt;
    assert(class_id < rt->class_count);
    set_value(ctx, &ctx->class_proto[class_id], obj);
}

JSValue JS_GetClassProto(JSContext *ctx, JSClassID class_id)
{
    JSRuntime *rt = ctx->rt;
    assert(class_id < rt->class_count);
    return JS_DupValue(ctx, ctx->class_proto[class_id]);
}
