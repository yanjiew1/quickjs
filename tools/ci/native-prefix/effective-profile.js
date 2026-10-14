/* Runtime profile capture; exact output must be reviewed for the revised source. */
const services = {
  DateTimeFormat: new Intl.DateTimeFormat(),
  NumberFormat: new Intl.NumberFormat(),
  Collator: new Intl.Collator(),
  PluralRules: new Intl.PluralRules(),
  RelativeTimeFormat: new Intl.RelativeTimeFormat(),
  ListFormat: new Intl.ListFormat(),
  DisplayNames: new Intl.DisplayNames(undefined, {type: 'language'}),
  Segmenter: new Intl.Segmenter(),
  DurationFormat: new Intl.DurationFormat()
};
const defaults = {};
for (const name of Object.keys(services)) defaults[name] = services[name].resolvedOptions();
print(JSON.stringify({schema:'intl-effective-profile-v2', defaults,
  requestedJapaneseHour12: new Intl.DateTimeFormat('ja', {hour:'numeric', hour12:true}).resolvedOptions(),
  explicitHanidec: new Intl.DateTimeFormat('en-US-u-nu-hanidec', {
    hour:'2-digit', minute:'2-digit', second:'2-digit', timeZone:'UTC'
  }).resolvedOptions(),
  supportedNumberingSystems: Intl.supportedValuesOf('numberingSystem')}));
