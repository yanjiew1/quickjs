The executables included in this archive run on Linux, Mac, Windows,
FreeBSD, OpenBSD and NetBSD for both the ARM64 and x86_64
architectures.

Platform Notes:

- if you get errors on Linux, you should disable the binfmt_misc
  module which automatically invokes wine with Windows executable:

sudo sh -c 'echo -1 > /proc/sys/fs/binfmt_misc/cli'     # remove Ubuntu's MZ interpreter
sudo sh -c 'echo -1 > /proc/sys/fs/binfmt_misc/status'  # remove ALL binfmt_misc entries

- Under Windows, you can rename the executables with a .exe extension.

- Use the --assimilate option to build a platform specific binary for
  better startup time:

  ./qjs --assimilate

- See https://github.com/jart/cosmopolitan for more information about
  platform specific issues.

QuickJS Javascript Engine

Copyright (c) 2017-2021 Fabrice Bellard
Copyright (c) 2017-2021 Charlie Gordon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
