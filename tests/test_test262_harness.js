/* Synthetic inputs for the patched buildString runner unit. */
const nativeRange = $262.codePointRange;
function check(actual, expected) { if (actual !== expected) throw Error('range helper mismatch'); }
function reference(args) {
    const pieces = [String.fromCodePoint.apply(null, args.loneCodePoints)];
    for (const range of args.ranges) {
        let chunk = [];
        for (let cp = range[0]; cp <= range[1]; cp++) {
            chunk.push(cp);
            if (chunk.length === 500) {
                pieces.push(String.fromCodePoint.apply(null, chunk)); chunk = [];
            }
        }
        pieces.push(String.fromCodePoint.apply(null, chunk));
    }
    return pieces.join('');
}
const priorRange = Object.hasOwn(globalThis, 'codePointRange');
const priorPoint = Object.hasOwn(globalThis, 'codePoint');
for (const args of [
    {loneCodePoints:[], ranges:[]},
    {loneCodePoints:[0, 0xd800, 0xdfff, 0x10000, 0x10ffff],
     ranges:[[0,70000],[0xd7ff,0xe001],[0xffff,0x10002],[0x10fffe,0x10ffff],[3,2]]}
]) {
    const expected = reference(args);
    let calls = 0;
    globalThis.$262 = {codePointRange(first, end) { calls++; return nativeRange(first, end); }};
    check(buildString(args), expected);
    check(calls, args.ranges.length);
    globalThis.$262 = {};
    check(buildString(args), expected);
    delete globalThis.$262;
    check('$262' in globalThis, false);
    check(buildString(args), expected);
    check(Object.hasOwn(globalThis, 'codePointRange'), priorRange);
    check(Object.hasOwn(globalThis, 'codePoint'), priorPoint);
}
