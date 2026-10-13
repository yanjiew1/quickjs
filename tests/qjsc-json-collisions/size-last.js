import peer from "./a_attributes_size.js";
import numericPeer from "./a_1_attributes_size.js";
import payload from "./a.json" with { type: "json" };
if (payload.value !== 73 || peer !== 75 || numericPeer !== 77)
    throw Error("JSON sidecar C-name collision changed module values");
print("qjsc-json-collision-ok");
