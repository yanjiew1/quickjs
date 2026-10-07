export let value = 1;
export { value as "", value as "*", value as "x y", value as "0",
         value as "01", value as "𠮷", value as "\0",
         value as "\uD83D\uDE80" };
export function update(next) { value = next; }
export default "default value";
