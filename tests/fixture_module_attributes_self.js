import * as self from "./fixture_module_attributes_self.js" with { type: "js" };
export const marker = {};
export function readMarker() { return self.marker; }
globalThis.attributeSelfEvaluations =
    (globalThis.attributeSelfEvaluations ?? 0) + 1;
