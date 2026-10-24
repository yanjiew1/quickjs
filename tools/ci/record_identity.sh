#!/bin/sh
# Shared CI evidence/reporting; workflow steps retain execution policy.
set -e

git rev-parse HEAD > ci-identity.txt
printf 'run=%s attempt=%s runner=%s arch=%s\n' \
  "$GITHUB_RUN_ID" "$GITHUB_RUN_ATTEMPT" "$RUNNER_OS" "$RUNNER_ARCH" >> ci-identity.txt
uname -a >> ci-identity.txt
meson --version >> ci-identity.txt
python3 --version >> ci-identity.txt
if test -d test262/.git; then
  printf 'test262=' >> ci-identity.txt
  git -C test262 rev-parse HEAD >> ci-identity.txt
fi
if test -f build/.ninja_log; then
  # Keep raw Ninja actions separately from compile_commands.json compiler entries.
  cp build/.ninja_log build/ninja-actions.log
fi
