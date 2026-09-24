# Validation checkpoint

Baseline before this change: `0653411`.

| Check | Baseline | After Atomics split and core header fixes |
| --- | --- | --- |
| GCC `CONFIG_WERROR=y` `make all` and `make test` | Pass | Pass |
| Clang `CONFIG_WERROR=y` `make all` and `make test` | Pass | Pass |
| `make test2` with `test262.conf -a` | 58/83558 errors, 3356 excluded, 6000 skipped | 58/83558 errors, 3356 excluded, 6000 skipped |
| `make test2-check` | 58/59 errors | 58/59 errors |

The Test262 failures match the recorded baseline in `TODO` and the previous
validation. `test262_errors.txt` is the repository's list of expected errors.
