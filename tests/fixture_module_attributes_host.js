/* Used by the C API host that supports kind, mode and other attributes. */
import first from "attribute-target" with { kind: "first", mode: "test" };
import second from "attribute-target" with { mode: "test", kind: "second" };
import equal from "attribute-target" with { mode: "test", kind: "first" };
/* The mode-first request matches mode before finding kind absent here. */
import alternate from "attribute-target" with { other: "first", mode: "test" };
import empty from "attribute-empty";
import emptyAgain from "attribute-empty" with {};
import "attribute-target" with { mode: "test", kind: "first" };
export { default as indirectFirst } from "attribute-target"
    with { kind: "first", mode: "test" };
import * as sharedFirst from "attribute-shared" with { kind: "first" };
import * as sharedSecond from "attribute-shared" with { kind: "second" };

if (first !== "first" || second !== "second" || equal !== first ||
    alternate !== "alternate" ||
    empty !== "empty" || emptyAgain !== empty || sharedFirst !== sharedSecond)
    throw new Error("static module attribute identity");
export const ok = true;
