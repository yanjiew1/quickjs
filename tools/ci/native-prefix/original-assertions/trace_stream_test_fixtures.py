"""Pure source-bound guard trace fixtures for streaming validation tests."""
import hashlib
from guard_binding import canonical_sha

def config_for(kind='assertion'):
    guard = 'relatedyear-selected-U'
    data = dict(source_files={'src/intl/date-native.c':'a'*64}, yearNames=['other']*60)
    data['yearNames'][15]='ji-mao'
    profile = dict(defaults={'DateTimeFormat':{'locale':'en-US','timeZone':'UTC','numberingSystem':'latn'},'Collator':{'collation':'default'}})
    review = dict(schema='native-premise-root-source-guard-review-v1',source_review_complete=True,
        conformance_accepted=False,semantic_adjudication_complete=False,
        engine_sha256='2'*64,source_tree_sha256='3'*64,profile_sha256='4'*64,
        profile_canonical_sha256=canonical_sha(profile),reviewed_guard_sites={'u':guard},
        guard_policy_sha256='b'*64,runtime_sha256='c'*64,trace_proof_sha256='a'*64,source_pins_sha256='d'*64,premise_data_sha256='e'*64)
    review['helper_sha256']={name:('a'*64 if name=='premise_trace_proof.py' else 'f'*64) for name in ('render_practical_witness.py','validate_practical_witness.py','guard_binding.py','premise_trace_proof.py')}
    binding = dict(source_guard_review=review,root_guard_review_sha256=canonical_sha(review),current_source_files=data['source_files'].copy(),
        helper_sha256=review['helper_sha256'].copy(),source_pins_canonical_sha256=canonical_sha(data['source_files']),premise_data_canonical_sha256=canonical_sha(data),
        **{k:review[k] for k in ('engine_sha256','source_tree_sha256','profile_sha256','guard_policy_sha256','runtime_sha256','trace_proof_sha256','source_pins_sha256','premise_data_sha256')})
    config = dict(fixture='mock.js',fixture_sha256='1'*64,variant='strict',engine_sha256='2'*64,source_tree_sha256='3'*64,profile_sha256='4'*64,
        profile=profile,premise_bindings=binding,premise_data=data,max_events=200,
        sites={'f':dict(kind='constructor',source='new Intl.DateTimeFormat(\"en\", options)',source_sha256='a'*64),
        'p':dict(kind='call',source='formatter.formatToParts(0)',source_sha256='b'*64),
        'h':dict(kind='helper',source='function helper() {}',source_sha256='5'*64),
        'u':dict(kind=kind,source='assert.sameValue(typeof part.value, "string")',source_sha256='6'*64,
            assertion_method='sameValue',continue_exact_data_assertion=True,premise_guard=guard,
            unavailable_premise=guard,unavailable_adapter_kind='year-read' if kind == 'value' else 'assertion-operands')})
    return config


def typed(value):
    if value is None:return {'kind':'null'}
    if value is ...:return {'kind':'undefined'}
    if isinstance(value,bool):return {'kind':'boolean','value':value}
    if isinstance(value,int):return {'kind':'number','value':str(value)}
    if isinstance(value,str):
        raw=value.encode('utf-16-le',errors='surrogatepass')
        return {'kind':'string','value':value,'utf16':[int.from_bytes(raw[i:i+2],'little') for i in range(0,len(raw),2)],'codepoints':[ord(c) for c in value]}
    array=isinstance(value,list); items=[(str(i),v) for i,v in enumerate(value)] if array else list(value.items())
    if array:items.append(('length',len(value)))
    return {'kind':'array' if array else 'object','properties':[dict(key=typed(k),value=typed(v),enumerable=not(array and k=='length'),configurable=not(array and k=='length'),writable=True) for k,v in items]}


def trace(unavailable=True,permit=True,kind='assertion'):
    config=config_for(kind)
    records=[]
    def add(event_kind,sid=None,**fields):
        records.append(dict(kind=event_kind,site=sid,**fields));return len(records)
    add('start',original_entry_reached=True)
    helper=add('enter','h',operation='helper',parent=None,arguments=[])
    selected=dict(locale='en',calendar='chinese',numberingSystem='latn',timeZone='UTC',year='numeric',month='numeric',day='numeric')
    actual_parts=[dict(type='month',value='11'),dict(type='literal',value='/'),dict(type='day',value='25'),dict(type='literal',value='/'),dict(type='yearName',value='ji-mao')]
    fe=add('enter','f',operation='constructor',parent=helper)
    fv=add('value','f',operation_entry=fe,role='native-result',value=typed({}))
    add('value','f',operation_entry=fe,role='constructor-resolved-options',value=typed(selected))
    add('exit','f',operation='constructor',entry=fe,completion='normal',arguments=[typed('en'),typed({})],receiver=None,result_event=fv)
    pe=add('enter','p',operation='call',parent=helper)
    pv=add('value','p',operation_entry=pe,role='native-result',value=typed(actual_parts))
    add('exit','p',operation='call',entry=pe,completion='normal',arguments=[typed(0)],receiver={'kind':'call-reference','call_event':fv},result_event=pv)
    join=add('value','u',role='auxiliary-premise-data',auxiliary=True,auxiliary_role='calendar-format-join',value=typed('11/25/ji-mao'))
    if unavailable:add('invariant','u',name='premise-exact-unavailable-obligation-allowlist',mandatory=True,passed=True)
    names=('premise-root-binding','premise-exact-production-source','premise-effective-profile','premise-context-kind','premise-original-formatter-origin','premise-selected-English-latn','premise-selected-lunisolar-calendar','parts-array','part-object-prototype','part-data-property-type','part-data-property-value','part-type-domain','premise-no-selected-r-token','premise-related-year-integer','premise-selected-U-correct-calendar-name','premise-original-parts-origin','premise-calendar-parts-format-join')
    for name in names:add('invariant','u',name=name,mandatory=True,passed=True)
    evidence=dict(calendar='chinese',expectedType='relatedYear',expectedValue=1999,part=...,actual_parts=actual_parts,selected=selected,source=...,selected_U_without_r=True,
        proof=dict(schema='native-premise-branch-proof-v2',guard_site='u',formatter_event=fv,parts_event=pv,original_result_event=None,join_text='11/25/ji-mao',original_arguments=[0],auxiliary_events=[dict(event=join,role='calendar-format-join')]))
    guard=add('premise-guard','u',guard_id='relatedyear-selected-U',passed=True,permit_snapshot_continuation=permit,
        root_guard_review_sha256=config['premise_bindings']['root_guard_review_sha256'],evidence=typed(evidence))
    if unavailable:
        for name in ('premise-unavailable-expression-prerequisite','premise-unavailable-original-helper-binding','premise-original-undefined-dereference-threw-TypeError'):
            add('invariant','u',name=name,mandatory=True,passed=True)
        add('unavailable-obligation','u',helper_entry=helper,guard_event=guard,prerequisite='relatedyear-selected-U',
            adapter_kind=config['sites']['u']['unavailable_adapter_kind'],original_evaluation_performed=True,original_assertion_invoked=False,
            original_assertion_passed=False,original_abrupt_premise=True,original_error={'test262_error':False,'message':"cannot read property 'value' of undefined"},
            error_constructor='TypeError',original_component={'kind':'undefined'},actual={'kind':'undefined'},expected={'kind':'undefined'},continued=True,
            source_expression=config['sites']['u']['source'],all_original_operand_bytes_preserved=True)
    else:
        entry=add('enter','u',operation='assertion',parent=helper,arguments=[{'kind':'undefined'},{'kind':'number','value':'1999'}],mandatory_override=False,premise_guard_event=guard)
        add('exit','u',operation='assertion',entry=entry,completion='throw',original_assertion_passed=False,continued=True,error={'test262_error':True})
    add('exit','h',operation='helper',entry=helper,completion='normal',result={'kind':'undefined'})
    add('completion',original_eof_reached=True,fixture_capture_complete=True,site_counts={'h':1,'u':1,'f':1,'p':1},data_mismatches=1,
        mandatory_failed_site_ids=[],continued_data_site_ids=['u'],totalized_original_data_obligations=int(unavailable),all_original_expressions_evaluated=not unavailable)
    command=dict(returncode=0,timed_out=False,interrupted=False,lifecycle_closed=True,output_limited=False,cleanup_failed=False,
        generated_sha256='7'*64,stderr_sha256=hashlib.sha256(b'').hexdigest(),**{k:config[k] for k in ('engine_sha256','source_tree_sha256','profile_sha256')})
    raw=dict(returncode=1,raw_preserved=True,counters={'failed':123})
    return records,config,command,raw

