"""Independent, finite TZif offset proof for the original link fixture.

No zoneinfo module, host path reread, QuickJS helper, C offset function,
or engine result supplies an expected offset. Only authenticated blobs do.
Copyright (c) 2026 Yan-Jie Wang. MIT license.
"""
import bisect
import calendar
import datetime
import hashlib
import json
import re
import struct
from pathlib import Path

EPOCH_NS = tuple(str(n * 1_000_000_000) for n in (
    -2208988800, -631152000, -315619200, 0, 315532800,
    631152000, 946684800, 1262304000, 1577836800, 1893456000))
MAX_FILE = 65536
MAX_RECORDS = 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def _block(raw, pos, width, version):
    require(pos + 44 <= len(raw), 'Truncated TZif header')
    h = raw[pos:pos + 44]
    require(h[:5] == b'TZif' + version and not any(h[5:20]),
            'Wrong TZif version or reserved bytes')
    nut, nstd, nleap, ntime, ntype, nchar = struct.unpack('>6I', h[20:])
    require(nleap == 0 and 1 <= ntype <= 256 and nchar > 0 and
            nstd in (0, ntype) and nut in (0, ntype), 'Invalid TZif counts')
    start = pos + 44
    end = start + ntime * (width + 1) + ntype * 6 + nchar + nstd + nut
    require(end <= len(raw), 'Truncated TZif block')
    times = list(struct.unpack('>' + ('q' if width == 8 else 'i') * ntime,
                              raw[start:start + ntime * width]))
    start += ntime * width
    indices = list(raw[start:start + ntime])
    start += ntime
    types = [struct.unpack('>iBB', raw[start + i * 6:start + (i + 1) * 6])
             for i in range(ntype)]
    start += ntype * 6
    chars = raw[start:start + nchar]
    start += nchar
    stds, uts = raw[start:start + nstd], raw[start + nstd:end]
    require(all(a < b for a, b in zip(times, times[1:])) and
            all(i < ntype for i in indices), 'Invalid transitions')
    require(all(c == 0 or 32 <= c <= 126 for c in chars), 'Invalid abbreviation bytes')
    for i, (offset, daylight, abbr) in enumerate(types):
        require(-86400 < offset < 86400 and daylight in (0, 1) and
                abbr < nchar and b'\0' in chars[abbr:], 'Invalid local time type')
        require(not nstd or stds[i] in (0, 1), 'Invalid standard indicator')
        require(not nut or (uts[i] in (0, 1) and
                (not uts[i] or (nstd and stds[i]))), 'Invalid UTC indicator')
    return end, {'times': times, 'indices': indices, 'types': types}


class Footer:
    """Parse POSIX suffixes; finite Gregorian years use Python civil dates."""
    def __init__(self, text, version):
        self.text, self.pos, self.version = text, 0, version

    def consume(self, char):
        if self.text[self.pos:self.pos + 1] != char:
            return False
        self.pos += 1
        return True

    def number(self, low, high):
        m = re.match(r'[0-9]+', self.text[self.pos:])
        require(m is not None, 'Missing footer number')
        self.pos += len(m[0])
        n = int(m[0])
        require(low <= n <= high, 'Footer number out of bounds')
        return n

    def name(self):
        if self.consume('<'):
            m = re.match(r'[A-Za-z0-9+\-]{3,}>', self.text[self.pos:])
            require(m is not None, 'Invalid quoted footer name')
            self.pos += len(m[0])
            return m[0][:-1]
        m = re.match(r'[A-Za-z]{3,}', self.text[self.pos:])
        require(m is not None, 'Invalid footer name')
        self.pos += len(m[0])
        return m[0]

    def clock(self, rule=False):
        signed = self.text[self.pos:self.pos + 1] in ('+', '-')
        sign = -1 if self.consume('-') else 1
        if sign == 1:
            self.consume('+')
        require(not (rule and self.version == b'2' and signed),
                'TZif v2 forbids signed rule times')
        hour = self.number(0, 167 if rule and self.version != b'2' else 24)
        minute = self.number(0, 59) if self.consume(':') else 0
        second = self.number(0, 59) if self.consume(':') else 0
        n = sign * (hour * 3600 + minute * 60 + second)
        require(rule or -86400 < n < 86400, 'Invalid footer offset')
        return n

    def rule(self):
        if self.consume('M'):
            month = self.number(1, 12)
            require(self.consume('.'), 'Missing month rule separator')
            week = self.number(1, 5)
            require(self.consume('.'), 'Missing weekday rule separator')
            day = self.number(0, 6)
            value = ('M', month, week, day)
        elif self.consume('J'):
            value = ('J', self.number(1, 365))
        else:
            value = ('D', self.number(0, 365))
        return value, self.clock(True) if self.consume('/') else 7200


def _rule_epoch(year, rule, before):
    r, clock = rule
    if r[0] == 'M':
        _, month, week, weekday = r
        first = datetime.date(year, month, 1)
        wanted = (weekday + 6) % 7  # POSIX Sunday 0, Python Monday 0.
        day = 1 + (wanted - first.weekday()) % 7 + (week - 1) * 7
        if day > calendar.monthrange(year, month)[1]:
            day -= 7
        date = datetime.date(year, month, day)
    else:
        day = r[1] if r[0] == 'D' else r[1] - 1
        if r[0] == 'J' and calendar.isleap(year) and r[1] >= 60:
            day += 1
        date = datetime.date(year, 1, 1) + datetime.timedelta(days=day)
    return (date - datetime.date(1970, 1, 1)).days * 86400 + clock - before


def parse_tzif(raw):
    require(isinstance(raw, bytes) and 44 <= len(raw) <= MAX_FILE,
            'Invalid TZif byte extent')
    version = raw[4:5]
    require(version in (b'2', b'3', b'4'), 'Unsupported TZif version')
    pos, _ = _block(raw, 0, 4, version)
    pos, result = _block(raw, pos, 8, version)
    tail = raw[pos:]
    require(len(tail) >= 2 and tail[:1] == tail[-1:] == b'\n' and
            b'\n' not in tail[1:-1] and b'\0' not in tail,
            'Invalid POSIX footer envelope')
    text = tail[1:-1].decode('ascii')
    result['footer'] = text
    if not text:
        result['future'] = None
        return result
    f = Footer(text, version)
    f.name()
    standard = -f.clock()
    if f.pos == len(text):
        result['future'] = (standard,)
    else:
        f.name()
        daylight = standard + 3600
        if text[f.pos:f.pos + 1] != ',':
            daylight = -f.clock()
        require(-86400 < daylight < 86400 and f.consume(','),
                'Missing bounded daylight offset or rule')
        start = f.rule()
        require(f.consume(','), 'Missing final footer rule')
        end = f.rule()
        require(f.pos == len(text), 'Trailing footer input')
        periodic = False
        for year in range(2000, 2400):
            a = _rule_epoch(year, start, standard)
            b = _rule_epoch(year, end, daylight)
            if b < a or 0 < b - a < (366 if calendar.isleap(year) else 365) * 86400:
                periodic = True
                break
        result['future'] = (standard, daylight, start, end, periodic)
    if result['times']:
        # The captured decoder is independent; prove footer continuity too.
        last = result['times'][-1]
        first = (datetime.date(402, 1, 1) - datetime.date(1970, 1, 1)).days * 86400
        limit = (datetime.date(9998, 1, 1) - datetime.date(1970, 1, 1)).days * 86400
        if first <= last < limit:
            require(_future_offset(result, last) ==
                    result['types'][result['indices'][-1]][0],
                    'Footer disagrees with final explicit transition')
    return result


def _future_offset(parsed, seconds):
    future = parsed['future']
    if len(future) == 1:
        return future[0]
    standard, daylight, start, end, periodic = future
    if not periodic:
        return daylight
    instant = datetime.datetime(1970, 1, 1) + datetime.timedelta(seconds=seconds)
    require(402 <= instant.year <= 9998, 'Finite footer proof civil range exceeded')
    events = []
    # A complete 400-year Gregorian cycle proves that no rarer enabled
    # season is omitted. We deliberately do not copy the C early exit.
    for year in range(instant.year - 400, instant.year + 2):
        a, b = _rule_epoch(year, start, standard), _rule_epoch(year, end, daylight)
        if b < a or 0 < b - a < (366 if calendar.isleap(year) else 365) * 86400:
            events.extend((t, offset) for t, offset in ((a, daylight), (b, standard))
                          if t <= seconds)
    return max(events, key=lambda item: item[0])[1] if events else standard


def offset_ns(parsed, epoch_ns):
    require(epoch_ns in EPOCH_NS, 'Timestamp outside original finite link fixture')
    seconds = int(epoch_ns) // 1_000_000_000
    times = parsed['times']
    if parsed['future'] is not None and (not times or seconds > times[-1]):
        offset = _future_offset(parsed, seconds)
    else:
        index = bisect.bisect_right(times, seconds) - 1
        type_id = parsed['indices'][index] if index >= 0 else 0
        offset = parsed['types'][type_id][0]
    return offset * 1_000_000_000


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate capture JSON key')
        result[key] = value
    return result


def read_capture(directory, command, primary_labels, system_root):
    """Command comes from the root-owned closed process receipt.

    Caller must separately bind its argv, effective environment, binary,
    compiler flags, source map and generated fixture to the exact attempt.
    """
    directory = Path(directory)
    require(command.get('lifecycle_closed') is True and command.get('returncode') == 0
            and command.get('timed_out') is False and command.get('interrupted') is False
            and command.get('output_limited') is False, 'Capture command did not close cleanly')
    manifest = directory / 'selected-timezones.ndjson'
    require(manifest.is_file() and not manifest.is_symlink(), 'Missing regular capture manifest')
    raw_manifest = manifest.read_bytes()
    require(raw_manifest.endswith(b'\n') and len(raw_manifest) <= 2 * 1024 * 1024,
            'Truncated or oversized capture manifest')
    rows = [json.loads(line, object_pairs_hook=_unique_object)
            for line in raw_manifest.splitlines()]
    require(0 < len(rows) <= MAX_RECORDS, 'Invalid snapshot count')
    selected, roles = {}, []
    allowed_files = {'selected-timezones.ndjson'}
    fields = {'schema', 'pid', 'sequence', 'identifier', 'primary', 'origin',
              'source_status', 'system_root', 'blob', 'size'}
    for sequence, row in enumerate(rows, 1):
        require(set(row) == fields and row['schema'] == 'qjs-selected-tzif-record-v1'
                and type(row['pid']) is int and row['pid'] == command.get('pid')
                and type(row['sequence']) is int and row['sequence'] == sequence,
                'Wrong record type, process or sequence')
        primary = row['primary']
        require(primary in primary_labels and row['identifier'] in primary_labels
                and primary_labels[row['identifier']] == primary
                and row['system_root'] == system_root
                and row['origin'] in ('system', 'embedded')
                and type(row['source_status']) is int and 0 <= row['source_status'] <= 4,
                'Wrong source, primary or origin')
        require((row['origin'] == 'system') == (row['source_status'] == 0),
                'Selected origin and source status disagree')
        name = f'snapshot-{sequence:04d}.tzif'
        require(row['blob'] == name and type(row['size']) is int
                and 44 <= row['size'] <= MAX_FILE, 'Invalid blob path or extent')
        blob = directory / name
        require(blob.is_file() and not blob.is_symlink(), 'Missing regular blob')
        raw = blob.read_bytes()
        require(len(raw) == row['size'], 'Blob extent differs')
        parsed = parse_tzif(raw)
        expected = {epoch: offset_ns(parsed, epoch) for epoch in EPOCH_NS}
        record = {'primary': primary, 'origin': row['origin'], 'source_status': row['source_status'], 'tzif_sha256': sha(raw),
                  'size': len(raw), 'system_root': system_root,
                  'offsets': expected, 'footer': parsed['footer']}
        require(primary not in selected or selected[primary] == record,
                'Selected bytes changed inside one process')
        selected[primary] = record
        roles.append({'role': 'actual-selected-provider-tzif-v1', 'path': name,
                      'sha256': sha(raw), 'size': len(raw), 'primary': primary,
                      'origin': row['origin'], 'pid': row['pid'], 'sequence': sequence})
        allowed_files.add(name)
    require({p.name for p in directory.iterdir()} == allowed_files,
            'Unaccounted capture artifact member')
    return {'schema': 'qjs-selected-tzif-finite-proof-v1', 'manifest_sha256': sha(raw_manifest),
            'command_sha256': sha(json.dumps(command, sort_keys=True, separators=(',', ':')).encode()),
            'selected_primaries': selected, 'artifact_roles': roles,
            'epoch_nanoseconds': list(EPOCH_NS), 'conformance_accepted': False}


def compare_actual(probe, actual, required_primaries, embedded_records):
    """No retrospective expected value is copied from an engine result."""
    require(probe['schema'] == actual['schema'] == 'qjs-selected-tzif-finite-proof-v1',
            'Wrong finite proof role')
    p, a = probe['selected_primaries'], actual['selected_primaries']
    require(set(required_primaries) <= set(p) and set(required_primaries) <= set(a),
            'Missing original fixture provider snapshot')
    for primary in required_primaries:
        require(p[primary] == a[primary], 'Provider changed after preflight: ' + primary)
        if a[primary]['origin'] == 'embedded':
            require(a[primary]['tzif_sha256'] == embedded_records[primary]['tzif_sha256']
                    and a[primary]['offsets'] == embedded_records[primary]['offsets'],
                    'Fallback does not match immutable embedded source')
    return True
