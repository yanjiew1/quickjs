import { assert } from "./assert.js";
import * as valid from "./fixture_module_review_reserved_valid.js";

async function test_reserved_binding_matrix()
{
    const invalid = [
        "named_eval", "named_arguments",
        "named_escaped_eval", "named_escaped_arguments",
        "namespace_eval", "namespace_arguments",
        "namespace_escaped_eval", "namespace_escaped_arguments",
        "default_eval", "default_arguments",
        "named_implements", "named_interface",
        "named_package", "named_private",
        "named_protected", "named_public",
        "named_escaped_yield", "named_escaped_static"
    ];
    for (const name of invalid) {
        let error;
        try {
            await import("./fixture_module_review_invalid_" + name + ".js");
        } catch (e) {
            error = e;
        }
        assert(error instanceof SyntaxError, true, name);
    }
    for (const name of ["evaluation", "argumentValue", "awaited", "yielded",
                        "implemented", "interfaced", "letValue", "packaged",
                        "privateValue", "protectedValue", "publicValue",
                        "staticValue", "enumeration", "klass", "quotedEvaluation",
                        "quotedAwaited", "eval", "arguments", "await", "yield",
                        "reexportEval", "reexportArguments", "reexportAwait",
                        "reexportYield", "reexportEnum", "reexportClass"])
        assert(valid[name], 42, name);
    assert(valid.null.value, 42);
    assert(valid.implements, valid.null);
}

await test_reserved_binding_matrix();
