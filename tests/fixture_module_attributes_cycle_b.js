import * as other from "./fixture_module_attributes_cycle_a.js" with { type: "js" };
export const marker = {};
export function fromA() { return other.marker; }
globalThis.attributeCycleBEvaluations =
    (globalThis.attributeCycleBEvaluations ?? 0) + 1;
