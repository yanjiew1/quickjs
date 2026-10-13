/* Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
export const usingModuleEvents = [];
using resource = {
    [Symbol.dispose]() { usingModuleEvents.push("disposed"); }
};
export { resource as usingModuleResource };
usingModuleEvents.push("evaluated");
