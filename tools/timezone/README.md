# Native timezone source inputs

`pin.json` selects released **IANA tzdata 2026e** and **CLDR JSON 48.2.3**.
The IANA Git tag resolves to `039ef27cc5f062a2055cb67435d6d71adbefd27d`.
CLDR is pinned to `91c267402229a59e3ef2774544f001bf959e8809`.
The CLDR JSON patch release includes timezone changes for tzdata 2026e;
the older CLDR XML 48.2 release does not include all those changes.

## Acquisition

Choose a cache on a filesystem with sufficient space:

```sh
python3 tools/timezone/acquire.py --cache /path/to/timezone-cache
python3 tools/timezone/acquire.py --cache /path/to/timezone-cache --offline
```

Acquisition uses HTTPS, checks the archive and every consumed source against
SHA-256 pins, limits download size, and uses finite request timeouts and
retries. It replaces verified files atomically. Archive extraction accepts
only the named regular files, validates the complete source set before writing,
and never follows archive paths or symlinks. Existing verified inputs need no
network. Updating a release requires an explicit pin review.

The cache contains `tz-2026e/` and `cldr/`. Raw CLDR timezone metadata is also
versioned under `data/timezone/sources/cldr`, with the Unicode license.
`timezone.json` supplies geographic alias groups; `windowsZones.json` supplies
Windows-key/territory mappings. These are generator inputs, never runtime
JSON dependencies. The native backend's final data is a single binary pack.

Normal builds do not download data, read installed timezone files, or run
this acquisition script. Pinned zic sources are available only for build-host
data generation; target runtime code does not link tzcode.

Sources reviewed on 2026-10-19:

- [IANA release archive](https://www.iana.org/time-zones/releases/2026e)
- [CLDR 48.2.3 release notes](https://cldr.unicode.org/downloads/cldr-48)
- [Released CLDR JSON sources](https://github.com/unicode-org/cldr-json/tree/48.2.3)

Run the fast offline acquisition contracts with:

```sh
python3 tests/test_timezone_generator.py
```
