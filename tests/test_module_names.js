/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

import { assert } from "./assert.js";
import * as reexported from "./fixture_module_names_reexport.js";
import * as chained from "./fixture_module_names_chain.js";
import * as namespaceMerge from "./fixture_module_names_namespace_merge.js";
import * as mixedMerge from "./fixture_module_names_mixed_merge.js";
import * as starMerge from "./fixture_module_names_star_merge.js";
import * as sameBinding from "./fixture_module_names_same_binding.js";
import * as source from "./fixture_module_names.js";
import * as namespaces from "./fixture_module_names_namespace.js";
import { "x y" as namedNamespace } from "./fixture_module_names_namespace.js";

for (const name of ["", "*", "x y", "0", "𠮷", "\0", "\uD83D\uDE80"]) {
    assert(namespaces[name] === source);
    assert(namespaces[name]["*"], 1);
}
assert(namedNamespace === source);
source.update(2);
assert(namedNamespace["*"], 2);
source.update(1);

async function assert_module_syntax_error(name)
{
    try {
        await import("./fixture_module_names_invalid_" + name + ".js");
    } catch (e) {
        assert(e instanceof SyntaxError, true, name);
        return;
    }
    assert(false, true, "SyntaxError expected: " + name);
}

for (const name of ["namespace_high_surrogate", "namespace_low_surrogate",
                   "namespace_duplicate", "destination_surrogate",
                   "import_surrogate", "import_missing_alias"])
    await assert_module_syntax_error(name);

function check_forwarded_value(expected)
{
    for (const name of ["forwardedStar", "*", "empty", "forwarded name", "0",
                       "01", "supplementary", "nul", "rocket", "imported star"]) {
        assert(reexported[name], expected, name);
        assert(chained[name], expected, name);
    }
    assert(chained["chained star"], expected);
    assert(sameBinding["*"], expected);
    assert(reexported.namespace === source);
    assert(reexported["direct namespace"] === source);
    assert(chained["chained namespace"] === source);
    assert(namespaceMerge.namespace === source);
    assert(reexported.default, "default value");
    assert(chained["chained default"], "default value");
    assert(Object.hasOwn(chained, "default"), false);
}

check_forwarded_value(1);
source.update(7);
check_forwarded_value(7);
source.update(1);
assert(Object.hasOwn(mixedMerge, "mixed"), false);
assert(Object.hasOwn(starMerge, "*"), false);
assert(starMerge.value, 1);

for (const name of ["source_high_surrogate", "source_low_surrogate",
                   "reexport_destination_surrogate", "quoted_local",
                   "quoted_local_alias", "quoted_local_destination",
                   "quoted_local_mixed", "duplicate_destination", "missing_star",
                   "mixed_ambiguity", "star_ambiguity"])
    await assert_module_syntax_error(name);
