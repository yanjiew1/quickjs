/* Apply an explicit UTC offset before TimeClip, at both Date boundaries. */
function assert(actual, expected) {
    if (!Object.is(actual, expected))
        throw Error(`${actual} !== ${expected}`);
}
assert(Date.parse("+275760-09-13T01:00:00+01:00"), 8640000000000000);
assert(Date.parse("-271821-04-19T23:00:00-01:00"), -8640000000000000);
assert(Date.parse("+275760-09-13T00:00:00-00:01"), NaN);
assert(Date.parse("-271821-04-20T00:00:00+00:01"), NaN);
