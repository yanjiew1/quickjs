"""Bounded transport and temporary indexes for exact original witness traces.

The private database contains only values encoded by this process. It is not a
capture artifact or an approval receipt and is deleted even on rejection.
"""
from collections import OrderedDict
from collections.abc import MutableMapping, MutableSet
from pathlib import Path
import hashlib
import io
import json
import sqlite3
import tempfile


DEFAULT_OUTPUT_LIMIT = 512 * 1024 * 1024
DEFAULT_RECORD_LIMIT = 16 * 1024 * 1024
LUNISOLAR_FIXTURE = ('test262/test/intl402/DateTimeFormat/prototype/'
                     'formatToParts/compare-to-temporal-lunisolar.js')
LUNISOLAR_SHA256 = 'a2aaeb68a67e1f4e95c81a7eec113474f4ee4e730b804e8df09a1387d446489e'
# 101 ISO years, three calendars, at most thirteen months per year. The
# observed largest full month segment is 281,556 bytes / 171 events. Reserve
# 512 KiB per month plus 16 MiB for constructors, start/EOF and year boundaries.
# This is an explicit fail-closed transport budget, not a conformance waiver or
# a proof that future implementations cannot exceed the reserve.
LUNISOLAR_OUTPUT_LIMIT = 101 * 3 * 13 * (512 * 1024) + 16 * 1024 * 1024
LUNISOLAR_RECORD_LIMIT = 64 * 1024
MAX_EVENTS = 2000000
DATABASE_LIMIT = 4 * 1024 * 1024 * 1024
MAX_STACK_DEPTH = 256


class TraceBoundInvalid(ValueError):
    pass


def trace_limits(config):
    if (config.get('fixture') == LUNISOLAR_FIXTURE and
            config.get('fixture_sha256') == LUNISOLAR_SHA256):
        return LUNISOLAR_OUTPUT_LIMIT, LUNISOLAR_RECORD_LIMIT
    return DEFAULT_OUTPUT_LIMIT, DEFAULT_RECORD_LIMIT


def hash_file(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


class EventLines:
    """One line at a time, authenticating the exact bytes including terminators."""
    def __init__(self, source, config, parser=None):
        self.source = source
        self.parser = parser or json.loads
        self.byte_limit, self.record_limit = trace_limits(config)
        budget = config.get('max_events', MAX_EVENTS)
        if type(budget) is not int or not 0 < budget <= MAX_EVENTS:
            raise TraceBoundInvalid('Invalid finite event bound')
        self.event_limit = budget
        self.count = 0
        self.bytes_seen = 0
        self.digest = hashlib.sha256()
        self.exhausted = False

    def __iter__(self):
        if self.count or self.exhausted:
            raise TraceBoundInvalid('Trace is single pass')
        if isinstance(self.source, Path):
            with self.source.open('rb') as stream:
                yield from self._read(stream)
        elif isinstance(self.source, str):
            # Compatibility for existing small pure mocked fixtures. Production
            # passes a Path, so this never copies the complete hosted trace.
            yield from self._consume((line.encode('utf-8')
                                      for line in io.StringIO(self.source)))
        else:
            def encoded():
                for line in self.source:
                    if isinstance(line, str):
                        # Preserve the historical iterable-of-text-lines API.
                        yield (line if line.endswith('\n') else line + '\n').encode('utf-8')
                    elif isinstance(line, bytes):
                        yield line
                    else:
                        raise TraceBoundInvalid('Unknown trace line type')
            yield from self._consume(encoded())
        self.exhausted = True

    def _read(self, stream):
        # readline's size bound prevents a forged huge line from being loaded
        # before the record ceiling can be checked.
        yield from self._consume(iter(lambda: stream.readline(self.record_limit + 1), b''))

    def _consume(self, lines):
        for raw in lines:
            self.count += 1
            self.bytes_seen += len(raw)
            if len(raw) > self.record_limit:
                raise TraceBoundInvalid('Finite record bound exceeded')
            if self.count > self.event_limit:
                raise TraceBoundInvalid('Finite trace bound exceeded')
            if self.bytes_seen > self.byte_limit:
                raise TraceBoundInvalid('Finite stdout byte bound exceeded')
            self.digest.update(raw)
            if not raw.strip():
                raise TraceBoundInvalid('Unknown blank output')
            try:
                # Decode explicitly: unlike json.loads(bytes), the previous
                # text parser did not silently accept a UTF-8 BOM.
                event = self.parser(raw.decode('utf-8'))
            except (ValueError, UnicodeError, TypeError, RecursionError) as exc:
                raise TraceBoundInvalid('Non-event output') from exc
            if not isinstance(event, dict):
                raise TraceBoundInvalid('Malformed event')
            yield event

    def verify(self, expected):
        if not self.exhausted or not self.count or self.digest.hexdigest() != expected:
            raise TraceBoundInvalid('Complete raw stdout binding differs')


class EventSet:
    """A finite event-ID set; 250,001 bytes at the global event ceiling."""
    def __init__(self, limit):
        self.limit = limit
        self.bits = bytearray((limit + 8) // 8)
        self.count = 0

    def __contains__(self, key):
        return (type(key) is int and 0 < key <= self.limit and
                bool(self.bits[key // 8] & (1 << (key % 8))))

    def add(self, key):
        if type(key) is not int or not 0 < key <= self.limit:
            raise TraceBoundInvalid('Invalid event reference')
        if key not in self:
            self.bits[key // 8] |= 1 << (key % 8)
            self.count += 1

    def __len__(self):
        return self.count

    def __iter__(self):
        for key in range(1, self.limit + 1):
            if key in self:
                yield key


def encode(value, undefined):
    """Tagged internal encoding preserves sets, tuples and UNDEFINED identity.

    User dictionary keys cannot introduce tags: every container is tagged by
    the encoder itself. No pickle or executable object deserialization is used.
    """
    if value is undefined:
        return ['undefined']
    if type(value) is dict:
        return ['dict', [[encode(k, undefined), encode(v, undefined)] for k, v in value.items()]]
    if type(value) is list:
        return ['list', [encode(v, undefined) for v in value]]
    if type(value) is tuple:
        return ['tuple', [encode(v, undefined) for v in value]]
    if type(value) is set:
        return ['set', [encode(v, undefined) for v in value]]
    if value is None or type(value) in (bool, int, float, str):
        return ['scalar', value]
    raise TraceBoundInvalid('Unknown private receipt value')


def decode(encoded, undefined):
    tag = encoded[0]
    if tag == 'undefined':
        return undefined
    if tag == 'scalar':
        return encoded[1]
    if tag == 'dict':
        return {decode(k, undefined): decode(v, undefined) for k, v in encoded[1]}
    values = [decode(v, undefined) for v in encoded[1]]
    if tag == 'list':
        return values
    if tag == 'tuple':
        return tuple(values)
    if tag == 'set':
        return set(values)
    raise TraceBoundInvalid('Unknown private receipt tag')


class ReceiptStore:
    """Private disk indexes; SQLite page cache capped at 2 MiB, mmap disabled."""
    def __init__(self, undefined, directory=None, database_limit=DATABASE_LIMIT):
        self.undefined = undefined
        self.directory = directory
        self.database_limit = database_limit

    def __enter__(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='native-trace-index-', dir=self.directory)
        self.connection = None
        try:
            self.connection = sqlite3.connect(str(Path(self.temporary.name) / 'receipts.sqlite'))
            self.connection.execute('PRAGMA page_size=4096')
            self.connection.execute('PRAGMA journal_mode=OFF')
            self.connection.execute('PRAGMA synchronous=OFF')
            self.connection.execute('PRAGMA mmap_size=0')
            self.connection.execute('PRAGMA cache_size=-2048')
            self.connection.execute('PRAGMA temp_store=FILE')
            self.connection.execute('PRAGMA max_page_count=' + str(max(1, self.database_limit // 4096)))
            self.connection.execute('CREATE TABLE receipts (namespace TEXT, key TEXT, value TEXT, PRIMARY KEY(namespace, key)) WITHOUT ROWID')
            return self
        except BaseException:
            self.__exit__(None, None, None)
            raise

    def __exit__(self, *args):
        try:
            if self.connection is not None:
                self.connection.close()
        finally:
            self.temporary.cleanup()

    def key(self, value):
        return json.dumps(encode(value, self.undefined), separators=(',', ':'), ensure_ascii=True, allow_nan=False)

    def mapping(self, name):
        return ReceiptMap(self, name)


class ReceiptMap(MutableMapping):
    """Retain all exact receipts on disk; cache at most eight values per map."""
    def __init__(self, store, name):
        self.store = store
        self.name = name
        self.cache = OrderedDict()

    def __getitem__(self, key):
        encoded_key = self.store.key(key)
        if encoded_key in self.cache:
            self.cache.move_to_end(encoded_key)
            return self.cache[encoded_key]
        row = self.store.connection.execute('SELECT value FROM receipts WHERE namespace=? AND key=?', (self.name, encoded_key)).fetchone()
        if row is None:
            raise KeyError(key)
        value = decode(json.loads(row[0]), self.store.undefined)
        self._cache(encoded_key, value)
        return value

    def _cache(self, key, value):
        self.cache[key] = value
        self.cache.move_to_end(key)
        if len(self.cache) > 8:
            self.cache.popitem(last=False)

    def __setitem__(self, key, value):
        encoded_key = self.store.key(key)
        encoded_value = self.store.key(value)
        self.store.connection.execute('INSERT OR REPLACE INTO receipts VALUES (?, ?, ?)', (self.name, encoded_key, encoded_value))
        self._cache(encoded_key, value)

    def __delitem__(self, key):
        encoded_key = self.store.key(key)
        cursor = self.store.connection.execute('DELETE FROM receipts WHERE namespace=? AND key=?', (self.name, encoded_key))
        if not cursor.rowcount:
            raise KeyError(key)
        self.cache.pop(encoded_key, None)

    def __iter__(self):
        for row in self.store.connection.execute('SELECT key FROM receipts WHERE namespace=?', (self.name,)):
            yield decode(json.loads(row[0]), self.store.undefined)

    def __len__(self):
        return self.store.connection.execute('SELECT count(*) FROM receipts WHERE namespace=?', (self.name,)).fetchone()[0]


class ReceiptSet(MutableSet):
    def __init__(self, store, name):
        self.mapping = store.mapping(name)

    def __contains__(self, key):
        return key in self.mapping

    def __iter__(self):
        return iter(self.mapping)

    def __len__(self):
        return len(self.mapping)

    def add(self, key):
        self.mapping[key] = True

    def discard(self, key):
        if key in self.mapping:
            del self.mapping[key]
