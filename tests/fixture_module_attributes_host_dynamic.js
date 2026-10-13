/* Used after fixture_module_attributes_host.js by the C API host. */
const first = await import("attribute-target",
    { with: { kind: "first", mode: "test" } });
const equal = await import("attribute-target",
    { with: { mode: "test", kind: ["f", "irst"].join("") } });
const second = await import("attribute-target",
    { with: { kind: "second", mode: "test" } });
/* The cached mode-first request must reject a later missing own kind. */
const alternate = await import("attribute-target",
    { with: { other: "first", mode: "test" } });
const alternateAgain = await import("attribute-target",
    { with: { mode: "test", other: "first" } });
const empty = await import("attribute-empty");
const emptyAgain = await import("attribute-empty", { with: {} });
const sharedFirst = await import("attribute-shared", { with: { kind: "first" } });
const sharedSecond = await import("attribute-shared", { with: { kind: "second" } });
if (first !== equal || first.default !== "first" || second.default !== "second" ||
    alternate !== alternateAgain || alternate.default !== "alternate" ||
    empty !== emptyAgain || sharedFirst !== sharedSecond)
    throw new Error("dynamic module attribute identity");
export const ok = true;
