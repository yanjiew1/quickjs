import { assert, assertThrows } from "./assert.js";
import { mutable as imported, update } from "./fixture_module.js";
import * as namespace from "./fixture_module.js";

assert(Reflect.set(namespace, "mutable", 9), false);
assertThrows(TypeError, () => { namespace.mutable = 9; });

function test_import_reference_timing()
{
    let effects = 0;
    assert(imported ||= ++effects, 1);
    assert(imported ??= ++effects, 1);
    assert(effects, 0);
    assertThrows(TypeError, () => { imported &&= ++effects; });
    assert(effects, 1);
    assert(imported, 1);
    assertThrows(TypeError, () => { eval("imported = ++effects"); });
    assert(effects, 2);
    assert(imported, 1);
    update(0);
    assert(imported &&= ++effects, 0);
    assertThrows(TypeError, () => { imported ||= ++effects; });
    assert(effects, 3);
    assert(imported, 0);
    assert(Reflect.set(namespace, "mutable", 9), false);
    assert(imported, 0);
}

test_import_reference_timing();
