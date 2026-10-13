import * as namespace from "./fixture_module_names.js";
import { "*" as importedStar } from "./fixture_module_names.js";
export { namespace as "namespace", importedStar as "imported star" };
export { "*" as forwardedStar, "*", "" as empty, "x y" as "forwarded name",
         "0", "01", "𠮷" as supplementary, "\0" as nul,
         "\uD83D\uDE80" as rocket, default } from "./fixture_module_names.js";
export * as "direct namespace" from "./fixture_module_names.js";
