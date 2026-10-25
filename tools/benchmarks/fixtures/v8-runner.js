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

/* run_harness.js */
var print = console.log;

function Run() {
  BenchmarkSuite.RunSuites({ NotifyStep: ShowProgress,
                             NotifyError: AddError,
                             NotifyResult: AddResult,
                             NotifyScore: AddScore });
}

var harnessErrorCount = 0;

function ShowProgress(name) {
  print('PROGRESS', name);
}

function AddError(name, error) {
  print('ERROR', name, error);
  print(error.stack);
  harnessErrorCount++;
}

function AddResult(name, result) {
  print('RESULT', name, result);
}

function AddScore(score) {
  print('SCORE', score);
}

// Select one benchmark in a fresh process; keep its original workload.
BenchmarkSuite.suites = BenchmarkSuite.suites.filter(function(suite) {
  if (suite.name !== scriptArgs[1]) return false;
  suite.benchmarks = suite.benchmarks.filter(function(benchmark) {
    return benchmark.name === scriptArgs[2];
  });
  return suite.benchmarks.length === 1;
});
if (BenchmarkSuite.suites.length !== 1) throw new Error('Unknown benchmark');
// Decrypt requires a ciphertext fixture, prepared outside measurement.
if (scriptArgs[1] === 'Crypto' && scriptArgs[2] === 'Decrypt') encrypt();

try {
  Run();
} catch (e) {
  print('*** Run() failed');
  print(e.stack || e);
}

if (harnessErrorCount > 0) {
  // Throw an error so that 'duk' has a non-zero exit code which helps
  // automatic testing.
  throw new Error('Benchmark had ' + harnessErrorCount + ' errors');
}

if (BenchmarkSuite.version !== '7') throw new Error('Expected V8 version 7');
console.log('V8_DETAILS ' + JSON.stringify(BenchmarkSuite.suites.map(function(suite) {
return {name:suite.name,reference:suite.reference,results:suite.results.map(function(result) {
return {name:result.benchmark.name,microseconds_per_iteration:result.time};})};})));
