/* Cross-realm Intl service behavior migrated from the C allocation test.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
import * as std from "std";
import { check } from "./realm-harness.js";

export function test_intl_service_realms()
{
    if (typeof Intl === "undefined")
        return;
    let realm = std.createRealm();
    const foreign = realm.evalScript(`[
        new Intl.PluralRules('en'), new Intl.ListFormat('en'),
        new Intl.RelativeTimeFormat('en'), new Intl.DisplayNames('en', {type:'region'}),
        (() => { function F() {} F.prototype = null; return F; })(),
        Intl.PluralRules.prototype, Intl.ListFormat.prototype,
        Intl.RelativeTimeFormat.prototype, Intl.DisplayNames.prototype
    ]`);
    realm = null;
    std.gc();

    check(Intl.PluralRules.prototype.select.call(foreign[0], 1), "one");
    check(Intl.ListFormat.prototype.format.call(foreign[1], ["A", "B"]), "A and B");
    check(Intl.RelativeTimeFormat.prototype.format.call(foreign[2], 1, "day"), "in 1 day");
    check(Intl.DisplayNames.prototype.of.call(foreign[3], "US"), "United States");
    for (const [index, Constructor] of [
        Intl.PluralRules, Intl.ListFormat, Intl.RelativeTimeFormat, Intl.DisplayNames,
    ].entries()) {
        const object = Reflect.construct(Constructor,
            ["en", {type: index === 3 ? "region" : undefined}], foreign[4]);
        check(Object.getPrototypeOf(object), foreign[5 + index]);
    }
    std.gc();
    check(typeof foreign.slice(0, 4).map(x => x.resolvedOptions().locale).join(","), "string");
}
