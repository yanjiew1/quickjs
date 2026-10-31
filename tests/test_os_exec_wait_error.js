/* os.exec must stop waiting when a child has already been reaped.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT
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
import * as os from "os";

os.signal(os.SIGCHLD, undefined);
try {
    const status = os.exec(["/bin/true"], { usePath: false });
    if (status > 0)
        throw Error("unexpected child status: " + status);
} finally {
    os.signal(os.SIGCHLD, null);
}

if (os.platform === "linux") {
    const source = os.open("/dev/null", os.O_RDONLY);
    if (source < 0)
        throw Error("cannot open descriptor fixture");
    try {
        const high = os.dup2(source, 2048);
        if (high === 2048) {
            try {
                const status = os.exec(["/bin/sh", "-c",
                                        "test ! -e /proc/self/fd/2048"],
                                       { usePath: false });
                if (status !== 0)
                    throw Error("os.exec leaked descriptor 2048: " + status);
            } finally {
                os.close(2048);
            }
        } else if (high !== -9 && high !== -24) {
            throw Error("cannot create high descriptor fixture: " + high);
        }
    } finally {
        os.close(source);
    }
}
