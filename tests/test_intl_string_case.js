/* Locale selection, original-input contexts and String case-conversion order.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT. */
import * as std from "std";
import { assert, assertThrows } from "./assert.js";

for (const locale of ["tr", "az", "tr-Latn-TR", "az-Cyrl-AZ-u-nu-arab"]) {
    for (const [input, output] of [
        ["I", "ı"], ["İ", "i"], ["I\u0307", "i"],
        ["I\u0323\u0307", "i\u0323"],
        ["I\u0307\u0307", "i\u0307"],
        ["I\u0301\u0307", "ı\u0301\u0307"],
        ["I\u034f\u0307", "ı\u034f\u0307"],
        ["I\0I", "ı\0ı"], ["I\ud800I", "ı\ud800ı"],
        ["I\udc00I", "ı\udc00ı"]
    ])
        assert(input.toLocaleLowerCase(locale), output);
    assert("iı".toLocaleUpperCase(locale), "İI");
    assert("i\u0307".toLocaleUpperCase(locale), "İ\u0307");
}

for (const [input, output] of [
    ["I\u0301", "i\u0307\u0301"], ["J\u0323\u0301", "j\u0307\u0323\u0301"],
    ["Į\u0301", "į\u0307\u0301"], ["ÌÍĨ", "i\u0307\u0300i\u0307\u0301i\u0307\u0303"],
    ["I\u0323", "i\u0323"], ["I\u034f\u0301", "i\u034f\u0301"]
])
    assert(input.toLocaleLowerCase("lt-LT"), output);
for (const [input, output] of [
    ["i\u0307", "I"], ["j\u0323\u0307", "J\u0323"], ["į\u0307", "Į"],
    ["i\u0307\u0307", "I\u0307"],
    ["i\u0301\u0307", "I\u0301\u0307"], ["i\u034f\u0307", "I\u034f\u0307"]
])
    assert(input.toLocaleUpperCase("lt"), output);
// Soft_Dotted includes supplementary letters; UTF-16 halves are not characters.
assert("\u{1d422}\u0307".toLocaleUpperCase("lt"), "\u{1d422}".toUpperCase());
for (const locale of ["lt", "tr", "az", "und"]) {
    for (const input of ["ΟΣ", "ΟΣΑ", "A\u0345Σ", "AΣ\u0345", "AΣ\u0301A", "Σ"])
        assert(input.toLocaleLowerCase(locale), input.toLowerCase());
    assert("ß\u{10428}".toLocaleUpperCase(locale), "SS\u{10400}");
}

assert("I".toLocaleLowerCase(["zz", "tr"]), "i");
assert("I".toLocaleLowerCase(new Intl.Locale("tr")), "ı");
for (const name of ["toLocaleLowerCase", "toLocaleUpperCase"]) {
    const method = String.prototype[name];
    assert(method.length, 0);
    assert(method.name, name);
    assertThrows(TypeError, () => new method());
    assertThrows(TypeError, () => method.call(null, "tr"));
    assertThrows(TypeError, () => method.call(undefined, "tr"));
    assertThrows(TypeError, () => method.call(Symbol(), "tr"));
    assertThrows(RangeError, () => method.call("", "en_US"));
    assertThrows(RangeError, () => method.call("", ["tr", "bad_locale"]));
    assertThrows(RangeError, () => method.call("I", "tr\0"));

    const events = [];
    const receiver = { toString() { events.push("string"); return "I"; } };
    const locales = {
        get length() { events.push("length"); return 2; },
        get 0() { events.push("first"); return "tr"; },
        get 1() { events.push("second"); return "en"; }
    };
    assert(method.call(receiver, locales), name === "toLocaleLowerCase" ? "ı" : "I");
    assert(events.join(","), "string,length,first,second");
    events.length = 0;
    assertThrows(TypeError, () => method.call(null, locales));
    assert(events.length, 0);
    const marker = {};
    try {
        method.call({ toString() { throw marker; } }, locales);
        throw Error("expected receiver conversion failure");
    } catch (error) {
        assert(error === marker);
    }
    assert(events.length, 0);

    const foreign = std.createRealm();
    const foreignMethod = foreign.global.String.prototype[name];
    assertThrows(foreign.global.RangeError, () => foreignMethod.call("", "bad_locale"));
}
