import * as std from "std";
if (typeof std.printf !== "function" || typeof std.loadFile !== "function")
    throw Error("non-JSON std imports must retain native dispatch");
print("qjsc-native-std-ok");
