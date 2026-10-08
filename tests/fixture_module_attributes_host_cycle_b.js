import * as other from "attribute-cycle-a" with { kind: "cycle" };
export const marker = {};
export function fromA() { return other.marker; }
