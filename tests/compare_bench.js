/* Run with qjs --std; directories contain measured runs 1 through 7. */
function read(path) {
    const data = std.loadFile(path);
    if (data === null)
        throw Error("cannot read " + path);
    return JSON.parse(data);
}

function medians(directory, prefix) {
    const samples = [];
    for (let i = 1; i <= 7; i++)
        samples.push(read(directory + "/" + prefix + "-" + i + ".json"));
    const keys = Object.keys(samples[0]).sort();
    for (const sample of samples) {
        if (JSON.stringify(Object.keys(sample).sort()) !== JSON.stringify(keys))
            throw Error("benchmark set changed in " + directory);
    }
    const result = {};
    for (const key of keys) {
        const values = samples.map(sample => sample[key]);
        if (values.some(value => !Number.isFinite(value) || value <= 0))
            throw Error("invalid measurement: " + key);
        values.sort((a, b) => a - b);
        result[key] = values[3];
    }
    return result;
}

if (scriptArgs.length < 4)
    throw Error("expected baseline-directory candidate-directory filename-prefix...");
const baseline = {}, candidate = {};
for (const prefix of scriptArgs.slice(3)) {
    const before = medians(scriptArgs[1], prefix);
    const after = medians(scriptArgs[2], prefix);
    for (const name of Object.keys(before)) {
        if (name in baseline)
            throw Error("duplicate benchmark: " + name);
    }
    Object.assign(baseline, before);
    Object.assign(candidate, after);
}
if (JSON.stringify(Object.keys(baseline)) !== JSON.stringify(Object.keys(candidate)))
    throw Error("baseline and candidate benchmark sets differ");
let sum = 0;
const regressions = [];
const benchmarks = [];
for (const name of Object.keys(baseline)) {
    const ratio = candidate[name] / baseline[name];
    const result = { name, baseline: baseline[name], candidate: candidate[name], ratio };
    benchmarks.push(result);
    sum += Math.log(ratio);
    if (ratio > 1.05)
        regressions.push(result);
}
const geometricMean = Math.exp(sum / Object.keys(baseline).length);
console.log(JSON.stringify({ geometricMean, benchmarks, regressions }, null, 2));
if (geometricMean > 1.03 || regressions.length)
    std.exit(1);
