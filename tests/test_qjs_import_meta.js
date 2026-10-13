import * as std from "std";
import * as os from "os";
import { url, main } from "./fixture_qjs_import_meta.js";

function assert(actual, expected)
{
    if (!Object.is(actual, expected))
        throw Error("qjs import.meta assertion failed: " + actual +
                    " !== " + expected);
}

function fileURL(filename)
{
    /* The libc metadata helper preserves raw file names on Windows. */
    if (os.platform !== "win32") {
        const [canonical, error] = os.realpath(filename);
        assert(error, 0);
        filename = canonical;
    }
    return "file://" + filename;
}

assert(globalThis.std, std);
assert(globalThis.os, os);
assert(import.meta.main, true);
assert(import.meta.url, fileURL(scriptArgs[0]));
assert(main, false);
assert(url, fileURL("fixture_qjs_import_meta.js"));
print("qjs-cli-file-meta-ok");
