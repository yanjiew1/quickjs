/* One-time initialization for immutable ICU4C provider metadata.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
#include "once.h"

#ifdef _WIN32
typedef struct QJSICUOnceCall {
    void (*initialize)(void);
} QJSICUOnceCall;

static BOOL CALLBACK initialize_once(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
    QJSICUOnceCall *call = parameter;

    call->initialize();
    return TRUE;
}
#endif

void qjs_icu4c_once(QJSICUOnce *once, void (*initialize)(void))
{
#ifdef _WIN32
    QJSICUOnceCall call = {initialize};

    InitOnceExecuteOnce(once, initialize_once, &call, NULL);
#else
    pthread_once(once, initialize);
#endif
}
