import peer from "./a_attributes.js";
import numericPeer from "./a_1_attributes.js";
import payload from "./a.json" with { type: "json" };
if (payload.value !== 73 || peer !== 74 || numericPeer !== 76)
    throw Error("JSON sidecar C-name collision changed module values");
print("qjsc-json-collision-ok");
