"""Root executes these isolated proof models; no runtime acceptance is granted."""
import unittest
from premise_trace_proof import validate_branch, PremiseProofInvalid, UNDEFINED
def typed(value):
    if value is UNDEFINED:
        return {'kind':'undefined'}
    if value is None:
        return {"kind": "null"}
    if type(value) is bool:
        return {"kind": "boolean", "value": value}
    if type(value) is int:
        return {"kind": "number", "value": str(value)}
    if isinstance(value, str):
        raw = value.encode("utf-16-le", errors="surrogatepass")
        return {"kind": "string", "value": value,
                "utf16": [int.from_bytes(raw[i:i + 2], "little")
                          for i in range(0, len(raw), 2)],
                "codepoints": [ord(ch) for ch in value]}
    array = isinstance(value, list)
    pairs = [(str(i), x) for i, x in enumerate(value)] + [("length", len(value))] if array else list(value.items())
    return {"kind": "array" if array else "object", "properties": [
        dict(key=typed(k), value=typed(v), writable=True,
             enumerable=not (array and k == "length"),
             configurable=not (array and k == "length")) for k, v in pairs]}

MONTH_NAMES = {'common': {'M01': 'Tishri', 'M02': 'Heshvan', 'M03': 'Kislev', 'M04': 'Tevet', 'M05': 'Shevat', 'M06': 'Adar', 'M07': 'Nisan', 'M08': 'Iyar', 'M09': 'Sivan', 'M10': 'Tamuz', 'M11': 'Av', 'M12': 'Elul'}, 'leap': {'M01': 'Tishri', 'M02': 'Heshvan', 'M03': 'Kislev', 'M04': 'Tevet', 'M05': 'Shevat', 'M05L': 'Adar I', 'M06': 'Adar II', 'M07': 'Nisan', 'M08': 'Iyar', 'M09': 'Sivan', 'M10': 'Tamuz', 'M11': 'Av', 'M12': 'Elul'}}

BASE_NAMES={'premise-original-formatter-origin','premise-selected-English-latn','parts-array','number-date-parts-nonempty',
 'part-object-prototype','part-data-property-type','part-data-property-value','part-type-domain',
 'premise-temporal-calendar-binding','premise-original-lunisolar-parts-origin',
 'premise-lunisolar-parts-format-join','premise-numeric-field-day'}
HEBREW_NAMES={'premise-hebrew-numeric-year-part','premise-numeric-field-year',
 'premise-hebrew-month-code-and-leap-domain','premise-hebrew-independent-named-month'}
LUNISOLAR_NAMES={'premise-original-lunisolar-month-code-domain','premise-original-lunisolar-month-and-leap-marker'}

def model(calendar='hebrew',code='M05L',leap=True):
    name=MONTH_NAMES['leap' if leap else 'common'].get(code,'unreviewed') if calendar=='hebrew' else str(int(code[1:3]))+('bis' if code.endswith('L') else '')
    p=[dict(type='month',value=name),dict(type='literal',value='/'),dict(type='day',value='30'),
       dict(type='literal',value='/'),dict(type='year',value='5809')]
    r=dict(locale='en',calendar=calendar,numberingSystem='latn',timeZone='UTC',
           year='numeric',month='short' if calendar=='hebrew' else 'numeric',day='numeric')
    proof=dict(schema='native-premise-branch-proof-v2',guard_site='guard',formatter_event=1,parts_event=2,
               original_result_event=None,auxiliary_events=[dict(event=3,role='lunisolar-format-join')],
               original_arguments=[0],join_text=''.join(x['value'] for x in p))
    evidence=dict(calendar=calendar,calendar_year=5809,day=30,monthCode=code,
                  hebrew_in_leap_year=leap if calendar=='hebrew' else UNDEFINED,
                  actual_parts=p,selected=r,numeric_year_part=p[-1],unavailable_read_event=UNDEFINED,
                  selected_U_without_r=False,proof=proof)
    c=[dict(site='guard',sequence=10,permit_snapshot_continuation=False),
       dict(premise_guard='lunisolar-selected-U',premise_parameters={}),
       BASE_NAMES | (HEBREW_NAMES if calendar=='hebrew' else LUNISOLAR_NAMES),
       dict(fixture='test/intl402/DateTimeFormat/prototype/formatToParts/compare-to-temporal-lunisolar.js',premise_data={'hebrewMonthNames':MONTH_NAMES}),
       {},{},[],evidence]
    return c

def check(c):
    e=c[7];p=e['actual_parts'];proof=e['proof'];text=''.join(x['value'] for x in p)
    proof['join_text']=text
    c[4].update({1:dict(site='ctor',source='new Intl.DateTimeFormat(locale, options)',
                     arguments=[typed('en'),typed({})],receiver=None,value=typed({}),resolved=typed(e['selected'])),
                 2:dict(site='parts',source='formatter.formatToParts(epochMilliseconds)',arguments=[typed(0)],
                     receiver=dict(kind='call-reference',call_event=1),value=typed(p))})
    c[5][3]=dict(site='guard',auxiliary_role='lunisolar-format-join',value=typed(text))
    c[0]['evidence']=typed(e)
    return validate_branch(*c[:7])

class H2LunisolarProofTests(unittest.TestCase):
    def setUp(self):
        # Ensure common fixture premises pass before testing any corruption.
        # An omitted required receipt must fail every method, not turn all
        # rejection tests into apparent successes.
        check(model())
        check(model('chinese'))
        check(model('dangi'))
    def reject(self,c, missing_receipt=False):
        with self.assertRaises(PremiseProofInvalid) as failure:
            check(c)
        if not missing_receipt:
            self.assertNotIn('Missing branch-specific invariant',str(failure.exception))
    def test_all_independent_hebrew_month_names(self):
        for leap in (False,True):
            for code in MONTH_NAMES['leap' if leap else 'common']:
                with self.subTest(leap=leap,code=code):check(model(code=code,leap=leap))
    def test_chinese_and_dangi_still_numeric(self):
        for calendar in ('chinese','dangi'):
            for code in ('M01','M05L','M12'):check(model(calendar,code))
    def test_hebrew_numeric_or_arbitrary_resolved_month_rejected(self):
        for value in ('numeric','2-digit','long','narrow'):
            c=model();c[7]['selected']['month']=value;self.reject(c)
    def test_chinese_and_dangi_named_resolved_month_rejected(self):
        for calendar in ('chinese','dangi'):
            c=model(calendar);c[7]['selected']['month']='short';self.reject(c)
    def test_year_day_calendar_zone_numbering_bindings(self):
        for key,value in [('year','2-digit'),('day','2-digit'),('calendar','gregory'),('timeZone','Etc/GMT'),('numberingSystem','arab')]:
            c=model();c[7]['selected'][key]=value;self.reject(c)
    def test_ordinal_unknown_or_swapped_adar_rejected(self):
        for value in ('6','unreviewed','Adar II','Adar'):
            c=model();c[7]['actual_parts'][0]['value']=value;self.reject(c)
    def test_hebrew_year_and_day_values_not_waived(self):
        for index in (2,4):
            c=model();c[7]['actual_parts'][index]['value']='99';self.reject(c)
    def test_hebrew_missing_or_related_or_duplicate_year_rejected(self):
        c=model();c[7]['numeric_year_part']=UNDEFINED;c[7]['actual_parts'].pop();c[7]['selected_U_without_r']=True;c[0]['permit_snapshot_continuation']=True;self.reject(c)
        c=model();c[7]['actual_parts'][-1]['type']='relatedYear';self.reject(c)
        c=model();c[7]['actual_parts'].append(dict(type='year',value='5809'));self.reject(c)
    def test_duplicate_month_rejected(self):
        c=model();c[7]['actual_parts'].append(dict(type='month',value='Adar I'));self.reject(c)
    def test_wrong_leap_or_monthcode_domain_rejected(self):
        for code,leap in [('M05L',False),('M00',True),('M13',True),('M06L',True)]:self.reject(model(code=code,leap=leap))
        for value in (None,0,'true'):
            c=model();c[7]['hebrew_in_leap_year']=value;self.reject(c)
    def test_missing_independent_bank_rejected(self):
        c=model();c[3]['premise_data']={};self.reject(c)
    def test_hebrew_invariant_receipts_are_required(self):
        for name in HEBREW_NAMES:
            c=model();c[2]=c[2]-{name};self.reject(c,missing_receipt=True)
    def test_hebrew_evidence_missing_or_added_field_rejected(self):
        c=model();del c[7]['hebrew_in_leap_year'];self.reject(c)
        c=model();c[7]['extra']='not allowed';self.reject(c)
    def test_chinese_dangi_leap_marker_still_required(self):
        for calendar in ('chinese','dangi'):
            c=model(calendar);c[7]['actual_parts'][0]['value']='5';self.reject(c)
    def test_chinese_dangi_cannot_claim_hebrew_leap_evidence(self):
        for calendar in ('chinese','dangi'):
            c=model(calendar);c[7]['hebrew_in_leap_year']=True;self.reject(c)

if __name__=='__main__':unittest.main()
