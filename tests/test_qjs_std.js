/* --std must finish before a non-module script starts. */
function assert(actual, expected)
{
    if (!Object.is(actual, expected))
        throw Error("qjs --std assertion failed: " + actual + " !== " + expected);
}

assert(typeof std, "object");
assert(typeof os, "object");
assert(typeof std.sprintf, "function");
assert(typeof os.getcwd, "function");
assert(std.evalScript("40 + 2"), 42);
let sum = 0;
for (let i = 0; i < 7; i++)
    sum += i;
assert(std.sprintf("sum=%d", sum), "sum=21");
const [directory, error] = os.getcwd();
assert(error, 0);
assert(typeof directory, "string");
print("qjs-cli-std-ok");
