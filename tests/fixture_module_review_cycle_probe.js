import * as namespace from "./fixture_module_review_cycle_keys.js";

const ownKeys = Reflect.ownKeys(namespace);
const ownNames = Object.getOwnPropertyNames(namespace);
const proxyOwnKeys = Reflect.ownKeys(new Proxy(namespace, {}));
let keysError, entriesError, forInError;
try {
    Object.keys(namespace);
} catch (e) {
    keysError = e;
}
try {
    Object.entries(namespace);
} catch (e) {
    entriesError = e;
}
try {
    for (const key in namespace) {}
} catch (e) {
    forInError = e;
}
export const snapshot = { ownKeys, ownNames, proxyOwnKeys,
                          keysError, entriesError, forInError };
