import * as ordinary from "./fixture_module_attributes_dual.js";
import value from "./fixture_module_attributes_dual.js" with { type: "json" };
import "./fixture_module_attributes_dual.js" with { type: "json" };
export { default as again } from "./fixture_module_attributes_dual.js"
    with { type: "json" };
export * as jsonNamespace from "./fixture_module_attributes_dual.js"
    with { type: "json" };
export { ordinary, value };
