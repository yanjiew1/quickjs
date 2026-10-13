/* Fresh-realm fixture for migrated behavioral regressions. */
import * as std from "std";

export function check(actual, expected, message)
{
    if (!Object.is(actual, expected))
        throw Error(message || "realm assertion failed");
}

/* Evaluate the original assertions in two fresh realms, then release both
   carrier objects. Returned functions and pending jobs retain their realms. */
export function run_in_realms(body)
{
    let caller = std.createRealm(), foreign = std.createRealm();
    caller.global.foreign = foreign.global;
    foreign = null;
    std.gc();
    const result = caller.evalScript("(" + body + ")()");
    caller = null;
    std.gc();
    return result;
}
