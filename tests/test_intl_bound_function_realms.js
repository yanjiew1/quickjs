/* QuickJS Intl built-in function creation realm regression.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 * Intended for a host providing $262.createRealm().
 */
function check(condition, message) {
    if (!condition)
        throw Error(message);
}

const foreign = $262.createRealm().global;
for (const [service, property, length, argumentsList] of [
    ["Collator", "compare", 2, ["a", "b"]],
    ["NumberFormat", "format", 1, [1]],
    ["DateTimeFormat", "format", 1, [0]],
]) {
    const getter = Object.getOwnPropertyDescriptor(
        foreign.Intl[service].prototype, property).get;
    const instance = new Intl[service]("en");
    const fn = getter.call(instance);
    check(fn === getter.call(instance), service + " stable cached function");
    check(Object.getPrototypeOf(fn) === foreign.Function.prototype,
          service + " uses getter creation realm");
    check(fn.name === "" && fn.length === length,
          service + " bound name and length");
    check(Object.getOwnPropertyNames(fn).join(",") === "length,name",
          service + " property creation order");
    for (const key of ["length", "name"]) {
        const descriptor = Object.getOwnPropertyDescriptor(fn, key);
        check(descriptor.configurable && !descriptor.enumerable &&
              !descriptor.writable, service + " property attributes");
    }
    check(Object.prototype.toString.call(fn) === "[object Function]",
          service + " callable class");
    check(Object.isExtensible(fn), service + " extensible built-in function");
    check(!Object.hasOwn(fn, "prototype"), service + " no own prototype");
    let constructorError;
    try { Reflect.construct(fn, []); } catch (error) { constructorError = error; }
    check(Object.getPrototypeOf(constructorError) === TypeError.prototype,
          service + " nonconstructor rejection in caller realm");
    check(typeof fn(...argumentsList) !== "undefined", service + " ordinary call");

    for (const argument of [Symbol(), { [Symbol.toPrimitive]() { return {}; } }]) {
        let error;
        try { fn(argument); } catch (caught) { error = caught; }
        check(Object.getPrototypeOf(error) === foreign.TypeError.prototype,
              service + " coercion error in creation realm");
    }
    const sentinel = {};
    let error;
    try {
        fn({ [Symbol.toPrimitive]() { throw sentinel; } });
    } catch (caught) {
        error = caught;
    }
    check(error === sentinel, service + " exact abrupt completion identity");
    let reentries = 0;
    fn({ [Symbol.toPrimitive]() {
        reentries++;
        fn(...argumentsList);
        return service === "Collator" ? "a" : 1;
    } });
    check(reentries === 1, service + " one direct callback with nested invocation");

    /* Native initial name remains empty after arbitrary public name mutation. */
    Object.defineProperty(fn, "name", { get() { throw sentinel; } });
    check(Function.prototype.toString.call(fn).includes("[native code]"),
          service + " toString does not read mutable name");
}
