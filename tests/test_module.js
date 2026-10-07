import { assert, assertThrows } from "./assert.js";
import * as keyNamespace from "./fixture_module_namespace_keys.js";
import { mutable as imported, update } from "./fixture_module.js";
import * as namespace from "./fixture_module.js";
import * as selfNamespace from "./test_module.js";
import { module_const_write_tdz as uninitializedImport } from "./test_module.js";

import "./test_module_bindings_review.js";

import "./test_module_namespace_review.js";

assert(Reflect.set(namespace, "mutable", 9), false);
assertThrows(TypeError, () => { namespace.mutable = 9; });

function test_module_namespace_keys()
{
    const expected = ["", "0", "01", "10", "2", "4294967294", "4294967295",
                      "9007199254740991", "a", "\u{10000}", "\ue000"];
    function check(ns) {
        const lists = [Object.keys(ns), Object.getOwnPropertyNames(ns),
                       Reflect.ownKeys(ns).filter(key => typeof key === "string")];
        const enumerated = [];
        for (const key in ns)
            enumerated.push(key);
        lists.push(enumerated.sort());
        for (const keys of lists) {
            assert(keys.length, expected.length);
            for (let i = 0; i < keys.length; i++)
                assert(keys[i], expected[i], "namespace key " + i);
        }
        const all = Reflect.ownKeys(ns);
        assert(all.length, expected.length + 1);
        assert(all[expected.length], Symbol.toStringTag);
        const entries = Object.entries(ns);
        assert(entries.length, expected.length);
        for (let i = 0; i < entries.length; i++) {
            assert(entries[i][0], expected[i]);
            assert(entries[i][1], 42);
        }
    }

    check(keyNamespace);
    check(new Proxy(keyNamespace, {}));
    assert(Reflect.deleteProperty(keyNamespace, "10"), false);
    assert(Reflect.deleteProperty(keyNamespace, "missing"), true);
    assert(Reflect.defineProperty(keyNamespace, "10", { value: 42 }), true);
    assert(Reflect.defineProperty(keyNamespace, "10", { value: 43 }), false);
    assert(Reflect.deleteProperty(keyNamespace, Symbol.toStringTag), false);
    check(keyNamespace);
    check(new Proxy(keyNamespace, {}));

    const ordinary = { "2": 2, "10": 10, "01": 1, "0": 0 };
    assert(Object.keys(ordinary).join(","), "0,2,10,01");
    assert(Object.getOwnPropertyNames(ordinary).join(","), "0,2,10,01");
    assert(Reflect.ownKeys(new Proxy(ordinary, {})).join(","), "0,2,10,01");
}

test_module_namespace_keys();

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

function test_imported_write_references()
{
    let effects = 0;
    assertThrows(TypeError, () => { uninitializedImport = ++effects; });
    assert(effects, 1);
    assertThrows(TypeError, () => { [uninitializedImport] = [++effects]; });
    assert(effects, 2);
    assertThrows(TypeError, () => {
        class C { [uninitializedImport = ++effects]() {} }
    });
    assert(effects, 3);
    assertThrows(ReferenceError, () => { uninitializedImport += ++effects; });
    assertThrows(ReferenceError, () => { uninitializedImport ||= ++effects; });
    assertThrows(ReferenceError, () => { uninitializedImport &&= ++effects; });
    assertThrows(ReferenceError, () => { uninitializedImport ??= ++effects; });
    assert(effects, 3);

    update(1);
    effects = 0;
    assert(imported ||= ++effects, 1);
    assert(imported ??= ++effects, 1);
    assert(effects, 0);
    assertThrows(TypeError, () => { imported &&= ++effects; });
    assert(effects, 1);
    assert(imported, 1);
    assertThrows(TypeError, () => { imported = ++effects; });
    assert(effects, 2);
    assert(imported, 1);
    assertThrows(TypeError, () => { imported += ++effects; });
    assert(effects, 3);
    assert(imported, 1);
    assertThrows(TypeError, () => { [imported] = [7]; });
    assertThrows(TypeError, () => { ({ value: imported } = { value: 7 }); });
    assert(imported, 1);
    assertThrows(TypeError, () => {
        ({ value: imported = ++effects } = {});
    });
    assert(effects, 4);
    assertThrows(TypeError, () => {
        class C { [imported = ++effects]() {} }
    });
    assert(effects, 5);
    assert(imported, 1);

    const order = [];
    const original = { valueOf() { order.push("convert"); return 1; } };
    update(original);
    assertThrows(TypeError, () => { imported += (order.push("rhs"), 2); });
    assert(order.join(","), "rhs,convert");
    assert(imported, original);
    update(0);
}

test_module_const_write_tdz();
test_imported_write_references();
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

import { usingModuleEvents, usingModuleResource } from "./fixture_using_module.js";
assert(usingModuleEvents.join(","), "evaluated,disposed");
assert(typeof usingModuleResource[Symbol.dispose], "function");

async function test_module_reserved_bindings()
{
    const invalid = [
        "named_await", "named_class", "named_yield", "named_static",
        "escaped_await", "escaped_class", "namespace_let",
        "namespace_escaped_let", "shorthand_let", "shorthand_escaped_await",
        "default_escaped_await", "export_await", "export_escaped_await",
        "export_let", "export_class"
    ];
    for (const name of invalid) {
        let error;
        try {
            await import("./fixture_module_reserved_invalid_" + name + ".js");
        } catch (e) {
            error = e;
        }
        assert(error instanceof SyntaxError, true, name);
    }

    const valid = await import("./fixture_module_reserved_valid.js");
    for (const name of ["awaited", "klass", "letValue", "from", "await",
                        "class", "let", "default"])
        assert(valid[name], 42, name);
    assert(valid.namespace.value, 42);
}

await test_module_reserved_bindings();
