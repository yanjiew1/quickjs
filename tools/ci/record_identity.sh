#!/bin/sh
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

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
