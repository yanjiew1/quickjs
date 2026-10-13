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
