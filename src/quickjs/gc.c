/*
 * QuickJS garbage collection and value lifetime
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
#include "internal/value-conversion.h"
#include "internal/value-compare.h"
#include "internal/value-print.h"
#include "internal/runtime.h"
#include "internal/number.h"
#include "internal/bigint.h"
#include "internal/string.h"
#include "internal/object.h"
#include "internal/gc.h"
#include "internal/function.h"
#include "internal/generator.h"
#include "internal/error.h"
#include "internal/function-list.h"
#include "internal/module.h"
#include "internal/frontend.h"
#include "libunicode.h"
#include "builtins/collections.h"
#include "builtins/weakref.h"
#include "builtins/finalization-registry.h"
#include "builtins/proxy.h"
#include "builtins/typed-array.h"
#include "builtins/array-buffer.h"
#include "builtins/string.h"

static void js_autoinit_mark(JSRuntime *rt, JSProperty *pr,
                             JS_MarkFunc *mark_func)
{
    mark_func(rt, &js_autoinit_get_realm(pr)->header);
}

static void free_object(JSRuntime *rt, JSObject *p)
{
    int i;
    JSClassFinalizer *finalizer;
    JSShape *sh;
    JSShapeProperty *pr;

    p->free_mark = 1; /* used to tell the object is invalid when
                         freeing cycles */
    /* free all the fields */
    sh = p->shape;
    pr = get_shape_prop(sh);
    for(i = 0; i < sh->prop_count; i++) {
        free_property(rt, &p->prop[i], pr->flags);
        pr++;
    }
    js_free_rt(rt, p->prop);
    /* as an optimization we destroy the shape immediately without
       putting it in gc_zero_ref_count_list */
    js_free_shape(rt, sh);

    /* fail safe */
    p->shape = NULL;
    p->prop = NULL;

    finalizer = rt->class_array[p->class_id].finalizer;
    if (finalizer)
        (*finalizer)(rt, JS_MKPTR(JS_TAG_OBJECT, p));

    /* fail safe */
    p->class_id = 0;
    p->u.opaque = NULL;
    p->u.func.var_refs = NULL;
    p->u.func.home_object = NULL;

    remove_gc_object(&p->header);
    if (rt->gc_phase == JS_GC_PHASE_REMOVE_CYCLES) {
        if (js_rc(p)->ref_count == 0 && p->weakref_count == 0) {
            js_free_rt(rt, p);
        } else {
            /* keep the object structure because there are may be
               references to it */
            list_add_tail(&p->header.link, &rt->gc_zero_ref_count_list);
        }
    } else {
        /* keep the object structure in case there are weak references to it */
        if (p->weakref_count == 0) {
            js_free_rt(rt, p);
        } else {
            js_rc(p)->mark = 0; /* reset the mark so that the weakref can be freed */
        }
    }
}

static void free_gc_object(JSRuntime *rt, JSGCObjectHeader *gp)
{
    switch(js_rc(gp)->gc_obj_type) {
    case JS_GC_OBJ_TYPE_JS_OBJECT:
        free_object(rt, (JSObject *)gp);
        break;
    case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
        free_function_bytecode(rt, (JSFunctionBytecode *)gp);
        break;
    case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
        __async_func_free(rt, (JSAsyncFunctionState *)gp);
        break;
    case JS_GC_OBJ_TYPE_MODULE:
        js_free_module_def(rt, (JSModuleDef *)gp);
        break;
    default:
        abort();
    }
}

void free_zero_refcount(JSRuntime *rt)
{
    struct list_head *el;
    JSGCObjectHeader *p;

    rt->gc_phase = JS_GC_PHASE_DECREF;
    for(;;) {
        el = rt->gc_zero_ref_count_list.next;
        if (el == &rt->gc_zero_ref_count_list)
            break;
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->ref_count == 0);
        free_gc_object(rt, p);
    }
    rt->gc_phase = JS_GC_PHASE_NONE;
}

/* called with the ref_count of 'v' reaches zero. */
void __JS_FreeValueRT(JSRuntime *rt, JSValue v)
{
    uint32_t tag = JS_VALUE_GET_TAG(v);

#ifdef DUMP_FREE
    {
        printf("Freeing ");
        if (tag == JS_TAG_OBJECT) {
            JS_DumpObject(rt, JS_VALUE_GET_OBJ(v));
        } else {
            JS_DumpValueShort(rt, v);
            printf("\n");
        }
    }
#endif

    switch(tag) {
    case JS_TAG_STRING:
        {
            JSString *p = JS_VALUE_GET_STRING(v);
            if (p->atom_type) {
                JS_FreeAtomStruct(rt, p);
            } else {
#ifdef DUMP_LEAKS
                list_del(&p->link);
#endif
                js_free_rt(rt, p);
            }
        }
        break;
    case JS_TAG_STRING_ROPE:
        /* Note: recursion is acceptable because the rope depth is bounded */
        {
            JSStringRope *p = JS_VALUE_GET_STRING_ROPE(v);
            JS_FreeValueRT(rt, p->left);
            JS_FreeValueRT(rt, p->right);
            js_free_rt(rt, p);
        }
        break;
    case JS_TAG_OBJECT:
    case JS_TAG_FUNCTION_BYTECODE:
    case JS_TAG_MODULE:
        {
            JSGCObjectHeader *p = JS_VALUE_GET_PTR(v);
            if (rt->gc_phase != JS_GC_PHASE_REMOVE_CYCLES) {
                list_del(&p->link);
                list_add(&p->link, &rt->gc_zero_ref_count_list);
                js_rc(p)->mark = 1; /* indicate that the object is about to be freed */
                if (rt->gc_phase == JS_GC_PHASE_NONE) {
                    free_zero_refcount(rt);
                }
            }
        }
        break;
    case JS_TAG_BIG_INT:
        {
            JSBigInt *p = JS_VALUE_GET_PTR(v);
            js_free_rt(rt, p);
        }
        break;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(v);
            JS_FreeAtomStruct(rt, p);
        }
        break;
    default:
        abort();
    }
}

void __JS_FreeValue(JSContext *ctx, JSValue v)
{
    __JS_FreeValueRT(ctx->rt, v);
}

/* garbage collection */

static void gc_remove_weak_objects(JSRuntime *rt)
{
    struct list_head *el;

    /* add the freed objects to rt->gc_zero_ref_count_list so that
       rt->weakref_list is not modified while we traverse it */
    rt->gc_phase = JS_GC_PHASE_DECREF;

    list_for_each(el, &rt->weakref_list) {
        JSWeakRefHeader *wh = list_entry(el, JSWeakRefHeader, link);
        switch(wh->weakref_type) {
        case JS_WEAKREF_TYPE_MAP:
            map_delete_weakrefs(rt, wh);
            break;
        case JS_WEAKREF_TYPE_WEAKREF:
            weakref_delete_weakref(rt, wh);
            break;
        case JS_WEAKREF_TYPE_FINREC:
            finrec_delete_weakref(rt, wh);
            break;
        default:
            abort();
        }
    }

    rt->gc_phase = JS_GC_PHASE_NONE;
    /* free the freed objects here. */
    free_zero_refcount(rt);
}

void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                   JSGCObjectTypeEnum type)
{
    js_rc(h)->mark = 0;
    js_rc(h)->gc_obj_type = type;
    list_add_tail(&h->link, &rt->gc_obj_list);
}

void remove_gc_object(JSGCObjectHeader *h)
{
    list_del(&h->link);
}

void JS_MarkValue(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    if (JS_VALUE_HAS_REF_COUNT(val)) {
        switch(JS_VALUE_GET_TAG(val)) {
        case JS_TAG_OBJECT:
        case JS_TAG_FUNCTION_BYTECODE:
        case JS_TAG_MODULE:
            mark_func(rt, JS_VALUE_GET_PTR(val));
            break;
        default:
            break;
        }
    }
}

static void mark_children(JSRuntime *rt, JSGCObjectHeader *gp,
                          JS_MarkFunc *mark_func)
{
    switch(js_rc(gp)->gc_obj_type) {
    case JS_GC_OBJ_TYPE_JS_OBJECT:
        {
            JSObject *p = (JSObject *)gp;
            JSShapeProperty *prs;
            JSShape *sh;
            int i;
            sh = p->shape;
            mark_func(rt, &sh->header);
            /* mark all the fields */
            prs = get_shape_prop(sh);
            for(i = 0; i < sh->prop_count; i++) {
                JSProperty *pr = &p->prop[i];
                if (prs->atom != JS_ATOM_NULL) {
                    if (prs->flags & JS_PROP_TMASK) {
                        if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                            if (pr->u.getset.getter)
                                mark_func(rt, &pr->u.getset.getter->header);
                            if (pr->u.getset.setter)
                                mark_func(rt, &pr->u.getset.setter->header);
                        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                            /* Note: the tag does not matter
                               provided it is a GC object */
                            mark_func(rt, &pr->u.var_ref->header);
                        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                            js_autoinit_mark(rt, pr, mark_func);
                        }
                    } else {
                        JS_MarkValue(rt, pr->u.value, mark_func);
                    }
                }
                prs++;
            }

            if (p->class_id != JS_CLASS_OBJECT) {
                JSClassGCMark *gc_mark;
                gc_mark = rt->class_array[p->class_id].gc_mark;
                if (gc_mark)
                    gc_mark(rt, JS_MKPTR(JS_TAG_OBJECT, p), mark_func);
            }
        }
        break;
    case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
        /* the template objects can be part of a cycle */
        {
            JSFunctionBytecode *b = (JSFunctionBytecode *)gp;
            int i;
            for(i = 0; i < b->cpool_count; i++) {
                JS_MarkValue(rt, b->cpool[i], mark_func);
            }
            if (b->realm)
                mark_func(rt, &b->realm->header);
        }
        break;
    case JS_GC_OBJ_TYPE_VAR_REF:
        {
            JSVarRef *var_ref = (JSVarRef *)gp;
            if (var_ref->is_detached) {
                JS_MarkValue(rt, *var_ref->pvalue, mark_func);
            } else {
                JSStackFrame *sf = var_ref->stack_frame;
                if (sf->js_mode & JS_MODE_ASYNC) {
                    JSAsyncFunctionState *async_func = container_of(sf, JSAsyncFunctionState, frame);
                    mark_func(rt, &async_func->header);
                }
            }
        }
        break;
    case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
        {
            JSAsyncFunctionState *s = (JSAsyncFunctionState *)gp;
            JSStackFrame *sf = &s->frame;
            JSValue *sp;

            if (!s->is_completed) {
                JS_MarkValue(rt, sf->cur_func, mark_func);
                JS_MarkValue(rt, s->this_val, mark_func);
                /* sf->cur_sp = NULL if the function is running */
                if (sf->cur_sp) {
                    /* if the function is running, cur_sp is not known so we
                       cannot mark the stack. Marking the variables is not needed
                       because a running function cannot be part of a removable
                       cycle */
                    for(sp = sf->arg_buf; sp < sf->cur_sp; sp++)
                        JS_MarkValue(rt, *sp, mark_func);
                }
            }
            JS_MarkValue(rt, s->resolving_funcs[0], mark_func);
            JS_MarkValue(rt, s->resolving_funcs[1], mark_func);
        }
        break;
    case JS_GC_OBJ_TYPE_SHAPE:
        {
            JSShape *sh = (JSShape *)gp;
            if (sh->proto != NULL) {
                mark_func(rt, &sh->proto->header);
            }
        }
        break;
    case JS_GC_OBJ_TYPE_JS_CONTEXT:
        {
            JSContext *ctx = (JSContext *)gp;
            JS_MarkContext(rt, ctx, mark_func);
        }
        break;
    case JS_GC_OBJ_TYPE_MODULE:
        {
            JSModuleDef *m = (JSModuleDef *)gp;
            js_mark_module_def(rt, m, mark_func);
        }
        break;
    default:
        abort();
    }
}

static void gc_decref_child(JSRuntime *rt, JSGCObjectHeader *p)
{
    assert(js_rc(p)->ref_count > 0);
    js_rc(p)->ref_count--;
    if (js_rc(p)->ref_count == 0 && js_rc(p)->mark == 1) {
        list_del(&p->link);
        list_add_tail(&p->link, &rt->tmp_obj_list);
    }
}

void gc_decref(JSRuntime *rt)
{
    struct list_head *el, *el1;
    JSGCObjectHeader *p;

    init_list_head(&rt->tmp_obj_list);

    /* decrement the refcount of all the children of all the GC
       objects and move the GC objects with zero refcount to
       tmp_obj_list */
    list_for_each_safe(el, el1, &rt->gc_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->mark == 0);
        mark_children(rt, p, gc_decref_child);
        js_rc(p)->mark = 1;
        if (js_rc(p)->ref_count == 0) {
            list_del(&p->link);
            list_add_tail(&p->link, &rt->tmp_obj_list);
        }
    }
}

static void gc_scan_incref_child(JSRuntime *rt, JSGCObjectHeader *p)
{
    js_rc(p)->ref_count++;
    if (js_rc(p)->ref_count == 1) {
        /* ref_count was 0: remove from tmp_obj_list and add at the
           end of gc_obj_list */
        list_del(&p->link);
        list_add_tail(&p->link, &rt->gc_obj_list);
        js_rc(p)->mark = 0; /* reset the mark for the next GC call */
    }
}

static void gc_scan_incref_child2(JSRuntime *rt, JSGCObjectHeader *p)
{
    js_rc(p)->ref_count++;
}

static void gc_scan(JSRuntime *rt)
{
    struct list_head *el;
    JSGCObjectHeader *p;

    /* keep the objects with a refcount > 0 and their children. */
    list_for_each(el, &rt->gc_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->ref_count > 0);
        js_rc(p)->mark = 0; /* reset the mark for the next GC call */
        mark_children(rt, p, gc_scan_incref_child);
    }

    /* restore the refcount of the objects to be deleted. */
    list_for_each(el, &rt->tmp_obj_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        mark_children(rt, p, gc_scan_incref_child2);
    }
}

static void gc_free_cycles(JSRuntime *rt)
{
    struct list_head *el, *el1;
    JSGCObjectHeader *p;
#ifdef DUMP_GC_FREE
    BOOL header_done = FALSE;
#endif

    rt->gc_phase = JS_GC_PHASE_REMOVE_CYCLES;

    for(;;) {
        el = rt->tmp_obj_list.next;
        if (el == &rt->tmp_obj_list)
            break;
        p = list_entry(el, JSGCObjectHeader, link);
        /* Only need to free the GC object associated with JS values
           or async functions. The rest will be automatically removed
           because they must be referenced by them. */
        switch(js_rc(p)->gc_obj_type) {
        case JS_GC_OBJ_TYPE_JS_OBJECT:
        case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
        case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
        case JS_GC_OBJ_TYPE_MODULE:
#ifdef DUMP_GC_FREE
            if (!header_done) {
                printf("Freeing cycles:\n");
                JS_DumpObjectHeader(rt);
                header_done = TRUE;
            }
            JS_DumpGCObject(rt, p);
#endif
            free_gc_object(rt, p);
            break;
        default:
            list_del(&p->link);
            list_add_tail(&p->link, &rt->gc_zero_ref_count_list);
            break;
        }
    }
    rt->gc_phase = JS_GC_PHASE_NONE;

    list_for_each_safe(el, el1, &rt->gc_zero_ref_count_list) {
        p = list_entry(el, JSGCObjectHeader, link);
        assert(js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT ||
               js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_FUNCTION_BYTECODE ||
               js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_ASYNC_FUNCTION ||
               js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_MODULE);
        if (js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT &&
            ((JSObject *)p)->weakref_count != 0) {
            /* keep the object because there are weak references to it */
            js_rc(p)->mark = 0;
        } else {
            js_free_rt(rt, p);
        }
    }

    init_list_head(&rt->gc_zero_ref_count_list);
}

void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects)
{
    if (remove_weak_objects) {
        /* free the weakly referenced object or symbol structures, delete
           the associated Map/Set entries and queue the finalization
           registry callbacks. */
        gc_remove_weak_objects(rt);
    }

    /* decrement the reference of the children of each object. mark =
       1 after this pass. */
    gc_decref(rt);

    /* keep the GC objects with a non zero refcount and their childs */
    gc_scan(rt);

    /* free the GC objects in a cycle */
    gc_free_cycles(rt);
}

void JS_RunGC(JSRuntime *rt)
{
    JS_RunGCInternal(rt, TRUE);
}

/* Return false if not an object or if the object has already been
   freed (zombie objects are visible in finalizers when freeing
   cycles). */
BOOL JS_IsLiveObject(JSRuntime *rt, JSValueConst obj)
{
    JSObject *p;
    if (!JS_IsObject(obj))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    return !p->free_mark;
}
