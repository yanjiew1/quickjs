# CLDR timezone generation inputs

The geographic metadata is from released CLDR 48.2, tag `release-48-2`,
commit `11299982335beb974c1c63c45265184e759c0f41`. The adjacent pin binds
its exact XML bytes; Unicode licensing is preserved in `LICENSE`. XML is
a host generation input. Native runtime offsets remain pinned separately
to IANA tz 2026e and consume compiled binary views.

`windowsZones.xml` and `pin.json` additionally bind the Windows key/territory
mapping. `tools/gen-windows-zones.py` validates the pinned XML and IANA names
and emits the compact WTZ1 table compiled into `windows-zone-data.inc`.
Native Windows discovery uses `GetUserGeoID` and `GetGeoInfoW` to select
region-specific rows, falling back to `001` only when necessary.
