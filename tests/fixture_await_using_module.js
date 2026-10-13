/* Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
export const awaitUsingModuleEvents = [];
using first = { [Symbol.dispose]() { awaitUsingModuleEvents.push("sync"); } };
await using resource = {
    [Symbol.asyncDispose]() {
        awaitUsingModuleEvents.push("async start");
        return Promise.resolve().then(() => awaitUsingModuleEvents.push("async end"));
    }
};
export { resource as awaitUsingModuleResource };
awaitUsingModuleEvents.push("evaluated");
