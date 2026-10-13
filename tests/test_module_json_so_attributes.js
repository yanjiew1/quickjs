import { assert } from "./assert.js";
import value from "./module-attributes/json.so" with { type: "json" };

assert(value.value, 42);
assert((await import("./module-attributes/json.so",
                     { with: { type: "json" } })).default, value);
let error;
try {
    await import("./module-attributes/invalid-json.so", { with: { type: "json" } });
} catch (e) {
    error = e;
}
assert(error instanceof SyntaxError, true);
