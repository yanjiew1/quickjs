import { assert, assertThrows } from "./assert.js";
import * as namespace from "./fixture_module.js";
import * as selfNamespace from "./test_module.js";

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
