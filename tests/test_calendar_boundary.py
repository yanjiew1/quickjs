#!/usr/bin/env python3
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

"""Check the build-host boundary export without executing a target binary."""
import datetime
import json
import subprocess
import sys

raw = subprocess.run([sys.argv[1]], stdout=subprocess.PIPE, check=True, timeout=30)
data = json.loads(raw.stdout.decode('ascii'))
assert set(data) == {'schema', 'role', 'years'}
assert data['schema'] == 1 and data['role'] == 'approximation'
assert len(data['years']) == 2
epoch = datetime.date(1970, 1, 1)
expected = [('chinese', 1899, datetime.date(1899, 2, 10), datetime.date(1900, 1, 31)),
            ('dangi', 2050, datetime.date(2050, 1, 23), datetime.date(2051, 2, 11))]
for row, (name, year, first, end) in zip(data['years'], expected):
    assert row['calendar'] == name and row['year'] == year
    assert row['first_day'] == (first - epoch).days
    assert row['end_day'] == (end - epoch).days
    assert len(row['months']) == 12 + bool(row['leap_month'])
    assert all(type(m) is int and m in (29, 30) for m in row['months'])
    assert sum(row['months']) == row['end_day'] - row['first_day']
