import * as other from "attribute-cycle-b" with { kind: "cycle" };
export const marker = {};
export function fromB() { return other.marker; }
