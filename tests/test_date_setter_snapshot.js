function assert(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw Error(message || `${actual} !== ${expected}`);
}

/* Argument coercion may mutate this Date; setters retain the original slot. */
for (const [method, first, rest] of [
    ["setDate", 12, []], ["setFullYear", 2025, [6, 14]],
    ["setHours", 15, [26, 37, 48]], ["setMilliseconds", 789, []],
    ["setMinutes", 41, [52, 63]], ["setMonth", 8, [19]],
    ["setSeconds", 31, [42]], ["setYear", 125, []], ["setUTCDate", 12, []],
    ["setUTCFullYear", 2025, [6, 14]], ["setUTCHours", 15, [26, 37, 48]],
    ["setUTCMilliseconds", 789, []], ["setUTCMinutes", 41, [52, 63]],
    ["setUTCMonth", 8, [19]], ["setUTCSeconds", 31, [42]]
]) {
    const original = Date.parse("2024-04-23T10:11:12.345Z");
    const expected = new Date(original);
    expected[method](first, ...rest);
    const changed = new Date(original);
    changed[method]({ valueOf() { changed.setTime(0); return first; } }, ...rest);
    assert(changed.getTime(), expected.getTime(), `${method} snapshots DateValue`);
}
