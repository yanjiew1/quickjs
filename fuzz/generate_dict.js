/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Function to recursively iterate through built-in names.
function collectBuiltinNames(obj, visited = new Set(), result = new Set()) {
  // Check if the object has already been visited to avoid infinite recursion.
  if (visited.has(obj))
    return;

  // Add the current object to the set of visited objects
  visited.add(obj);
  // Get the property names of the current object
  const properties = Object.getOwnPropertyNames(obj);
  // Iterate through each property
  for (var i=0; i < properties.length; i++) {
    var property = properties[i];
    if (property != "collectBuiltinNames" && typeof property != "number")
      result.add(property);
    // Check if the property is an object and if so, recursively iterate through its properties.
    if (typeof obj[property] === 'object' && obj[property] !== null)
      collectBuiltinNames(obj[property], visited, result);
  }
  return result;
}

// Start the recursive iteration with the global object.
console.log(Array.from(collectBuiltinNames(this)).join('\n'));
