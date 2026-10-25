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

/* Build pinned zic on the host; never link this tool into the runtime. */

/* Host data generation needs no message catalog or libintl dependency. */
#define HAVE_GETTEXT 0

#if defined(__MINGW32__)
/* Upstream's documented native Windows portability settings.  The
   HAVE_DIRECT_H branch supplies uid_t/gid_t and maps mkdir to _mkdir.
   MinGW declares mempcpy, but upstream's default detection misses it.
   Upstream copies zone aliases when hard and symbolic links are absent. */
#define HAVE_DIRECT_H 1
#define HAVE_FCHMOD 0
#define HAVE_LINK 0
#define HAVE_SYMLINK 0
#define HAVE_MEMPCPY 1
#define HAVE_PWD_H 0
#define HAVE_GETRANDOM 0
#endif

/* This translation unit belongs only to HOST_ZIC, never libquickjs. */
#include "zic.c"
