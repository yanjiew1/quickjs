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

"""Pair compiled collation bytes with the source/profile of an enclosing QJIN.

This adapter validates framing and resolved identities, never Unicode/CE
semantics. The C compiler and runtime reader retain that responsibility.
"""
import collation_archive as archive
import collation_bindings
import collation_groups
import collation_root
from cldr_xml import DataError


def identities(view):
    """Decode stable program identities, not offsets or compiler history."""
    labels = view['sections'][9]
    return tuple(tuple(bytes(labels[row[i]:row[i] + row[i + 1]]).decode('utf-8')
                       for i in (0, 2, 4))
                 for row in archive.PROGRAM.iter_unpack(view['sections'][8]))


def bindings(view, programs):
    labels = view['sections'][9]
    rows = []
    for row in archive.BINDING.iter_unpack(view['sections'][11]):
        locale, kind = (bytes(labels[row[i]:row[i] + row[i + 1]]).decode('ascii')
                        for i in (0, 2))
        targets = tuple(None if index == 0xffffffff else programs[index] for index in row[4:])
        rows.append(collation_bindings.Binding(locale, kind, *targets))

    return tuple(rows)


def _decode_checked(view, count):
    """Interpret one validated archive, retaining the bundled record contract."""
    if not count or count != view['program_count'] or not view['counts'][11]:
        raise DataError('invalid bundled collation program/binding count')

    programs = identities(view)
    return {'fingerprint': view['fingerprint'], 'programs': programs,
            'bindings': bindings(view, programs)}


def decode(section):
    """Check nested framing/counts; an absent optional section stays absent."""
    if section is None:
        return None
    count, data = section
    return _decode_checked(archive.unpack(data), count)


def pair(sources, profile, fingerprint, data):
    """Require exact source bindings and the pruned executable target closure.

    fingerprint is the acquired CLDR manifest digest, NOT QJIN's generation
    digest. The outer recipe separately hashes this payload and its source reads.
    """
    view = archive.unpack(data, fingerprint)
    section = view['program_count'], data
    decoded = _decode_checked(view, section[0])
    expected = collation_bindings.collect(sources, profile)
    if (decoded['bindings'] != expected or
            decoded['programs'] != collation_bindings.program_targets(expected)):
        raise DataError('compiled collation does not match the requested source/profile')

    # Rule collection accounts for XML/keyword/inheritance inputs. Also record
    # the compiler's shared-root inputs even though pairing does not rebuild it.
    sources.read(collation_root.SOURCE)
    sources.read(collation_groups.SOURCE)
    sources.read(collation_groups.SCRIPTS)
    return section
