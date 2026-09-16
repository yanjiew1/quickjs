#!/usr/bin/env python3
"""Reproduce the correctness-first split from pinned, unmodified source inputs.

This one-off migration keeps upstream bodies; it is not used by the build.
"""
from __future__ import annotations
import argparse
import collections
import json
import re
import subprocess
from pathlib import Path
from split05_lib import functions, mask_c, data_definitions
from split05_layout import split_sources

BASE = 'b63ab025d402e46c647a8a239bffe72a58f73a35'
UPSTREAM = '04be246001599f5995fa2f2d8c91a0f198d3f34c'
ROOT = Path(__file__).resolve().parent.parent

def git_read(ref, path):
    return subprocess.check_output(['git','show',f'{ref}:{path}'],cwd=ROOT).decode()

def replace_ranges(text, edits):
    last=len(text)+1
    for start,end,new in sorted(edits,reverse=True):
        assert end <= last, (start,end,last)
        text=text[:start]+new+text[end:]
        last=start
    return text

def signature(f, external=False):
    sig=f['sig']
    if external and re.search(r'\bstatic\b',sig):
        sig=re.sub(r'\bstatic\s+', 'QJS_INTERNAL ',sig,count=1)
        sig=re.sub(r'\b(?:inline|force_inline)\s+', '',sig)
    return sig

def definition(f,external=False):
    return signature(f,external)+f['text'][len(f['sig']):]

def proto_names(text):
    return set(re.findall(r'(?m)^QJS_INTERNAL[^;{}]*?\b(\w+)\s*\([^;{}]*\);', text))

def strip_remaps(s):
    # Match complete logical preprocessor lines, including continuation lines.
    return re.sub(r'(?m)^#define[^\n]*(?:\n(?<=[\\]\n)[^\n]*)*',
                  lambda m: '' if re.search(r'\bqjs_\w*|\b\w+_inline\b',m.group()) else m.group(),s)

def normalize_prototypes(s, uf, exposed=None, header=False):
    pat=re.compile(r'(?m)^([A-Za-z_][^;{}#=]*?\([^;{}]*?\));')
    edits=[]
    bodies=functions(s)
    for m in pat.finditer(s):
        if any(f['start'] <= m.start() < f['end'] for f in bodies):continue
        sig=m.group(1)
        if any(x in sig for x in ['/*','*/','//']) or sig.startswith('typedef'):continue
        fs=functions(sig+'\n{}')
        if len(fs)!=1: continue
        name=fs[0]['name']
        if name not in uf:continue
        f=uf[name][0]
        ext=header or 'QJS_INTERNAL' in sig or (exposed is not None and name in exposed)
        edits.append((m.start(),m.end(),signature(f,ext)+';'))
    return replace_ranges(s,edits)

def clean_header(s):
    # Owner guards only selected private inline twins in the earlier split.
    pat=re.compile(r'(?m)^#ifndef QUICKJS_\w+_OWNER\n')
    while (m:=pat.search(s)):
        depth=1;other=None;stop=None
        for directive in re.finditer(r'(?m)^#(if\w*|else|endif)\b[^\n]*\n?',s[m.end():]):
            kind=directive.group(1)
            if kind.startswith('if'):depth+=1
            elif kind=='endif':
                depth-=1
                if depth==0:
                    stop=(m.end()+directive.start(),m.end()+directive.end());break
            elif kind=='else' and depth==1:other=(m.end()+directive.start(),m.end()+directive.end())
        assert stop is not None
        first=s[m.end():other[0] if other else stop[0]]
        if other:
            second=s[other[1]:stop[0]]
            assert ''.join(first.split())==''.join(second.split()) or not second.strip(), (first,second)
        s=s[:m.start()]+first+s[stop[1]:]
    s=re.sub(r'(?m)^#define QJS_(?!INTERNAL\b)\w+[^\n]*\n?', '',s)
    s=re.sub(r'(?ms)^(?:typedef )?enum(?: QJS\w+)? \{\n(?:[ \t]*QJS_\w+[^\n]*\n)+\}(?: QJS\w+)?;\n?', '',s)
    return s

def clean_gaps(s):
    # Do not reformat function bodies or data initializers, including upstream
    # trailing spaces. Only remove blank debris left by the mechanical moves.
    spans=sorted((f['start'],f['end']) for f in functions(s)+data_definitions(s))
    output=[];at=0
    for start,end in spans:
        if start<at:continue
        output.append(re.sub(r'\n(?:[ \t]*\n){2,}', '\n\n',s[at:start]))
        output.append(s[start:end]);at=end
    output.append(re.sub(r'\n(?:[ \t]*\n){2,}', '\n\n',s[at:]))
    return ''.join(output).rstrip()+'\n'

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--base-ref', default=BASE)
    ap.add_argument('--upstream-dir', type=Path)
    args=ap.parse_args()
    def upstream(path):
        return (args.upstream_dir/path).read_text() if args.upstream_dir else git_read(UPSTREAM,path)
    usrc=upstream('quickjs.c')
    uf=collections.defaultdict(list)
    for f in functions(usrc): uf[f['name']].append(f)
    allpaths=subprocess.check_output(['git','ls-tree','-r','--name-only',args.base_ref,'src/quickjs'],cwd=ROOT).decode().splitlines()
    originals={p:git_read(args.base_ref,p) for p in allpaths if p.endswith(('.c','.h'))}
    cs={p:s for p,s in originals.items() if p.endswith('.c')}
    hs={p:s for p,s in originals.items() if p.endswith('.h')}
    license=usrc[:usrc.index('#include')]
    public=set(f['name'] for f in functions(upstream('quickjs.h')))
    # Replace every existing upstream-derived implementation, not just wrappers.
    newcs={}
    seen=collections.Counter()
    owners={}
    removed=[]
    for path,s in cs.items():
        edits=[]
        local_index=collections.Counter()
        for f in functions(s):
            name=f['name']
            if name in ('js_get_stack_pointer','js_check_stack_overflow'):
                edits.append((f['start'],f['end'],''));continue
            if name not in uf:
                assert name.startswith('qjs_') or name=='js_base_get_function_bytecode', name
                removed.append(name)
                edits.append((f['start'],f['end'],''));continue
            i=local_index[name]; local_index[name]+=1
            assert i < len(uf[name]), (name,i)
            target=uf[name][i]
            edits.append((f['start'],f['end'],definition(target,'QJS_INTERNAL' in f['sig'])))
            seen[name]+=1
            owners[name]=path
        s=replace_ranges(s,edits)
        # Remove split-only caller remaps; preserve upstream call spelling.
        s=strip_remaps(s)
        newcs[path]=s
    header_owner={
        'allocator':'allocator','runtime':'runtime','array':'builtin-array',
        'function':'function-vm','iterator':'function-vm','number':'number',
        'property':'object','regexp':'object','string':'atom-string',
    }
    forced_owner={'js_rc':'allocator','array_buffer_is_resizable':'builtin-typed-array',
                  'set_value':'object'}
    added=collections.defaultdict(list)
    newhs={}
    for path,s in hs.items():
        edits=[]
        for f in functions(s):
            name=f['name']
            if name in uf:
                if name not in seen and name != 'js_check_stack_overflow':
                    module=forced_owner.get(name,header_owner[Path(path).stem.removeprefix('internal-')])
                    owner=f'src/quickjs/{module}.c'
                    added[owner].append(definition(uf[name][0], True))
                    owners[name]=owner;seen[name]+=1
                edits.append((f['start'],f['end'],signature(uf[name][0], True)+';'))
            else:
                removed.append(name)
                edits.append((f['start'],f['end'],''))
        s=replace_ranges(s,edits)
        # Private remaps and non-upstream forward declarations disappear.
        s=strip_remaps(s)
        s=re.sub(r'(?m)^(?:static\s+|QJS_INTERNAL\s+)[^;{}]*?\b(?:qjs_\w+|\w+_inline)\s*\([^;{}]*\);\n?', '',s)
        # Remaining upstream static declarations become out-of-line declarations.
        s=re.sub(r'(?m)^static\s+(?:force_inline\s+|inline\s+)?([^;{}]*\([^;{}]*\));',r'QJS_INTERNAL \1;',s)
        # Remove comments describing the now-removed optimization architecture.
        s=re.sub(r'/\*[^*]*(?:\*(?!/)[^*]*)*\*/', lambda m: '' if any(k in m.group() for k in ['monolithic','inline across','zero-ref path','qjs_']) else m.group(),s)
        newhs[path]=s
    # Preserve the two upstream stack-check configurations, not the fork's fused body.
    group='#if !defined(CONFIG_STACK_CHECK)\n'
    group+=definition(uf['js_get_stack_pointer'][0])+'\n\n'
    group+=definition(uf['js_check_stack_overflow'][0],True)+'\n#else\n'
    group+=definition(uf['js_get_stack_pointer'][1])+'\n\n'
    group+=definition(uf['js_check_stack_overflow'][1],True)+'\n#endif\n'
    added['src/quickjs/runtime.c'].insert(0,group)
    owners['js_get_stack_pointer']='src/quickjs/runtime.c'
    owners['js_check_stack_overflow']='src/quickjs/runtime.c'
    for path,defs in added.items():
        s=newcs[path]
        if path.endswith('/runtime.c'):
            # A forward declaration preserves the upstream TU-local stack helper.
            at=re.search(r'(?m)^#include[^\n]*\n',s).end()
            s=s[:at]+'static inline uintptr_t js_get_stack_pointer(void);\n'+s[at:]
        newcs[path]=s+'\n\n'+'\n\n'.join(defs)+'\n'
    # Restore original data initializers as well as function bodies.
    upstream_data={d['name']:d for d in data_definitions(usrc)}
    for path,s in newcs.items():
        edits=[]
        for d in data_definitions(s):
            if d['name'] in upstream_data:
                text=upstream_data[d['name']]['text']
                if d['text'].startswith('QJS_INTERNAL'):
                    text=re.sub(r'^static\s+','QJS_INTERNAL ',text)
                edits.append((d['start'],d['end'],text))
        newcs[path]=replace_ranges(s,edits)
    # The old Proxy wrapper used an automatic class table; upstream owns the
    # canonical static table immediately before its intrinsic initializer.
    path='src/quickjs/builtin-proxy.c'
    at=next(f['start'] for f in functions(newcs[path]) if f['name']=='JS_AddIntrinsicProxy')
    newcs[path]=newcs[path][:at]+upstream_data['js_proxy_class_def']['text']+'\n\n'+newcs[path][at:]
    newcs['src/quickjs/function-vm.c']=newcs['src/quickjs/function-vm.c'].replace('#define JS_EQ_STRICT QJS_EQ_STRICT\n','')
    # Move shared representation/type declarations, not bodies. These are
    # intentional temporary exposures needed by the verbatim upstream callers.
    shared=[]
    for name,owner in [('JSStrictEqModeEnum','number.c'),('JSCFunctionDataRecord','object.c')]:
        path='src/quickjs/'+owner
        pat=r'(?ms)^typedef (?:struct|enum) '+name+r' \{.*?^} '+name+r';'
        match=re.search(pat,newcs[path]);assert match,name
        shared.append(match.group())
        newcs[path]=newcs[path][:match.start()]+newcs[path][match.end():]
    # Restore the shared upstream Array find modes (typed arrays use them too).
    shared.append(re.search(r'enum \{\n    ArrayFind,.*?\n\};',usrc,re.S).group())
    # The parser only needs an incomplete type across the module boundary.
    shared.append('typedef struct JSParseState JSParseState;')
    key='src/quickjs/internal-types.h'
    at=newhs[key].rfind('#endif')
    newhs[key]=newhs[key][:at]+'\n'+'\n\n'.join(shared)+'\n\n'+newhs[key][at:]
    for path,s in newhs.items():newhs[path]=clean_header(normalize_prototypes(s,uf,header=True))
    # Original shared constants; remove fork aliases and TU-local duplicates.
    macro_pat=r'(?m)^#define ((?:(?:HINT_|ATOD_|GEN_MAGIC_|DEFINE_GLOBAL_|JS_BACKTRACE_FLAG_|special_|JS_CALL_FLAG_|FUNC_RET_)\w+|MAX_SAFE_INTEGER|JS_ThrowTypeErrorAtom|JS_ThrowSyntaxErrorAtom))[^\n]*'
    shared_macros=re.findall(macro_pat,usrc)
    macro_lines=re.findall(r'(?m)^#define (?:(?:HINT_|ATOD_|GEN_MAGIC_|DEFINE_GLOBAL_|JS_BACKTRACE_FLAG_|special_|JS_CALL_FLAG_|FUNC_RET_)\w+|MAX_SAFE_INTEGER|JS_ThrowTypeErrorAtom|JS_ThrowSyntaxErrorAtom)[^\n]*',usrc)
    for mapping in [newcs,newhs]:
        for path,s in list(mapping.items()):
            mapping[path]=re.sub(macro_pat+r'(?:\n(?<=[\\]\n)[^\n]*)*\n?', '',s)
    key='src/quickjs/internal-config.h'
    at=newhs[key].rfind('#endif')
    newhs[key]=newhs[key][:at]+'\n'+'\n'.join(macro_lines)+'\n\n'+newhs[key][at:]
    # Keep the original width-dependent bounds together in a private header.
    bounds=re.search(r'#if JS_SHORT_BIG_INT_BITS == 32\n.*?#endif',usrc,re.S).group()
    path='src/quickjs/number.c'
    assert bounds in newcs[path]
    newcs[path]=newcs[path].replace(bounds,'',1)
    key='src/quickjs/internal-number.h'
    at=newhs[key].rfind('#endif')
    newhs[key]=newhs[key][:at]+'\n'+bounds+'\n\n'+newhs[key][at:]
    newcs, movements = split_sources(newcs, license)
    # Identify source-level cross-TU references, including callback tables and
    # inactive diagnostic paths. A broad temporary interface is intentional.
    uses=collections.defaultdict(set)
    for path,s in newcs.items():
        for token in set(re.findall(r'\b[A-Za-z_]\w*\b',mask_c(s))):
            if token in uf:uses[token].add(path)
    exposed=set()
    for s in newhs.values():
        exposed.update(proto_names(s))
        # Canonical function references inside shared macros also need linkage.
        macros='\n'.join(re.findall(r'(?m)^[ \t]*#define[^\n]*(?:\n(?<=[\\]\n)[^\n]*)*',s))
        exposed.update(set(re.findall(r'\b[A-Za-z_]\w*\b',macros)) & set(uf))
    exposed|={n for n,paths in uses.items() if len(paths)>1}
    declarations=[]
    for name in sorted(exposed):
        if name not in uf: continue
        f=uf[name][0]
        if re.search(r'\bstatic\b',f['sig']):
            declarations.append(signature(f,True)+';')
    for path,s in newcs.items():
        edits=[]
        for f in functions(s):
            if f['name'] in exposed and re.search(r'\bstatic\b',f['sig']):
                edits.append((f['start'],f['start']+len(f['sig']),signature(f,True)))
        s=replace_ranges(s,edits)
        s=normalize_prototypes(s,uf,exposed=exposed)
        # Every TU sees the same declaration-only temporary private interface.
        inc=re.search(r'(?m)^#include',s)
        at=inc.start() if inc else len(license)
        s=s[:at]+'#include "internal-canonical.h"\n'+s[at:]
        newcs[path]=s
    data_decls=[]
    for path,s in list(newcs.items()):
        edits=[]
        for d in data_definitions(s):
            name=d['name']
            consumers=[p for p,t in newcs.items() if p!=path and re.search(r'\b'+name+r'\b',mask_c(t))]
            if not consumers:continue
            array=d['array'] or ''
            if array.replace(' ','')=='[]':array='['+str(d['count'])+']'
            data_decls.append('extern QJS_INTERNAL '+d['type']+' '+name+array+';')
            edits.append((d['start'],d['end'],re.sub(r'^static\s+','QJS_INTERNAL ',d['text'])))
        newcs[path]=replace_ranges(s,edits)
    # Include all private type/declaration headers before the additional declarations.
    common=license+'#ifndef QUICKJS_INTERNAL_CANONICAL_H\n#define QUICKJS_INTERNAL_CANONICAL_H\n\n'
    common+='/* Temporary broad cross-TU interface for the correctness-first split.\n'
    common+='   No function bodies or performance remaps belong in this header. */\n'
    for path in sorted(newhs):common+=f'#include "{Path(path).name}"\n'
    common+='\n'+'\n'.join(data_decls)+'\n\n'+'\n'.join(declarations)+'\n\n#endif\n'
    newhs['src/quickjs/internal-canonical.h']=common
    for path,s in {**newcs,**newhs}.items():
        s=re.sub(r'(?m)^#define QUICKJS_\w+_OWNER\n', '',s)
        (ROOT/path).write_text(clean_gaps(s))
    (ROOT/'quickjs.h').write_text(upstream('quickjs.h'))
    makefile=git_read(args.base_ref,'Makefile')
    for old,extra in [('object',['shape','property']),('number',['bigint']),('function-vm',['function','iterator'])]:
        needle='$(OBJDIR)/src/quickjs/'+old+'.o'
        assert needle in makefile
        makefile=makefile.replace(needle,needle+' '+ ' '.join('$(OBJDIR)/src/quickjs/'+x+'.o' for x in extra),1)
    (ROOT/'Makefile').write_text(makefile)
    print('Removed split helpers:',len(set(removed)))
    print('Cross-TU declarations:',len(declarations))
    (ROOT/'split05-inventory.json').write_text(json.dumps({'upstream':UPSTREAM,'base':BASE,'removed':sorted(set(removed)),'exposed':sorted(exposed),'movements':movements},indent=2)+'\n')

if __name__=='__main__':main()
