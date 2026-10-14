"""Independent offline branch proof checks. Root owns execution."""
import re


class PremiseProofInvalid(ValueError):
    pass


UNDEFINED = object()


def require(ok, message):
    if not ok:
        raise PremiseProofInvalid(message)


def decode(value):
    kind = value.get('kind')
    if kind == 'undefined':
        return UNDEFINED
    if kind == 'null':
        return None
    if kind in ('string', 'boolean'):
        return value['value']
    if kind in ('number', 'bigint'):
        text = value['value']
        require(re.fullmatch(r'-?[0-9]+', text) is not None, 'Nonintegral proof number')
        return int(text)
    require(kind in ('object', 'array'), 'Proof must contain complete data')
    out = {}
    for prop in value['properties']:
        key = decode(prop['key'])
        require(isinstance(key, str) and key not in out, 'Duplicate proof property')
        require(prop['enumerable'] is True or kind == 'array' and key == 'length', 'Hidden proof property')
        out[key] = decode(prop['value'])
    if kind == 'array':
        length = out.pop('length', None)
        require(type(length) is int and set(out) == {str(i) for i in range(length)}, 'Sparse or extended proof array')
        return [out[str(i)] for i in range(length)]
    return out


def selected(r, calendar=None):
    require(isinstance(r, dict) and re.fullmatch(r'en(?:-US)?(?:-u-ca-(?:chinese|dangi))?', r.get('locale', '')) is not None and r.get('numberingSystem') == 'latn', 'Wrong selected locale/numbering')
    if calendar is not None:
        require(r.get('calendar') == calendar and r.get('timeZone') == 'UTC', 'Wrong calendar/timezone')


def parts(p, source=UNDEFINED):
    require(isinstance(p, list) and p, 'Missing original parts')
    for row in p:
        require(isinstance(row, dict) and isinstance(row.get('type'), str) and isinstance(row.get('value'), str), 'Wrong original part primitive')
        if source is not UNDEFINED:
            require(row.get('source') in ('startRange', 'endRange', 'shared'), 'Wrong range source')


def numeric(p, field, expected, source=UNDEFINED):
    rows = [row for row in p if row['type'] == field and (source is UNDEFINED or row.get('source') == source)]
    require(len(rows) == 1 and re.fullmatch(r'-?[0-9]+', rows[0]['value']) is not None and int(rows[0]['value']) == expected, 'Wrong numeric ' + field)


def named(p, year, source, data):
    require(type(year) is int, 'Missing calendar year')
    rows = [row for row in p if row['type'] == 'yearName' and (source is UNDEFINED or row.get('source') == source)]
    require(len(rows) == 1 and rows[0]['value'] == data['yearNames'][(year-4) % 60], 'Wrong cycle name')


def part_snapshot(value, list_parts=False):
    require(value.get('kind')=='array', 'Untyped parts array receipt')
    allowed={'literal','weekday','era','year','relatedYear','yearName','month','day','dayPeriod','hour','minute','second','fractionalSecond','timeZoneName','unknown'}
    if list_parts:
        allowed={'element','literal'}
    for item in value['properties']:
        if item['key'].get('value')=='length':continue
        require(item['enumerable'] is True and item['configurable'] is True and item['writable'] is True and item['value'].get('kind')=='object','Wrong parts element descriptor')
        fields={prop['key'].get('value'):prop for prop in item['value']['properties']}
        require({'type','value'} <= fields.keys(), 'Missing original part fields')
        if list_parts: require(list(fields)==['type','value'], 'Wrong exact list part keys/order')
        for key in {'type','value','source'} & fields.keys():
            prop=fields[key]; require(prop['enumerable'] is True and prop['configurable'] is True and prop['writable'] is True and prop['value'].get('kind')=='string','Wrong original part field descriptor/primitive')
        require(fields['type']['value']['value'] in allowed,'Wrong original part type domain')
        if 'source' in fields:require(fields['source']['value']['value'] in ('startRange','endRange','shared'),'Wrong original part source domain')


def original(event_id, receipts, method=None, list_parts=False):
    require(type(event_id) is int and event_id in receipts, 'Missing completed original native receipt')
    r = receipts[event_id]
    if re.search(r'\.format(?:Range)?ToParts\s*\(',r['source']):part_snapshot(r['value'],list_parts=list_parts)
    if method:
        require(re.search(method, r['source']) is not None, 'Wrong original native method')
    return r


def receiver(r, event_id):
    require(r.get('receiver') == {'kind':'call-reference', 'call_event':event_id}, 'Wrong original receiver origin')


def result_equal(r, value):
    require(decode(r['value']) == value, 'Original result differs from guard evidence')


def selected_origin(proof, evidence, receipts):
    r = original(proof['formatter_event'], receipts, r'new\s+Intl\.DateTimeFormat')
    require(r.get('resolved') is not None and decode(r['resolved']) == evidence['selected'], 'Selected fields lack original constructor binding')
    return r


def validate_branch(event, site, names, config, receipts, auxiliary_receipts, stack):
    e = decode(event['evidence'])
    require(isinstance(e, dict) and isinstance(e.get('proof'), dict), 'Branch proof missing')
    proof = e['proof']
    require(proof.get('schema') == 'native-premise-branch-proof-v2' and proof.get('guard_site') == event['site'], 'Wrong branch proof identity')
    guard = site['premise_guard']
    schemas={
        'dayperiod-selected-hB':{'base','original','proof'},
        'relatedyear-selected-U':{'calendar','expectedType','expectedValue','part','actual_parts','selected','source','selected_U_without_r','proof'},
        'lunisolar-selected-U':{'calendar','calendar_year','monthCode','day','hebrew_in_leap_year','selected','actual_parts','numeric_year_part','unavailable_read_event','selected_U_without_r','proof'},
        'temporal-numeric-hour':{'result','option','original_options','original_call_event','selected','expected_numeric_hour','actual_numeric_hour','midnight_input','proof'},
        'arEG-fallback-latn':{'selected','result','actual_parts','iso_year','month','day','proof'},
        'collator-default-only':{'locale','collation','supported','resolved','original_resolved_event','selected_co','proof'},
        'locale-default-only':{'tag','supported','collations','selected_co','proof'},
        'tz-primary-identity':{'link','zone','link_primary','zone_primary','epochNs','operation','actual','proof'},
        'unmatched-locale-hc-default':{'original_options','requested','requested_cycle','selected','resolved','supported','compared','proof'},
        'unsupported-es-selected-unit-list':{'selected','original_options','actual_parts','expected_parts','elements','index','supported','joined','proof'}}
    require(guard in schemas and set(e)==schemas[guard] | ({'numeric'} if guard=='dayperiod-selected-hB' and 'numeric' in e else set()),'Wrong exact branch evidence fields')
    require(set(proof)=={'schema','guard_site','formatter_event','parts_event','original_result_event','auxiliary_events','original_arguments','join_text'},'Wrong exact branch proof fields')
    data = config['premise_data']
    linked = proof.get('auxiliary_events')
    require(isinstance(linked,list), 'Missing auxiliary receipt references')
    aux = {}
    for link in linked:
        i=link.get('event'); require(type(i) is int and event.get('_previous_guard_sequence',0) < i < event['sequence'] and i in auxiliary_receipts and i not in aux, 'Unbound/duplicate auxiliary event')
        row=auxiliary_receipts[i];
        if link['role'] in ('dayperiod-parts','extreme-plain-date-parts'):part_snapshot(row['value'])
        require(row['site']==event['site'] and row['auxiliary_role']==link['role'], 'Wrong auxiliary site/role')
        aux[i]=dict(role=link['role'], value=decode(row['value']))
    def auxiliary_values(role):
        return [v['value'] for v in aux.values() if v['role']==role]
    def auxiliary_equal(role,value):
        require(value in auxiliary_values(role), 'Missing authentic auxiliary ' + role)
    required = set()
    permit = event['permit_snapshot_continuation']
    if guard == 'dayperiod-selected-hB':
        required = {'premise-original-formatter-origin','premise-selected-English-latn','premise-real-h-B-selected-fields','premise-original-dayperiod-options','premise-original-24-hour-inputs','premise-original-hour-order','parts-array','part-object-prototype','part-data-property-type','part-data-property-value','part-type-domain','premise-selected-h-B-part-order','premise-numeric-field-hour','premise-source-dayperiod-boundary-name','premise-format-parts-join','premise-dayperiod-original-native-results-present','premise-dayperiod-original-input-and-formatter-binding','premise-dayperiod-original-observed-values-binding'}
        width = site['premise_parameters']['width']
        require(len(auxiliary_values('dayperiod-parts')) == 24 * (2 if 'numeric' in e else 1), 'Missing boundary auxiliary part receipts')
        for base in [e['base']] + ([e['numeric']] if 'numeric' in e else []):
            r = base['selected']; selected(r, 'gregory')
            require(r.get('dayPeriod') == width and r.get('hour') == 'numeric' and r.get('hourCycle') == 'h12' and r.get('hour12') is True and r.get('minute', UNDEFINED) is UNDEFINED and r.get('second', UNDEFINED) is UNDEFINED and base['source_pattern'] == 'h B' and base['requested_dayPeriod'] == width, 'Wrong h B selection')
            require(len(base['points']) == 24, 'Incomplete period boundaries')
            for h, point in enumerate(base['points']):
                rule = 'midnight' if h == 0 else 'noon' if h == 12 else 'morning1' if h < 12 else 'afternoon1' if h < 18 else 'evening1' if h < 21 else 'night1'
                label = data['dayPeriodLabels'][width][rule]
                auxiliary_equal('dayperiod-format',str(h % 12 or 12)+' '+label)
                auxiliary_equal('dayperiod-parts',[{'type':'hour','value':str(h % 12 or 12)},{'type':'literal','value':' '},{'type':'dayPeriod','value':label}])
                require(point == dict(hour=h, rule=rule, dayPeriod=label, numericHour=str(h % 12 or 12), text=str(h % 12 or 12)+' '+label), 'Wrong period/numeric/join source proof')
        orig = e['original']; rows = orig['rows']; require(bool(rows), 'Empty original period proof')
        base_event = proof['formatter_event']; base_rows = []
        for row in rows:
            require(type(row['hour']) is int and 0 <= row['hour'] < 24, 'Wrong original hour')
            h = row['hour']; formatter = original(row['formatter_event'],receipts,r'new\s+Intl\.DateTimeFormat')
            options=decode(formatter['arguments'][1]); require(len(formatter['arguments'])==2 and decode(formatter['arguments'][0])=='en' and set(options) <= {'dayPeriod','hour'} and options.get('dayPeriod')==width and options.get('hour','numeric')=='numeric','Wrong original dayPeriod request'); require(decode(formatter['resolved']) == (e['base']['selected'] if row['formatter_event'] == base_event else e['numeric']['selected']), 'Wrong original period formatter')
            receipt = original(row['event'], receipts, r'\.format(?:ToParts)?\s*\('); receiver(receipt,row['formatter_event']); result_equal(receipt,row['result'])
            args = receipt['arguments']; require(len(args) == 1 and args[0].get('kind') == 'call-reference', 'Wrong original day input receipt')
            date = original(args[0]['call_event'],receipts,r'new\s+Date'); require(len(date['arguments']) == 7 and decode(date['arguments'][3]) == h and all(decode(x) == 0 for x in date['arguments'][4:]), 'Wrong original local hour/date binding')
            rule = 'midnight' if h == 0 else 'noon' if h == 12 else 'morning1' if h < 12 else 'afternoon1' if h < 18 else 'evening1' if h < 21 else 'night1'; label=data['dayPeriodLabels'][width][rule]; text=str(h % 12 or 12)+' '+label
            require(row['text'] == text, 'Wrong original join text')
            if row['mode'] == 'parts':
                parts(row['result']); require(row['result'] == [{'type':'hour','value':str(h % 12 or 12)},{'type':'literal','value':' '},{'type':'dayPeriod','value':label}], 'Wrong original numeric/name/order/parts')
                required |= {'premise-dayperiod-original-part-order','premise-dayperiod-original-name','premise-dayperiod-original-join'}
            else:
                require(row['mode'] == 'format' and row['result'] == text, 'Wrong original format result'); required.add('premise-dayperiod-original-string-numeric-name-order')
            if row['formatter_event'] == base_event: base_rows.append(row)
        # No later original call may be hidden behind an earlier guard/cached proof.
        covered={row['event'] for row in rows}; formatter_events={row['formatter_event'] for row in rows}
        require(all(r.get('receiver') is None or isinstance(r['receiver'], dict) for r in receipts.values()), 'Malformed original receiver receipt')
        require(covered == {i for i,r in receipts.items() if r.get('receiver') is not None and r['receiver'].get('call_event') in formatter_events and re.search(r'\.format(?:ToParts)?\s*\(',r['source'])}, 'Stale or omitted original period output')
        observed=orig['observed']; helper=orig['helper_parts_event']
        require([r['hour'] for r in base_rows] == list(range(len(base_rows))), 'Original hour order changed')
        require(len(observed) == (len(base_rows)-1 if helper is not None and rows[-1]['formatter_event'] == base_event else 24), 'Missing original observed values')
        require(observed == [(r['result'][0]['value'] if r['mode']=='parts' else r['result']) for r in base_rows[:len(observed)]], 'Wrong original observed array')
        if helper is None: required.add('premise-dayperiod-original-full-observed-inputs'); require(len(base_rows)==24,'Missing full original period series')
        else: required.add('premise-dayperiod-original-helper-parts-binding'); require(helper in covered,'Unbound helper parts')
        require(permit is True, 'Wrong period permit')
    elif guard in ('relatedyear-selected-U','lunisolar-selected-U'):
        selected_origin(proof,e,receipts); p=e['actual_parts']; source=e.get('source',UNDEFINED); parts(p,source); r=e['selected']; calendar=e['calendar']; selected(r,calendar)
        require(calendar in (('chinese','dangi') if guard=='relatedyear-selected-U' else ('chinese','dangi','hebrew')) and
                all(r.get(k)=='numeric' for k in ('year','day')) and
                r.get('month') == ('short' if guard=='lunisolar-selected-U' and calendar=='hebrew' else 'numeric'), 'Wrong lunisolar fields')
        pr=original(proof['parts_event'],receipts,r'\.format(?:Range)?ToParts\s*\('); receiver(pr,proof['formatter_event']); result_equal(pr,p)
        require(proof['original_arguments'] == [({'original_event':v['call_event']} if v.get('kind')=='call-reference' else decode(v)) for v in pr['arguments']], 'Wrong original parts input references')
        auxiliary_equal('calendar-format-join' if guard=='relatedyear-selected-U' else 'lunisolar-format-join', proof['join_text'])
        require(proof['join_text']==''.join(row['value'] for row in p), 'Wrong calendar join')
        required={'premise-original-formatter-origin','premise-selected-English-latn','parts-array','part-object-prototype','part-data-property-type','part-data-property-value','part-type-domain'}
        absent=e['selected_U_without_r']; require(type(absent) is bool and permit == absent, 'Wrong absent-year permit')
        if guard=='relatedyear-selected-U':
            required |= {'premise-selected-lunisolar-calendar','premise-original-parts-origin','premise-calendar-parts-format-join'}
            require(len(pr['arguments']) == (1 if source is UNDEFINED else 2),'Wrong original date argument count')
            if source is not UNDEFINED: required |= {'part-data-property-source','part-source-domain'}
            if stack:
                helper=next((x for x in reversed(stack) if x['operation']=='helper'),None)
                if helper and helper.get('arguments'):
                    helper_source=config['sites'][helper['site']]['source']; index=0 if 'function findAll(' in helper_source else 1; original_expected=decode(helper['arguments'][index]); require(isinstance(original_expected,dict) and original_expected.get(e['expectedType'])==e['expectedValue'],'Expected component differs from original helper input')
            expected=e['expectedType']; require(expected in ('relatedYear','month','day'),'Wrong expected related field')
            require(absent == (expected=='relatedYear' and e['part'] is UNDEFINED),'Wrong related absent component')
            if absent:
                require(not any(row['type']=='relatedYear' and (source is UNDEFINED or row['source']==source) for row in p),'Present related year waived'); named(p,e['expectedValue'],source,data); required |= {'premise-no-selected-r-token','premise-related-year-integer','premise-selected-U-correct-calendar-name'}
            if e['part'] is not UNDEFINED: require(e['part'] in p and e['part']['type']==expected and (source is UNDEFINED or e['part'].get('source')==source),'Wrong original selected part')
            if expected in ('month','day'): numeric(p,expected,e['expectedValue'],source); required.add('premise-numeric-field-'+expected)
        else:
            required |= {'premise-temporal-calendar-binding','premise-original-lunisolar-parts-origin','premise-lunisolar-parts-format-join','premise-numeric-field-day','premise-original-lunisolar-month-code-domain','premise-original-lunisolar-month-and-leap-marker'}
            require(len(pr['arguments'])==1,'Wrong original epoch arguments'); numeric(p,'day',e['day'])
            if calendar in ('chinese','dangi'):
                match=re.fullmatch(r'M([0-9]{2})(L?)',e['monthCode']); require(match is not None,'Wrong month code'); m=[row for row in p if row['type']=='month']; require(len(m)==1 and re.fullmatch(r'[0-9]+[^0-9]+' if match[2] else r'[0-9]+',m[0]['value']) is not None and int(re.match(r'[0-9]+',m[0]['value'])[0])==int(match[1]),'Wrong lunisolar month/leap')
            else:
                required -= {'premise-original-lunisolar-month-code-domain','premise-original-lunisolar-month-and-leap-marker'}
                require(absent is False,'Hebrew missing year cannot be waived')
                required |= {'premise-hebrew-numeric-year-part','premise-numeric-field-year',
                             'premise-hebrew-month-code-and-leap-domain','premise-hebrew-independent-named-month'}
                require(isinstance(e['numeric_year_part'],dict) and e['numeric_year_part'].get('type')=='year',
                        'Hebrew year must be the original numeric year part')
                numeric(p,'year',e['calendar_year'])
                leap=e['hebrew_in_leap_year']; code=e['monthCode']
                require(type(leap) is bool and isinstance(code,str) and
                        re.fullmatch(r'M(?:0[1-9]|1[0-2]|05L)',code) is not None and
                        (code!='M05L' or leap), 'Wrong Hebrew month code/leap domain')
                bank=data.get('hebrewMonthNames',{}).get('leap' if leap else 'common',{})
                expected=bank.get(code); m=[row for row in p if row['type']=='month']
                require(isinstance(expected,str) and len(m)==1 and m[0]['value']==expected,
                        'Wrong independently pinned Hebrew month name')
            if calendar in ('chinese','dangi'):
                require(e['hebrew_in_leap_year'] is UNDEFINED,'Unexpected Hebrew leap evidence')
            require(absent == (not any(row['type'] in ('year','relatedYear') for row in p)), 'Wrong lunisolar absence')
            require(e['numeric_year_part'] is UNDEFINED if absent else e['numeric_year_part'] in p and e['numeric_year_part']['type'] in ('year','relatedYear'),'Wrong original numeric year lookup')
            if absent: require(e['numeric_year_part'] is UNDEFINED,'Wrong unavailable numeric year'); named(p,e['calendar_year'],UNDEFINED,data); required |= {'premise-no-numeric-year-only-lunisolar','premise-related-year-integer','premise-selected-U-correct-calendar-name'}
    elif guard=='temporal-numeric-hour':
        required={'premise-original-hourcycle-call','premise-original-hourcycle-options-shape','premise-original-hourcycle-option','premise-selected-English-latn','premise-hourcycle-option-resolution','premise-original-midnight-temporal-input','premise-original-midnight-numeric-cycle','premise-original-midnight-period'}
        params=site['premise_parameters']; r=e['selected']; auxiliary_equal('hourcycle-resolved-probe',r); midnight=e['midnight_input']; require(midnight.get('epochNanoseconds')=='0' and midnight.get('timeZoneId',UNDEFINED) in ('UTC',UNDEFINED) or set(midnight)=={'hour','minute','second','millisecond','microsecond','nanosecond'} and all(type(v) is int and v==0 for v in midnight.values()),'Wrong original midnight input'); selected(r); require(r.get('hourCycle')==params['hourCycle'] and r.get('hour12')==(params['hourCycle'] in ('h11','h12')) and r.get('timeZone')=='UTC','Wrong selected cycle')
        receipt=original(proof['original_result_event'],receipts,r'\.toLocaleString\s*\('); input_receipt=original(receipt.get('receiver',{}).get('call_event'),receipts,r'new\s+Temporal\.(?:Instant|PlainTime|PlainDateTime|ZonedDateTime)'); input_args=[decode(v) for v in input_receipt['arguments']]; input_source=input_receipt['source']; require(input_args==[0] if 'Temporal.Instant' in input_source else input_args==[0,'UTC'] if 'Temporal.ZonedDateTime' in input_source else all(v==0 for v in input_args) if 'Temporal.PlainTime' in input_source else len(input_args)>=3 and all(v==0 for v in input_args[3:]),'Wrong original Temporal constructor midnight'); require(receipt['site']==params['call_site'] and e['original_call_event']==proof['original_result_event'],'Wrong original cycle site'); result_equal(receipt,e['result'])
        require(len(receipt['arguments'])==2 and decode(receipt['arguments'][0])=='en' and decode(receipt['arguments'][1])==e['original_options'],'Wrong original cycle options')
        key,value=params['option'].split(':'); require(e['option']==params['option'] and set(e['original_options']) <= {key,'timeZone'} and str(e['original_options'][key]).lower()==value.lower() and e['original_options'].get('timeZone','UTC')=='UTC','Wrong requested cycle')
        matches=re.findall(r'(?:^|[^0-9])([0-9]{1,2}):([0-9]{2}):([0-9]{2})(?=[^0-9]|$)',e['result']); require(len(matches)==1 and int(matches[0][0])==params['hour'] and matches[0][1:]==('00','00') and e['expected_numeric_hour']==params['hour'] and e['actual_numeric_hour']==params['hour'],'Wrong original numeric cycle'); require(not r['hour12'] or re.search(r'(?:^|[^A-Za-z])AM(?:[^A-Za-z]|$)',e['result']) is not None,'Wrong midnight period'); require(permit is True,'Wrong cycle permit')
    elif guard=='unmatched-locale-hc-default':
        required={'premise-original-unmatched-hc-origin','premise-original-unmatched-hc-options','premise-original-unmatched-hc-extension','premise-unmatched-hc-support','premise-unmatched-hc-selected-default','premise-unmatched-hc-original-comparison'}
        params=site['premise_parameters']; ctor=original(proof['formatter_event'],receipts,r'new\s+Intl\.DateTimeFormat'); resolved=original(proof['original_result_event'],receipts,r'\.resolvedOptions\s*\('); receiver(resolved,proof['formatter_event']); result_equal(resolved,e['resolved'])
        require(resolved['arguments']==[] and ctor['site']==params['constructor_site'],'Wrong original resolvedOptions/constructor site')
        require([decode(v) for v in ctor['arguments']]==[e['requested'],e['original_options']] and decode(ctor['resolved'])==e['selected'] and e['resolved']==e['selected'],'Wrong original selected/options binding')
        require(e['requested_cycle'] in ('h11','h12','h23','h24') and e['requested']=='de-u-hc-'+e['requested_cycle'],'Wrong exact unmatched extension')
        options=e['original_options']; mode=params['option_mode']; require(mode in ('hour','timeStyle') and set(options)=={mode} and (options['hour']=='numeric' if mode=='hour' else options['timeStyle'] in ('full','long','medium','short')),'Explicit cycle option must remain mandatory')
        auxiliary_equal('unmatched-hc-supported',[]); require(e['supported']==[],'Matched requested extension must remain mandatory'); r=e['selected']; fallback=data['fallbackHourCycle']; require(fallback==dict(requested_base='de',selected_locale='en-US',hourCycle='h12',hour12=True),'Wrong source-bound fallback hour data')
        require(all(r.get(k)==v for k,v in dict(locale=fallback['selected_locale'],calendar='gregory',numberingSystem='latn',timeZone='UTC',hourCycle=fallback['hourCycle'],hour12=fallback['hour12']).items()) and (r.get('hour')=='numeric' and 'timeStyle' not in r if mode=='hour' else r.get('timeStyle')==options['timeStyle']),'Wrong selected default/fields')
        field=params['field']; require(field in ('hourCycle','hour12') and e['compared']==[r[field],e['requested_cycle'] if field=='hourCycle' else e['requested_cycle'] in ('h11','h12')] and permit is True,'Wrong original cycle comparison/permit')
    elif guard=='unsupported-es-selected-unit-list':
        required={'premise-original-selected-list-origin','premise-original-selected-list-options','premise-selected-list-unsupported-fallback','premise-selected-list-source-templates','premise-selected-list-original-input-elements','premise-selected-list-exact-parts-keys','parts-array','part-object-prototype','part-data-property-type','part-data-property-value','part-type-domain','list-part-type-domain','premise-selected-list-element-order-and-literals','premise-selected-list-original-literal-leaf','premise-selected-list-same-receiver-join'}
        params=site['premise_parameters']; ctor=original(proof['formatter_event'],receipts,r'new\s+Intl\.ListFormat'); receipt=original(proof['parts_event'],receipts,r'\.formatToParts\s*\(',list_parts=True); receiver(receipt,proof['formatter_event']); result_equal(receipt,e['actual_parts'])
        r=e['selected']; require(decode(ctor['resolved'])==r and r==dict(locale='en-US',style=params['style'],type='unit') and [decode(v) for v in ctor['arguments']]==['es-ES',e['original_options']] and e['original_options']==dict(style=params['style'],type='unit'),'Wrong selected list constructor/options')
        auxiliary_equal('selected-list-supported',[]); require(e['supported']==[] and data['selectedUnitListTemplates'][params['style']]==['{0}, {1}']*4,'Wrong selected support/template data')
        require(len(receipt['arguments'])==1,'Wrong original list argument count'); raw=receipt['arguments'][0]; elements=e['elements']; require(isinstance(elements,list) and all(isinstance(x,str) for x in elements),'Missing original list elements')
        if raw.get('kind')=='call-reference':
            it=original(raw['call_event'],receipts,r'\[Symbol\.iterator\]\s*\('); require(it['arguments']==[] and decode(it['receiver'])==elements,'Wrong original Array iterator source input')
        else:
            input=decode(raw); require(input==elements if isinstance(input,list) else input=='foo' and elements==list('foo') if isinstance(input,str) else isinstance(input,dict) and input==dict(i=0,array=elements),'Wrong original list/custom iterator input')
        require(elements in [[],['foo'],['foo','bar'],['foo','bar','baz'],['foo','bar','baz','quux'],list('foo')],'Wrong exact official source element sequence')
        expected=[]
        for value in elements:
            if expected: expected.append(dict(type='literal',value=', '))
            expected.append(dict(type='element',value=value))
        require(e['actual_parts']==expected and len(e['expected_parts'])==len(expected) and all(p['type']==q['type'] and isinstance(p['value'],str) and (p['type']=='literal' or p['value']==q['value']) for p,q in zip(e['expected_parts'],expected)),'Wrong selected parts/mandatory element count/order/type/value')
        index=e['index']; require(type(index) is int and 0<=index<len(expected) and expected[index]['type']=='literal' and e['expected_parts'][index]['type']=='literal','Wrong guarded literal leaf')
        auxiliary_equal('selected-list-format-join',e['joined']); require(e['joined']==''.join(p['value'] for p in expected) and proof['join_text']==e['joined'] and permit is True,'Wrong original selected list join/permit')
    elif guard=='arEG-fallback-latn':
        required={'premise-original-formatter-origin','premise-selected-English-latn','premise-arEG-unsupported-fallback','parts-array','part-object-prototype','part-data-property-type','part-data-property-value','part-type-domain','premise-numeric-field-day','premise-numeric-field-month','premise-numeric-field-year','premise-extreme-date-option-resolution','premise-extreme-date-format-parts-join','premise-extreme-date-source-values'}
        selected_origin(proof,e,receipts); receipt=original(proof['original_result_event'],receipts,r'\.format\s*\('); receiver(receipt,proof['formatter_event']); result_equal(receipt,e['result']); require(len(receipt['arguments'])==1 and receipt['arguments'][0].get('kind')=='call-reference','Wrong extreme date original input'); date=original(receipt['arguments'][0]['call_event'],receipts,r'new\s+Temporal\.PlainDate'); require([decode(v) for v in date['arguments']]==[e['iso_year'],e['month'],e['day']],'Extreme original date constructor differs'); auxiliary_equal('ar-EG-supportedLocalesOf',[]); auxiliary_equal('extreme-plain-date-parts',e['actual_parts']); selected(e['selected'],'iso8601'); require(all(e['selected'].get(k)=='numeric' for k in ('year','month','day')),'Wrong extreme date fields'); parts(e['actual_parts']); require((e['iso_year'],e['month'],e['day']) in ((275760,9,13),(-271821,4,19)),'Wrong extreme source values')
        for field in ('year','month','day'): numeric(e['actual_parts'],field,(1-e['iso_year'] if e['iso_year']<=0 else e['iso_year']) if field=='year' else e[field])
        require(e['result']==''.join(row['value'] for row in e['actual_parts']) and permit is True,'Wrong extreme join/permit')
    elif guard=='collator-default-only':
        required={'premise-original-collator-option-origin','premise-collator-support-policy','premise-required-collation-option-extension-resolution'}
        auxiliary_equal('collator-original-locale-support',e['supported']); receipt=original(proof['original_result_event'],receipts,r'\.resolvedOptions\s*\('); result_equal(receipt,e['resolved']); ctor=original(receipt['receiver']['call_event'],receipts,r'new\s+Intl\.Collator'); require(decode(ctor['arguments'][0])==e['locale'] and decode(ctor['arguments'][1])['collation']==e['collation'],'Wrong collator request')
        require(e['original_resolved_event']==proof['original_result_event'],'Wrong resolved receipt identity'); base=e['locale'].split('-u-')[0]; r=e['resolved']; require(base in ('en','de') and e['collation'] in ('pinyin','eor','phonebk') and e['supported']==([e['locale']] if base=='en' else []),'Wrong collator support'); require(all(r.get(k)==v for k,v in dict(locale='en' if base=='en' else 'en-US',collation='default',usage='sort',numeric=False,caseFirst='false',sensitivity='variant',ignorePunctuation=False).items()) and e['selected_co']==[None] and permit is True,'Wrong collator option resolution')
    elif guard=='locale-default-only':
        required={'premise-original-locale-collations-origin','premise-collations-array-shape-domain-order','premise-locale-collation-supported-default-only-or-fallback'}
        auxiliary_equal('locale-collator-prefix-support',e['supported']); receipt=original(proof['original_result_event'],receipts,r'\.getCollations\s*\('); result_equal(receipt,e['collations']); ctor=original(receipt['receiver']['call_event'],receipts,r'new\s+Intl\.Locale'); require(decode(ctor['arguments'][0])==e['tag'],'Wrong locale request'); a=e['collations']; require(all(isinstance(x,str) and x not in ('standard','search') for x in a) and a==sorted(set(a)),'Wrong locale array/order'); expected=e['supported']==['en'] and e['tag']=='en' and a==[]; require(expected or e['supported']==[] and a==['emoji','eor'],'Wrong locale fallback'); require(permit==expected and e['selected_co']==([None] if expected else None),'Wrong locale permit')
    elif guard=='tz-primary-identity':
        required={'premise-known-current-zone-records','premise-original-timezone-constructor-inputs'};
        require(isinstance(proof['original_arguments'],list) and len(proof['original_arguments'])==2,'Missing original timezone constructors')
        for i,zone in zip(proof['original_arguments'],[e['link'],e['zone']]):
            ctor=original(i,receipts,r'new\s+Temporal\.ZonedDateTime'); require([decode(v) for v in ctor['arguments']]==[int(e['epochNs']),zone],'Wrong original timezone input')
        selected_records = config.get('selected_provider_input', {}).get('records', data['timezoneRecords'])
        a=selected_records[e['link']]; z=selected_records[e['zone']]; same=a['primary']==z['primary']; require(e['link_primary']==a['primary'] and e['zone_primary']==z['primary'] and permit==(not same),'Wrong timezone primary source')
        require(e['operation']==site['premise_parameters']['operation'],'Wrong zone operation')
        if e['operation']=='equals':
            required |= {'premise-required-primary-identity-equality','premise-original-timezone-equals-origin'}; receipt=original(proof['original_result_event'],receipts,r'\.equals\s*\('); receiver(receipt,proof['original_arguments'][0]); require(receipt['arguments']==[{'kind':'call-reference','call_event':proof['original_arguments'][1]}] and decode(receipt['value'])==e['actual'][0],'Wrong original equals result/inputs'); require(e['actual']==[same,True],'Wrong zone equals result')
        else: required |= {'premise-offset-epoch-source-coverage','premise-zone-independent-source-offset-values'}; require(e['epochNs'] in a['offsets'] and e['epochNs'] in z['offsets'] and e['actual']==[a['offsets'][e['epochNs']],z['offsets'][e['epochNs']]],'Wrong zone offset samples')
    else:
        raise PremiseProofInvalid('Unknown guard branch')
    if 'parts-array' in required and '/DateTimeFormat/' in config['fixture']:
        required.add('number-date-parts-nonempty')
    require(required <= names, 'Missing branch-specific invariant: ' + ', '.join(sorted(required-names)))
    return e
