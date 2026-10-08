import value from "./invalid.so" with { type: "json" };
throw Error("invalid JSON was accepted as a native module");
