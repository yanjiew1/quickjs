import { assert } from "./assert.js";
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
