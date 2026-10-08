import * as self from "attribute-self" with { kind: "cycle" };
export const marker = {};
export function readMarker() { return self.marker; }
