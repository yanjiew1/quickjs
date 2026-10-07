import { assert, assertThrows } from "./assert.js";
import { mutable as imported, update } from "./fixture_module.js";
import * as namespace from "./fixture_module.js";
import * as selfNamespace from "./test_module.js";
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

function test_namespace_set(ns, export_name, initialized)
{
    const absent_symbol = Symbol("absent");
    const names = [export_name, "absent", absent_symbol,
                   Symbol.iterator, Symbol.toStringTag];
    const before = initialized ? ns[export_name] : undefined;
    for (const name of names) {
        const receiver = {};
        assert(Reflect.set(ns, name, 7), false);
        assert(Reflect.set(ns, name, 7, receiver), false);
        assert(Object.hasOwn(receiver, name), false);
        assert(Reflect.set(ns, name, 7, null), false);
        assert(Reflect.set(ns, name, 7, 0), false);
        assertThrows(TypeError, () => { ns[name] = 7; });

        const existing = {};
        Object.defineProperty(existing, name, {
            value: 3, writable: true, configurable: true
        });
        assert(Reflect.set(ns, name, 7, existing), false);
        assert(existing[name], 3);

        let traps = 0;
        const proxy_receiver = new Proxy({}, {
            getOwnPropertyDescriptor() { traps++; },
            defineProperty() { traps++; return true; }
        });
        assert(Reflect.set(ns, name, 7, proxy_receiver), false);
        assert(traps, 0);

        const child = Object.create(ns);
        assert(Reflect.set(child, name, 7), false);
        assert(Object.hasOwn(child, name), false);
        assertThrows(TypeError, () => { child[name] = 7; });
        assert(Reflect.set(child, name, 7, receiver), false);
        assert(Object.hasOwn(receiver, name), false);

        Object.defineProperty(child, name, {
            value: 3, writable: true, configurable: true
        });
        assert(Reflect.set(child, name, 7), true);
        assert(child[name], 7);
    }
    if (initialized)
        assert(ns[export_name], before);
    else
        assertThrows(ReferenceError, () => ns[export_name]);
}

test_namespace_set(namespace, "mutable", true);
test_namespace_set(selfNamespace, "namespace_set_tdz", false);
export let namespace_set_tdz = 1;
test_namespace_set(selfNamespace, "namespace_set_tdz", true);

assert(Reflect.set(selfNamespace, "namespace_set_reexport", 7, {}), false);
assertThrows(TypeError, () => { selfNamespace.namespace_set_reexport = 7; });
export * as namespace_set_reexport from "./fixture_module.js";
assert(selfNamespace.namespace_set_reexport, namespace);
