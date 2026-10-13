import * as other from "./fixture_module_attributes_cycle_b.js" with { type: "js" };
export const marker = {};
export function fromB() { return other.marker; }
globalThis.attributeCycleAEvaluations =
    (globalThis.attributeCycleAEvaluations ?? 0) + 1;
