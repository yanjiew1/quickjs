import { assert } from "./assert.js";
import { ordinary, value, again, jsonNamespace }
    from "./fixture_module_attributes_static.js";

async function rejects_syntax(specifier, options)
{
    let error;
    try {
        await import(specifier, options);
    } catch (e) {
        error = e;
    }
    assert(error instanceof SyntaxError, true, specifier);
}

assert(Object.hasOwn(ordinary, "default"), false);
assert(value, 17);
assert(again, 17);
assert(jsonNamespace.default, 17);
assert(await import("./fixture_module_attributes_dual.js"), ordinary);
assert(await import("./fixture_module_attributes_dual.js", { with: {} }), ordinary);
assert(await import("./fixture_module_attributes_dual.js",
                    { with: { type: "json" } }), jsonNamespace);

const source = await import("./fixture_module_attributes_source.js");
assert(source.default, 17);
assert(await import("./fixture_module_attributes_source.js", { with: {} }), source);
await rejects_syntax("./fixture_module_attributes_source.js",
                     { with: { type: "json" } });
assert(await import("./fixture_module_attributes_source.js"), source);

const json = await import("./fixture_module_attributes_json.js",
                          { with: { type: "json" } });
assert(json.default.value, 42);
assert(Object.keys(json).join(","), "default");
assert(await import("./fixture_module_attributes_json.js",
                    { with: { type: ["j", "son"].join("") } }), json);
json.default.value = 43;
assert((await import("./fixture_module_attributes_json.js",
                     { with: { type: "json" } })).default.value, 43);
await rejects_syntax("./fixture_module_attributes_json.js");

const extended = await import("./fixture_module_attributes_extended.js",
                              { with: { type: "json5" } });
assert(extended.default.value, 42);
await rejects_syntax("./fixture_module_attributes_extended.js",
                     { with: { type: "json" } });
assert(await import("./fixture_module_attributes_extended.js",
                    { with: { type: "json5" } }), extended);

/* Attributed self/cyclic imports must see identity during eager compilation. */
const jsOptions = { with: { type: "js" } };
const self = await import("./fixture_module_attributes_self.js", jsOptions);
assert(self.readMarker(), self.marker);
assert(await import("./fixture_module_attributes_self.js", jsOptions), self);
assert(globalThis.attributeSelfEvaluations, 1);
const cycleA = await import("./fixture_module_attributes_cycle_a.js", jsOptions);
const cycleB = await import("./fixture_module_attributes_cycle_b.js", jsOptions);
assert(cycleA.fromB(), cycleB.marker);
assert(cycleB.fromA(), cycleA.marker);
assert(await import("./fixture_module_attributes_cycle_a.js", jsOptions), cycleA);
assert(await import("./fixture_module_attributes_cycle_b.js", jsOptions), cycleB);
assert(globalThis.attributeCycleAEvaluations, 1);
assert(globalThis.attributeCycleBEvaluations, 1);
