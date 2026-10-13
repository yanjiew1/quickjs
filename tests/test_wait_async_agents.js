/*
 * Test262 host native wait wake and departing-agent regressions
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 * Run with run-test262, which supplies $262.agent.
 */
function report(expected)
{
    const start = $262.agent.monotonicNow();
    for (;;) {
        const value = $262.agent.getReport();
        if (value !== null) {
            if (value !== expected)
                throw Error("expected " + expected + ", got " + value);
            return;
        }
        if ($262.agent.monotonicNow() - start > 10000)
            throw Error("agent report timeout: " + expected);
        $262.agent.sleep(1);
    }
}

if (typeof Atomics === "object") {
    if (typeof Atomics.waitAsync !== "function")
        throw Error("Atomics.waitAsync is missing");
    $262.agent.start(`
        $262.agent.report("left");
        $262.agent.leaving();
    `);
    report("left");
    $262.agent.start(`
        $262.agent.receiveBroadcast(shared => {
            const words = new Int32Array(shared);
            const wait = Atomics.waitAsync(words, 0, 0);
            $262.agent.report("waiting");
            wait.value.then(value => {
                $262.agent.report(value);
                $262.agent.leaving();
            });
        });
        $262.agent.report("registered");
    `);
    report("registered");
    const words = new Int32Array(new SharedArrayBuffer(8));
    $262.agent.broadcast(words.buffer);
    report("waiting");
    if (Atomics.notify(words, 0, 1) !== 1)
        throw Error("native wait was not published");
    report("ok");
    $262.agent.broadcast(words.buffer); // departed agents must not keep this pending

    for (let round = 0; round < 8; round++) {
        $262.agent.start(`
            $262.agent.receiveBroadcast(() => {
                Promise.resolve().then(() => {
                    $262.agent.report("departed");
                    $262.agent.leaving();
                });
            });
            $262.agent.report("ready");
        `);
        report("ready");
        $262.agent.broadcast(words.buffer);
        // Race a new publication with the owner's queued leaving reaction.
        $262.agent.broadcast(words.buffer);
        report("departed");
    }
    $262.agent.start(`
        const words = new Int32Array(new SharedArrayBuffer(4));
        Atomics.waitAsync(words, 0, 0);
        $262.agent.report("pending cleanup");
    `);
    report("pending cleanup");
    // Host cleanup must wake and join this agent, cancelling on its owner.

}
