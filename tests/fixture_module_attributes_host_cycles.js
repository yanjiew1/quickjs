const options = { with: { kind: "cycle" } };
const self = await import("attribute-self", options);
if (self.readMarker() !== self.marker ||
    await import("attribute-self", options) !== self)
    throw new Error("attributed self module identity");
const a = await import("attribute-cycle-a", options);
const b = await import("attribute-cycle-b", options);
if (a.fromB() !== b.marker || b.fromA() !== a.marker ||
    await import("attribute-cycle-a", options) !== a ||
    await import("attribute-cycle-b", options) !== b)
    throw new Error("attributed module cycle identity");
export const ok = true;
