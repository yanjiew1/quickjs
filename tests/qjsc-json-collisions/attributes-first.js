import payload from "./a.json" with { type: "json" };
import peer from "./a_attributes.js";
if (payload.value !== 73 || peer !== 74)
    throw Error("JSON sidecar C-name collision changed module values");
print("qjsc-json-collision-ok");
