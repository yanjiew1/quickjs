# Native Intl data generation

These host tools verify Unicode and released CLDR inputs and produce one
byte-oriented Intl binary, optionally emitted as a single C definition.
Runtime readers use the binary; XML and JSON are generation inputs only.

`combined_bundle.py --help` describes the required verified input manifest,
output directory and explicit locale policy. `MODULE-PINS.json` authenticates
all collector modules before they are imported. The released bundle uses
CLDR 48.2 and its bundled UCA 17; core Unicode algorithms remain independent.
The runtime initially installs en/en-US, and service capability checks remain
separate from the presence of additional source-data rows.

`RELEASE-SOURCE-ASSEMBLY.json` and the generated data provenance describe
historical source preparation and acquisition. Their recorded paths, source
hashes and `generators_executed` fields are archived evidence, not a claim
that the current generator invocation or runtime configuration was validated.
Some collectors retain those preparation-era descriptions for source-pin
identity. The current entrypoint documentation describes maintained behavior.
Reproduction requires the verified original input manifest and consumed
source files; ordinary repository tests do not download those inputs.

`locale_inventory_carry.py` preserves accepted formatter payloads while
refreshing installed calendar/territory inventory and provenance. It does
not perform fresh formatter collection or execute runtime qualification.
Unicode data attribution is retained in
`src/intl/data/unicode-data-LICENSE.txt` and the consumed input manifest.
