import payload from "./payload.so" with { type: "json" };
import payloadAgain from "./payload.so" with { type: "json" };
import otherType from "./payload.so" with { type: "json5" };
import extended from "./extended.so" with { type: "json5" };
import plain from "./plain.json";
import plainAgain from "./plain.json" with {};
import namedStd from "std" with { type: "json" };
if (payload.value !== 42 || payload.kind !== "plain JSON .so")
    throw Error("qjsc did not serialize the JSON .so payload");
if (payload !== payloadAgain)
    throw Error("equal attributed imports did not retain identity");
if (otherType.value !== 42 || otherType === payload)
    throw Error("distinct serialized requests lost their attribute identity");
if (extended.value !== 51 || plain.value !== 61 || plain !== plainAgain)
    throw Error("JSON5 or absent/empty attribute preloads were not retained");
if (namedStd.kind !== "JSON before a system module name")
    throw Error("qjsc selected the std C module before JSON attributes");
print("qjsc-json-attribute-payload-ok");
