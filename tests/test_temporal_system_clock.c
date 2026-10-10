/* Fixed host-clock probes for Date and Temporal, including clamp and floor.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include <inttypes.h>
#include "../src/quickjs/internal/base.h"
#include "../src/quickjs/builtins/temporal/temporal-internal.h"

static QJSTemporalEpochNs fixed_epoch;
static int fixed_error, clock_calls;
/* Link native time-zone.c once with only its clock symbol renamed. */
int qjs_temporal_system_epoch(QJSTemporalEpochNs *result)
{
    clock_calls++;
    if (fixed_error) return fixed_error;
    *result = fixed_epoch;
    return 0;
}
static void eval_ok(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source),
                            "system-clock.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(value)) {
        JSValue error = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, error);
        fprintf(stderr, "system clock: %s\n", message ? message : "exception");
        JS_FreeCString(ctx, message); JS_FreeValue(ctx, error); abort();
    }
    JS_FreeValue(ctx, value);
}
static void run_case(JSContext *ctx, QJSTemporalEpochNs host,
                      const char *nanoseconds, int64_t milliseconds)
{
    char source[2048];
    fixed_epoch = host; fixed_error = 0; clock_calls = 0;
    snprintf(source, sizeof(source),
        "{ const ns = BigInt('%s'), ms = %" PRId64 ";"
        " if (Date.now() !== ms || new Date().getTime() !== ms ||"
        "     Date() !== new Date(ms).toString()) throw Error('Date clock floor/clamp');"
        " if (Temporal.Now.instant().epochNanoseconds !== ns ||"
        "     Temporal.Now.zonedDateTimeISO('UTC').epochNanoseconds !== ns)"
        "   throw Error('Temporal clock clamp');"
        " const expected = new Temporal.Instant(ns).toZonedDateTimeISO('UTC');"
        " if (!Temporal.Now.plainDateTimeISO('UTC').equals(expected.toPlainDateTime()) ||"
        "     !Temporal.Now.plainDateISO('UTC').equals(expected.toPlainDate()) ||"
        "     !Temporal.Now.plainTimeISO('UTC').equals(expected.toPlainTime()))"
        "   throw Error('Temporal.Now shared clock source'); }",
        nanoseconds, milliseconds);
    eval_ok(ctx, source);
    assert(clock_calls == 8); /* Three Date and five Temporal clock operations. */
}
int main(void)
{
    static const struct {
        int64_t ns, ms;
        const char *text;
    } small[] = {
        {-1000001, -2, "-1000001"}, {-1000000, -1, "-1000000"},
        {-999999, -1, "-999999"}, {-1, -1, "-1"}, {0, 0, "0"},
        {1, 0, "1"}, {999999, 0, "999999"},
        {1000000, 1, "1000000"}, {1000001, 1, "1000001"},
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    QJSTemporalEpochNs positive, negative, outside;
    size_t i;
    assert(rt); ctx = JS_NewContext(rt); assert(ctx);
    for (i = 0; i < countof(small); i++)
        run_case(ctx, qjs_temporal_epoch_ns_from_int64(small[i].ns), small[i].text, small[i].ms);
    assert(!qjs_temporal_epoch_ns_from_milliseconds(&positive, 8640000000000000.0));
    assert(!qjs_temporal_epoch_ns_from_milliseconds(&negative, -8640000000000000.0));
    run_case(ctx, positive, "8640000000000000000000", INT64_C(8640000000000000));
    run_case(ctx, negative, "-8640000000000000000000", -INT64_C(8640000000000000));
    assert(!qjs_temporal_epoch_ns_add(&outside, positive, qjs_temporal_epoch_ns_from_int64(1)));
    run_case(ctx, outside, "8640000000000000000000", INT64_C(8640000000000000));
    assert(!qjs_temporal_epoch_ns_add(&outside, negative, qjs_temporal_epoch_ns_from_int64(-1)));
    run_case(ctx, outside, "-8640000000000000000000", -INT64_C(8640000000000000));
    fixed_epoch.low = UINT64_MAX; fixed_epoch.high = INT64_MAX; /* Huge positive host result. */
    run_case(ctx, fixed_epoch, "8640000000000000000000", INT64_C(8640000000000000));
    fixed_epoch.low = 0; fixed_epoch.high = UINT64_C(1) << 63; /* Huge negative host result. */
    run_case(ctx, fixed_epoch, "-8640000000000000000000", -INT64_C(8640000000000000));
    fixed_error = QJS_TEMPORAL_ERROR_MEMORY; clock_calls = 0;
    eval_ok(ctx,
        "for (const operation of [() => Date.now(), () => new Date(), () => Date(),"
        "                         () => Temporal.Now.instant()]) {"
        " let error; try { operation(); } catch (caught) { error = caught; }"
        " if (!(error instanceof InternalError)) throw Error('clock failure must propagate'); }");
    assert(clock_calls == 4);
    fixed_error = QJS_TEMPORAL_ERROR_BACKEND; clock_calls = 0;
    eval_ok(ctx,
        "for (const operation of [() => Date.now(), () => Temporal.Now.instant()]) {"
        " let error; try { operation(); } catch (caught) { error = caught; }"
        " if (!(error instanceof InternalError)) throw Error('host clock backend failure'); }");
    assert(clock_calls == 2);
    fixed_error = 0;
    JS_FreeContext(ctx); JS_FreeRuntime(rt);
    return 0;
}
