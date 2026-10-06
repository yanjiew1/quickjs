/*
 * QuickJS module execution and asynchronous evaluation
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
#include "internal/module.h"
#include "internal/runtime.h"
#include "internal/allocator.h"
#include "internal/gc.h"
#include "internal/string.h"
#include "internal/object.h"
#include "internal/generator.h"
#include "internal/error.h"
#include "builtins/promise.h"

static void js_set_module_evaluated(JSContext *ctx, JSModuleDef *m)
{
    m->status = JS_MODULE_STATUS_EVALUATED;
    if (!JS_IsUndefined(m->promise)) {
        JSValue value, ret_val;
        assert(m->cycle_root == m);
        value = JS_UNDEFINED;
        ret_val = JS_Call(ctx, m->resolving_funcs[0], JS_UNDEFINED,
                          1, (JSValueConst *)&value);
        JS_FreeValue(ctx, ret_val);
    }
}

typedef struct {
    JSModuleDef **tab;
    int count;
    int size;
} ExecModuleList;

/* XXX: slow. Could use a linked list instead of ExecModuleList */
static BOOL find_in_exec_module_list(ExecModuleList *exec_list, JSModuleDef *m)
{
    int i;
    for(i = 0; i < exec_list->count; i++) {
        if (exec_list->tab[i] == m)
            return TRUE;
    }
    return FALSE;
}

static int gather_available_ancestors(JSContext *ctx, JSModuleDef *module,
                                      ExecModuleList *exec_list)
{
    int i;

    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        return -1;
    }
    for(i = 0; i < module->async_parent_modules_count; i++) {
        JSModuleDef *m = module->async_parent_modules[i];
        if (!find_in_exec_module_list(exec_list, m) &&
            !m->cycle_root->eval_has_exception) {
            assert(m->status == JS_MODULE_STATUS_EVALUATING_ASYNC);
            assert(!m->eval_has_exception);
            assert(m->async_evaluation);
            assert(m->pending_async_dependencies > 0);
            m->pending_async_dependencies--;
            if (m->pending_async_dependencies == 0) {
                if (js_resize_array(ctx, (void **)&exec_list->tab, sizeof(exec_list->tab[0]), &exec_list->size, exec_list->count + 1)) {
                    return -1;
                }
                exec_list->tab[exec_list->count++] = m;
                if (!m->has_tla) {
                    if (gather_available_ancestors(ctx, m, exec_list))
                        return -1;
                }
            }
        }
    }
    return 0;
}

static int exec_module_list_cmp(const void *p1, const void *p2, void *opaque)
{
    JSModuleDef *m1 = *(JSModuleDef **)p1;
    JSModuleDef *m2 = *(JSModuleDef **)p2;
    return (m1->async_evaluation_timestamp > m2->async_evaluation_timestamp) -
        (m1->async_evaluation_timestamp < m2->async_evaluation_timestamp);
}

static int js_execute_async_module(JSContext *ctx, JSModuleDef *m);
static int js_execute_sync_module(JSContext *ctx, JSModuleDef *m,
                                  JSValue *pvalue);
#ifdef DUMP_MODULE_EXEC
static void js_dump_module(JSContext *ctx, const char *str, JSModuleDef *m)
{
    char buf1[ATOM_GET_STR_BUF_SIZE];
    static const char *module_status_str[] = { "unlinked", "linking", "linked", "evaluating", "evaluating_async", "evaluated" };
    printf("%s: %s status=%s\n", str, JS_AtomGetStr(ctx, buf1, sizeof(buf1), m->module_name), module_status_str[m->status]);
}
#endif

static JSValue js_async_module_execution_rejected(JSContext *ctx, JSValueConst this_val,
                                                  int argc, JSValueConst *argv, int magic, JSValue *func_data)
{
    JSModuleDef *module = JS_VALUE_GET_PTR(func_data[0]);
    JSValueConst error = argv[0];
    int i;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, module);
#endif
    if (js_check_stack_overflow(ctx->rt, 0))
        return JS_ThrowStackOverflow(ctx);

    if (module->status == JS_MODULE_STATUS_EVALUATED) {
        assert(module->eval_has_exception);
        return JS_UNDEFINED;
    }

    assert(module->status == JS_MODULE_STATUS_EVALUATING_ASYNC);
    assert(!module->eval_has_exception);
    assert(module->async_evaluation);

    module->eval_has_exception = TRUE;
    module->eval_exception = JS_DupValue(ctx, error);
    module->status = JS_MODULE_STATUS_EVALUATED;
    module->async_evaluation = FALSE;

    if (!JS_IsUndefined(module->promise)) {
        JSValue ret_val;
        assert(module->cycle_root == module);
        ret_val = JS_Call(ctx, module->resolving_funcs[1], JS_UNDEFINED,
                          1, &error);
        JS_FreeValue(ctx, ret_val);
    }

    for(i = 0; i < module->async_parent_modules_count; i++) {
        JSModuleDef *m = module->async_parent_modules[i];
        JSValue m_obj = JS_NewModuleValue(ctx, m);
        js_async_module_execution_rejected(ctx, JS_UNDEFINED, 1, &error, 0,
                                           &m_obj);
        JS_FreeValue(ctx, m_obj);
    }
    return JS_UNDEFINED;
}

static JSValue js_async_module_execution_fulfilled(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv, int magic, JSValue *func_data)
{
    JSModuleDef *module = JS_VALUE_GET_PTR(func_data[0]);
    ExecModuleList exec_list_s, *exec_list = &exec_list_s;
    int i;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, module);
#endif
    if (module->status == JS_MODULE_STATUS_EVALUATED) {
        assert(module->eval_has_exception);
        return JS_UNDEFINED;
    }
    assert(module->status == JS_MODULE_STATUS_EVALUATING_ASYNC);
    assert(!module->eval_has_exception);
    assert(module->async_evaluation);
    module->async_evaluation = FALSE;
    js_set_module_evaluated(ctx, module);

    exec_list->tab = NULL;
    exec_list->count = 0;
    exec_list->size = 0;

    if (gather_available_ancestors(ctx, module, exec_list) < 0) {
        js_free(ctx, exec_list->tab);
        return JS_EXCEPTION;
    }

    /* sort by increasing async_evaluation timestamp */
    rqsort(exec_list->tab, exec_list->count, sizeof(exec_list->tab[0]),
           exec_module_list_cmp, NULL);

    for(i = 0; i < exec_list->count; i++) {
        JSModuleDef *m = exec_list->tab[i];
#ifdef DUMP_MODULE_EXEC
        printf("  %d/%d", i, exec_list->count); js_dump_module(ctx, "", m);
#endif
        if (m->status == JS_MODULE_STATUS_EVALUATED) {
            assert(m->eval_has_exception);
        } else if (m->has_tla) {
            js_execute_async_module(ctx, m);
        } else {
            JSValue error;
            if (js_execute_sync_module(ctx, m, &error) < 0) {
                JSValue m_obj = JS_NewModuleValue(ctx, m);
                js_async_module_execution_rejected(ctx, JS_UNDEFINED,
                                                   1, (JSValueConst *)&error, 0,
                                                   &m_obj);
                JS_FreeValue(ctx, m_obj);
                JS_FreeValue(ctx, error);
            } else {
                m->async_evaluation = FALSE;
                js_set_module_evaluated(ctx, m);
            }
        }
    }
    js_free(ctx, exec_list->tab);
    return JS_UNDEFINED;
}

static int js_execute_async_module(JSContext *ctx, JSModuleDef *m)
{
    JSValue promise, m_obj;
    JSValue resolve_funcs[2], ret_val;
#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif
    promise = js_async_function_call(ctx, m->func_obj, JS_UNDEFINED, 0, NULL, 0);
    if (JS_IsException(promise))
        return -1;
    m_obj = JS_NewModuleValue(ctx, m);
    resolve_funcs[0] = JS_NewCFunctionData(ctx, js_async_module_execution_fulfilled, 0, 0, 1, (JSValueConst *)&m_obj);
    resolve_funcs[1] = JS_NewCFunctionData(ctx, js_async_module_execution_rejected, 0, 0, 1, (JSValueConst *)&m_obj);
    ret_val = js_promise_then(ctx, promise, 2, (JSValueConst *)resolve_funcs);
    JS_FreeValue(ctx, ret_val);
    JS_FreeValue(ctx, m_obj);
    JS_FreeValue(ctx, resolve_funcs[0]);
    JS_FreeValue(ctx, resolve_funcs[1]);
    JS_FreeValue(ctx, promise);
    return 0;
}

/* return < 0 in case of exception. *pvalue contains the exception. */
static int js_execute_sync_module(JSContext *ctx, JSModuleDef *m,
                                  JSValue *pvalue)
{
#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif
    if (m->init_func) {
        /* C module init : no asynchronous execution */
        if (m->init_func(ctx, m) < 0)
            goto fail;
    } else {
        JSValue promise;
        JSPromiseStateEnum state;

        promise = js_async_function_call(ctx, m->func_obj, JS_UNDEFINED, 0, NULL, 0);
        if (JS_IsException(promise))
            goto fail;
        state = JS_PromiseState(ctx, promise);
        if (state == JS_PROMISE_FULFILLED) {
            JS_FreeValue(ctx, promise);
        } else if (state == JS_PROMISE_REJECTED) {
            *pvalue = JS_PromiseResult(ctx, promise);
            JS_FreeValue(ctx, promise);
            return -1;
        } else {
            JS_FreeValue(ctx, promise);
            JS_ThrowTypeError(ctx, "promise is pending");
        fail:
            *pvalue = JS_GetException(ctx);
            return -1;
        }
    }
    *pvalue = JS_UNDEFINED;
    return 0;
}

/* spec: InnerModuleEvaluation. Return (index, JS_UNDEFINED) or (-1,
   exception) */
static int js_inner_module_evaluation(JSContext *ctx, JSModuleDef *m,
                                      int index, JSModuleDef **pstack_top,
                                      JSValue *pvalue)
{
    JSModuleDef *m1;
    int i;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif

    if (js_check_stack_overflow(ctx->rt, 0)) {
        JS_ThrowStackOverflow(ctx);
        *pvalue = JS_GetException(ctx);
        return -1;
    }

    if (m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
        m->status == JS_MODULE_STATUS_EVALUATED) {
        if (m->eval_has_exception) {
            *pvalue = JS_DupValue(ctx, m->eval_exception);
            return -1;
        } else {
            *pvalue = JS_UNDEFINED;
            return index;
        }
    }
    if (m->status == JS_MODULE_STATUS_EVALUATING) {
        *pvalue = JS_UNDEFINED;
        return index;
    }
    assert(m->status == JS_MODULE_STATUS_LINKED);

    m->status = JS_MODULE_STATUS_EVALUATING;
    m->dfs_index = index;
    m->dfs_ancestor_index = index;
    m->pending_async_dependencies = 0;
    index++;
    /* push 'm' on stack */
    m->stack_prev = *pstack_top;
    *pstack_top = m;

    for(i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry *rme = &m->req_module_entries[i];
        m1 = rme->module;
        index = js_inner_module_evaluation(ctx, m1, index, pstack_top, pvalue);
        if (index < 0)
            return -1;
        assert(m1->status == JS_MODULE_STATUS_EVALUATING ||
               m1->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
               m1->status == JS_MODULE_STATUS_EVALUATED);
        if (m1->status == JS_MODULE_STATUS_EVALUATING) {
            m->dfs_ancestor_index = min_int(m->dfs_ancestor_index,
                                            m1->dfs_ancestor_index);
        } else {
            m1 = m1->cycle_root;
            assert(m1->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
                   m1->status == JS_MODULE_STATUS_EVALUATED);
            if (m1->eval_has_exception) {
                *pvalue = JS_DupValue(ctx, m1->eval_exception);
                return -1;
            }
        }
        if (m1->async_evaluation) {
            m->pending_async_dependencies++;
            if (js_resize_array(ctx, (void **)&m1->async_parent_modules, sizeof(m1->async_parent_modules[0]), &m1->async_parent_modules_size, m1->async_parent_modules_count + 1)) {
                *pvalue = JS_GetException(ctx);
                return -1;
            }
            m1->async_parent_modules[m1->async_parent_modules_count++] = m;
        }
    }

    if (m->pending_async_dependencies > 0) {
        assert(!m->async_evaluation);
        m->async_evaluation = TRUE;
        m->async_evaluation_timestamp =
            ctx->rt->module_async_evaluation_next_timestamp++;
    } else if (m->has_tla) {
        assert(!m->async_evaluation);
        m->async_evaluation = TRUE;
        m->async_evaluation_timestamp =
            ctx->rt->module_async_evaluation_next_timestamp++;
        js_execute_async_module(ctx, m);
    } else {
        if (js_execute_sync_module(ctx, m, pvalue) < 0)
            return -1;
    }

    assert(m->dfs_ancestor_index <= m->dfs_index);
    if (m->dfs_index == m->dfs_ancestor_index) {
        for(;;) {
            /* pop m1 from stack */
            m1 = *pstack_top;
            *pstack_top = m1->stack_prev;
            if (!m1->async_evaluation) {
                m1->status = JS_MODULE_STATUS_EVALUATED;
            } else {
                m1->status = JS_MODULE_STATUS_EVALUATING_ASYNC;
            }
            /* spec bug: cycle_root must be assigned before the test */
            m1->cycle_root = m;
            if (m1 == m)
                break;
        }
    }
    *pvalue = JS_UNDEFINED;
    return index;
}

/* Run the <eval> function of the module and of all its requested
   modules. Return a promise or an exception. */
JSValue js_evaluate_module(JSContext *ctx, JSModuleDef *m)
{
    JSModuleDef *m1, *stack_top;
    JSValue ret_val, result;

#ifdef DUMP_MODULE_EXEC
    js_dump_module(ctx, __func__, m);
#endif
    assert(m->status == JS_MODULE_STATUS_LINKED ||
           m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
           m->status == JS_MODULE_STATUS_EVALUATED);
    if (m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
        m->status == JS_MODULE_STATUS_EVALUATED) {
        m = m->cycle_root;
    }
    /* a promise may be created only on the cycle_root of a cycle */
    if (!JS_IsUndefined(m->promise))
        return JS_DupValue(ctx, m->promise);
    m->promise = JS_NewPromiseCapability(ctx, m->resolving_funcs);
    if (JS_IsException(m->promise))
        return JS_EXCEPTION;

    stack_top = NULL;
    if (js_inner_module_evaluation(ctx, m, 0, &stack_top, &result) < 0) {
        while (stack_top != NULL) {
            m1 = stack_top;
            assert(m1->status == JS_MODULE_STATUS_EVALUATING);
            m1->status = JS_MODULE_STATUS_EVALUATED;
            m1->eval_has_exception = TRUE;
            m1->eval_exception = JS_DupValue(ctx, result);
            m1->cycle_root = m; /* spec bug: should be present */
            stack_top = m1->stack_prev;
        }
        JS_FreeValue(ctx, result);
        assert(m->status == JS_MODULE_STATUS_EVALUATED);
        assert(m->eval_has_exception);
        ret_val = JS_Call(ctx, m->resolving_funcs[1], JS_UNDEFINED,
                          1, (JSValueConst *)&m->eval_exception);
        JS_FreeValue(ctx, ret_val);
    } else {
#ifdef DUMP_MODULE_EXEC
        js_dump_module(ctx, "  done", m);
#endif
        assert(m->status == JS_MODULE_STATUS_EVALUATING_ASYNC ||
               m->status == JS_MODULE_STATUS_EVALUATED);
        assert(!m->eval_has_exception);
        if (!m->async_evaluation) {
            JSValue value;
            assert(m->status == JS_MODULE_STATUS_EVALUATED);
            value = JS_UNDEFINED;
            ret_val = JS_Call(ctx, m->resolving_funcs[0], JS_UNDEFINED,
                              1, (JSValueConst *)&value);
            JS_FreeValue(ctx, ret_val);
        }
        assert(stack_top == NULL);
    }
    return JS_DupValue(ctx, m->promise);
}

