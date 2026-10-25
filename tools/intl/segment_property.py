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

"""Bounded host bridge to the explicit build-machine Unicode property tool.

Only successful Unicode 18 replies enter the cache. JSON is a preparation
transport, never runtime data or a substitute for source authentication.
"""
from collections import namedtuple
import json
import math
import os
import signal
import subprocess
import threading
import time

from cldr_xml import DataError
from segment_expression import PROPERTY
from segment_sets import checked_ranges

PropertyLimits = namedtuple('PropertyLimits',
                            'stdout stderr seconds total_seconds queries ranges retained_pairs')
DEFAULT_LIMITS = PropertyLimits(524288, 8192, 10, 120, 256, 16384, 262144)


def _capture(arguments, seconds, stdout_limit, stderr_limit):
    """Drain both pipes under byte/time limits; kill and reap failed children.

    Separate bounded readers also work on Windows, where selectors do not
    support anonymous pipes. The owned C tool does not spawn descendants;
    POSIX failure cleanup additionally kills its process group.
    """
    wake = threading.Event()
    states, readers = [], []

    def read(stream, limit, state):
        size = 0
        try:
            while True:
                data = stream.read1(min(4096, limit + 1 - size))
                if not data:
                    break
                size += len(data)
                if size > limit:
                    state['error'] = 'property tool output budget exceeded'
                    break
                state['chunks'].append(data)
        except OSError as error:
            state['error'] = 'property tool pipe failed: ' + str(error)
        finally:
            stream.close()
            state['done'].set()
            wake.set()

    try:
        child = subprocess.Popen(arguments, stdin=subprocess.DEVNULL,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 start_new_session=os.name == 'posix')
    except OSError as error:
        raise DataError('cannot start segmentation property tool: ' + str(error))
    try:
        for stream, limit in ((child.stdout, stdout_limit), (child.stderr, stderr_limit)):
            state = {'chunks': [], 'done': threading.Event(), 'error': None}
            states.append(state)
            thread = threading.Thread(target=read, args=(stream, limit, state), daemon=True)
            try:
                thread.start()
            except (RuntimeError, OSError) as error:
                raise DataError('cannot start property pipe reader: ' + str(error))
            readers.append(thread)
        deadline = time.monotonic() + seconds
        while True:
            for state in states:
                if state['error']:
                    raise DataError(state['error'])
            if child.poll() is not None and all(state['done'].is_set() for state in states):
                break
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise DataError('segmentation property tool timed out')
            wake.wait(min(remaining, 0.05))
            wake.clear()
        return child.returncode, b''.join(states[0]['chunks']), b''.join(states[1]['chunks'])
    finally:
        # Closing a stream from another thread can wait for its read lock.
        # Kill first so readers observe EOF, then join them under a bound.
        if child.poll() is None or any(not state['done'].is_set() for state in states):
            try:
                if os.name == 'posix':
                    os.killpg(child.pid, signal.SIGKILL)
                else:
                    child.kill()
            except ProcessLookupError:
                pass
            except PermissionError:
                # A group signal can be denied (observed on Darwin). The
                # explicitly owned tool has no descendants; killing the child
                # directly still permits bounded wait/pipe cleanup.
                try:
                    child.kill()
                except ProcessLookupError:
                    pass
        try:
            child.wait(timeout=2)
        except subprocess.TimeoutExpired:
            raise DataError('segmentation property tool could not be reaped')
        for reader in readers:
            reader.join(timeout=1)
            if reader.is_alive():
                raise DataError('segmentation property pipe cleanup timed out')
        for stream in (child.stdout, child.stderr):
            if not stream.closed:
                stream.close()


def _object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise DataError('duplicate property reply key')
        result[key] = value
    return result


def _constant(value):
    raise DataError('non-finite property reply number')


class PropertyResolver:
    """One explicit host executable, exact-query cache and generation budget."""
    def __init__(self, command, limits=DEFAULT_LIMITS):
        if isinstance(command, (str, os.PathLike)):
            command = (str(command),)
        if (not isinstance(command, (tuple, list)) or not command or
                any(not isinstance(part, str) or not part or '\0' in part for part in command)):
            raise DataError('invalid segmentation property command')
        if not os.path.dirname(command[0]):
            raise DataError('explicit segmentation property executable path required')
        if not isinstance(limits, PropertyLimits):
            raise DataError('invalid segmentation property limits')
        # Callers may tighten this prepared profile, not silently remove its
        # resource bounds. Compare integers before converting for finiteness.
        for name, value, maximum in zip(limits._fields, limits, DEFAULT_LIMITS):
            numeric = type(value) in (int, float) if name in ('seconds', 'total_seconds') else type(value) is int
            if not numeric or value <= 0 or value > maximum or not math.isfinite(value):
                raise DataError('invalid segmentation property limits')
        self.command, self.limits = tuple(command), limits
        self.cache, self.pairs, self.elapsed = {}, 0, 0

    def __call__(self, query):
        if (not isinstance(query, str) or len(query) > 191 or
                not PROPERTY.fullmatch(query) or any(len(part) > 95 for part in query.split('='))):
            raise DataError('unsupported segmentation property spelling')
        if query in self.cache:
            return self.cache[query]
        if len(self.cache) >= self.limits.queries:
            raise DataError('segmentation distinct property budget exceeded')
        remaining = self.limits.total_seconds - self.elapsed
        if remaining <= 0:
            raise DataError('segmentation cumulative property time budget exceeded')
        started = time.monotonic()
        try:
            code, output, diagnostic = _capture(self.command + (query,),
                                                 min(remaining, self.limits.seconds),
                                                 self.limits.stdout, self.limits.stderr)
        except DataError as error:
            raise DataError('{}: {}'.format(query, error))
        finally:
            self.elapsed += time.monotonic() - started
        if code or diagnostic:
            raise DataError('{}: property tool failed (status {}): {}'.format(
                query, code, diagnostic.decode('utf-8', errors='replace')))
        try:
            data = json.loads(output.decode('utf-8'), object_pairs_hook=_object,
                              parse_constant=_constant)
        except (ValueError, UnicodeError, RecursionError) as error:
            raise DataError('{}: invalid property reply: {}'.format(query, error))
        if (not isinstance(data, dict) or set(data) != {'unicode', 'ranges'} or
                data['unicode'] != '18.0.0'):
            raise DataError(query + ': incompatible Unicode property reply')
        try:
            result = checked_ranges(data['ranges'], self.limits.ranges)
        except DataError as error:
            raise DataError('{}: {}'.format(query, error))
        if self.pairs + len(result) > self.limits.retained_pairs:
            raise DataError('segmentation property cache range budget exceeded')
        self.pairs += len(result)
        self.cache[query] = result
        return result
