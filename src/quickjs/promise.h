/*
 * QuickJS Promise and Microtask Job Queue definitions
 */
#ifndef QUICKJS_PROMISE_H
#define QUICKJS_PROMISE_H

#include "quickjs/def.h"
#include "quickjs/runtime.h"

int JS_EnqueueJob2(JSContext *ctx, JSJobFunc *job_func,
                   int argc, JSValueConst *argv, BOOL no_exception);
int JS_EnqueueJob(JSContext *ctx, JSJobFunc *job_func,
                  int argc, JSValueConst *argv);
BOOL JS_IsJobPending(JSRuntime *rt);
int JS_ExecutePendingJob(JSRuntime *rt, JSContext **pctx);
void js_free_job_list(JSRuntime *rt);

JSValue js_promise_then(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv);

#endif /* QUICKJS_PROMISE_H */
