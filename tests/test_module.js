import { assert, assertThrows } from "./assert.js";
import { mutable as imported, update } from "./fixture_module.js";
import * as namespace from "./fixture_module.js";
import { module_const_write_tdz as uninitializedImport } from "./test_module.js";

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

function test_module_const_write_tdz()
{
    let effects = 0;
    assertThrows(ReferenceError, () => { module_const_write_tdz = ++effects; });
    assert(effects, 1);
    effects = 0;
    assertThrows(ReferenceError, () => {
        eval("module_const_write_tdz = ++effects");
    });
    assert(effects, 1);
    effects = 0;
    function captured() {
        return () => { module_const_write_tdz = ++effects; };
    }
    assertThrows(ReferenceError, captured());
    assert(effects, 1);
    const rhsError = new Error("RHS");
    function abrupt() { throw rhsError; }
    try {
        module_const_write_tdz = abrupt();
        assert(false);
    } catch (e) {
        assert(e, rhsError);
    }
    try {
        uninitializedImport = abrupt();
        assert(false);
    } catch (e) {
        assert(e, rhsError);
    }
    assertThrows(ReferenceError, () => uninitializedImport);
    effects = 0;
    assertThrows(TypeError, () => { uninitializedImport = ++effects; });
    assert(effects, 1);
    assertThrows(TypeError, () => { eval("uninitializedImport = ++effects"); });
    assert(effects, 2);
    function imported() {
        return () => { uninitializedImport = ++effects; };
    }
    assertThrows(TypeError, imported());
    assert(effects, 3);
    assertThrows(ReferenceError, () => uninitializedImport);
}

test_module_const_write_tdz();
export const module_const_write_tdz = 1;
