"""Mechanical source ownership changes for the split-05 migration."""
from split05_lib import functions


def split_sources(sources, license_text):
    sources = dict(sources)
    moves = []
    prefix = 'src/quickjs/'
    vm_source=sources[prefix+'function-vm.c']
    vm_prologue=vm_source[len(license_text):min(f['start'] for f in functions(vm_source))]

    def take_range(owner, target, start, end):
        owner, target = prefix + owner, prefix + target
        text = sources[owner]
        block = text[start:end]
        names = [f['name'] for f in functions(block)]
        assert names, (owner, target)
        sources[owner] = text[:start] + text[end:]
        if target not in sources:
            sources[target] = license_text + '\n'
        sources[target] += '\n' + block + '\n'
        moves.extend({'name': n, 'from': owner, 'to': target} for n in names)

    def take_functions(owner, target, names):
        wanted = set(names)
        found = [f for f in functions(sources[prefix + owner]) if f['name'] in wanted]
        assert set(f['name'] for f in found) == wanted, (owner, wanted)
        # Preserve original order in the destination and offsets in the source.
        blocks = []
        for f in reversed(found):
            text = sources[prefix + owner]
            start = f['start']
            # Move immediately adjacent comments with their implementation.
            before = text[:start].rstrip()
            if before.endswith('*/'):
                comment_start = before.rfind('/*')
                if comment_start >= 0 and '\n' not in text[len(before):start].strip():
                    start = comment_start
            blocks.append((start, f['end']))
        before = len(sources.get(prefix + target, ''))
        for start, end in reversed(blocks):
            # Prior removals change later offsets: locate by stable function name.
            original_name = next(f['name'] for f in found if f['end'] == end)
            text = sources[prefix + owner]
            current = next(f for f in functions(text) if f['name'] == original_name)
            delta = current['start'] - next(f['start'] for f in found if f['name'] == original_name)
            take_range(owner, target, start + delta, end + delta)

    # BigInt core is one contiguous upstream section, including limb helpers,
    # local tables and conditional 32/64-bit code. Generic conversions stay put.
    text = sources[prefix + 'number.c']
    end = next(f['end'] for f in functions(text) if f['name'] == 'JS_CompactBigInt')
    take_range('number.c', 'bigint.c', text.index('/* bigint support */'), end)
    take_functions('number.c', 'bigint.c', [
        'js_bigint_to_string', 'JS_NewBigInt64', 'JS_NewBigUint64',
        'JS_StringToBigInt', 'JS_StringToBigIntErr', 'JS_ToBigIntFree',
        'JS_ToBigInt', 'JS_ToBigInt64Free', 'JS_ToBigInt64', 'JS_ToBigIntBuf',
        'js_compare_bigint', 'js_bigint_sign',
    ])

    text = sources[prefix + 'object.c']
    end = next(f['end'] for f in functions(text) if f['name'] == 'JS_DumpShapes')
    take_range('object.c', 'shape.c', text.index('/* Shape support */'), end)
    text = sources[prefix + 'object.c']
    fs = {f['name']: f for f in functions(text)}
    take_range('object.c', 'property.c', fs['JS_SetImmutablePrototype']['start'],
               fs['JS_DeletePropertyInt64']['end'])
    take_functions('property.c', 'shape.c', ['js_shape_prepare_update', 'js_update_property_flags'])
    take_functions('object.c', 'shape.c', ['get_shape_prop', 'find_own_property1', 'find_own_property'])
    take_functions('object.c', 'property.c', ['js_obj_to_desc'])
    take_functions('function-vm.c', 'property.c', ['JS_CopyDataProperties'])

    # Keep the interpreter's stack-specific iterator operations in the VM.
    take_functions('function-vm.c', 'iterator.c', [
        'build_for_in_iterator', 'js_for_in_prepare_prototype_chain_enum',
        'JS_GetIterator2', 'JS_GetIterator', 'JS_IteratorNext2', 'JS_IteratorClose',
        'JS_IteratorGetCompleteValue', 'js_iterator_get_value_done',
        'js_create_iterator_result', 'JS_IteratorNext', 'js_get_fast_array',
    ])
    take_functions('object.c', 'iterator.c', ['js_for_in_iterator_finalizer', 'js_for_in_iterator_mark'])

    # Move the whole async/generator block together with its local record types.
    text = sources[prefix + 'function-vm.c']
    fs = {f['name']: f for f in functions(text)}
    take_range('function-vm.c', 'function.c', fs['JS_Call']['start'],
               fs['js_async_generator_function_call']['end'])
    # Function/closure setup precedes the interpreter. Select the non-VM pieces
    # as a whole block, leaving its direct stack operations in function-vm.c.
    text = sources[prefix + 'function-vm.c']
    fs = {f['name']: f for f in functions(text)}
    take_range('function-vm.c', 'function.c', fs['free_var_ref']['start'],
               fs['js_call_bound_function']['end'])
    take_functions('function.c', 'function-vm.c', [
        'js_for_in_start', 'js_for_in_next', 'js_for_of_start',
        'js_for_of_next', 'js_for_await_of_next', 'js_append_enumerate',
        'js_op_define_class',
    ])
    # Stack helpers precede the dispatch function, as in upstream; this avoids
    # adding a new header surface for functions private to the VM.
    key=prefix+'function-vm.c'
    text=sources[key]
    fs={f['name']:f for f in functions(text)}
    start=fs['js_for_in_start']['start']
    end=fs['js_op_define_class']['end']
    helpers=text[start:end]
    text=text[:start]+text[end:]
    at=text.index('#ifdef OPCODE_ASM_LABEL')
    sources[key]=text[:at]+helpers+'\n\n'+text[at:]
    # Retain the original forward declarations ahead of the rearranged groups.
    prologue = vm_prologue
    sources[prefix + 'function.c'] = license_text + prologue + sources[prefix + 'function.c'][len(license_text):]
    text = sources[prefix + 'object.c']
    fs = {f['name']: f for f in functions(text)}
    take_range('object.c', 'function.c', fs['js_function_set_properties']['start'],
               fs['JS_NewCFunctionData']['end'])
    take_functions('object.c', 'function.c', [
        'js_c_function_finalizer', 'js_c_function_mark',
        'js_bytecode_function_finalizer', 'js_bytecode_function_mark',
        'js_bound_function_finalizer', 'js_bound_function_mark',
    ])
    return sources, moves
