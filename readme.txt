The main documentation is in doc/quickjs.pdf or doc/quickjs.html.

Engine and library implementation sources are in src/. Command-line tools,
the REPL, and Unicode utilities are in tools/. Public headers stay at the
repository root. Generated C sources and compiler dependencies are in .obj/.

Build with make; run the built-in tests with make test. Optional LTO builds
use make CONFIG_LTO=y. Use make clean when changing compiler or build flags.
