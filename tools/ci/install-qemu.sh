#!/bin/sh
# Copyright (c) 2026 Yan-Jie Wang
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

# Run inside the privileged official Debian trixie container used by CI.
# Its signed distribution packages include the upstream page-flags race fix.
set -eu

arch=${1:?supply the selected QEMU architecture}
case "$arch" in
    arm|i386|riscv64|s390x|ppc64le) ;;
    *) echo "unsupported CI emulator: $arch" >&2; exit 2 ;;
esac

apt-get update
apt-get install -y --no-install-recommends qemu-user
version=$(dpkg-query -W -f '${Version}' qemu-user)
dpkg --compare-versions "$version" ge '1:10.0.13'

if [ ! -f /proc/sys/fs/binfmt_misc/register ]; then
    mount -t binfmt_misc binfmt_misc /proc/sys/fs/binfmt_misc
fi
config="/usr/share/qemu/binfmt.d/qemu-$arch.conf"
entry="/proc/sys/fs/binfmt_misc/qemu-$arch"
test -f "$config"
# Replace only this guest handler; registering x86_64 would intercept the
# native runner. F retains the static interpreter after this container exits.
if [ -f "$entry" ]; then
    echo -1 > "$entry"
fi
cat "$config" > /proc/sys/fs/binfmt_misc/register
grep -q '^flags:.*F' "$entry"
printf 'Debian qemu-user: %s\n' "$version"
"/usr/bin/qemu-$arch" --version
cat "$entry"
