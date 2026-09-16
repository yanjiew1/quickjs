import re

# QuickJS definitions use braces at column zero. Do not parse preprocessed C:
# retain definitions in inactive #if branches as well.
PAT = re.compile(r'(?m)^([A-Za-z_][^;{}#=]*?)(?:\n| )\{')

def mask_c(src):
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*', re.S)
    def blank(m): return ''.join('\n' if c == '\n' else ' ' for c in m.group())
    text = tokens.sub(blank, src)
    return re.sub(r'(?m)^[ \t]*#[^\n]*(?:\\\n[^\n]*)*', blank, text)

def functions(src):
    out=[]
    masked=mask_c(src)
    for m in PAT.finditer(src):
        sig=m.group(1)
        if '/*' in sig or '*/' in sig or '//' in sig:
            # There are no comments inside upstream function signatures.
            continue
        sig_for_name=re.sub(r'\(([A-Za-z_]\w*)\)(?=\s*\()', r'\1', sig)
        clean=re.sub(r'__attribute__\s*\(\(.*?\)\)', '', sig_for_name, flags=re.S)
        names=re.findall(r'(?:^|[\s*])(?:\(\s*)?([A-Za-z_]\w*)(?:\s*\))?\s*\(',clean)
        if not names: continue
        name=names[0]
        if masked[m.end()-1] != '{': continue
        depth=1; endpos=None
        for brace in re.finditer(r'[{}]',masked[m.end():]):
            depth += 1 if brace.group() == '{' else -1
            if depth == 0:
                endpos=m.end()+brace.end(); break
        if endpos is None: raise ValueError(name)
        # Structs have { on the same line; this guard also excludes block macros.
        if name in ['if','switch','for','while']: continue
        out.append(dict(name=name,start=m.start(),end=endpos,body_start=m.end()-1,
                        sig=sig,text=src[m.start():endpos],
                        line=src.count('\n',0,m.start())+1))
    return out


def data_definitions(src):
    """Top-level brace-initialized data, including inactive preprocessor branches."""
    masked=mask_c(src)
    pat=re.compile(r'(?m)^(?:static|QJS_INTERNAL)\s+([^;={}]+?)\b(\w+)\s*(\[[^\]]*\])?\s*=\s*\{')
    out=[]
    for m in pat.finditer(src):
        if masked[m.end()-1]!='{': continue
        if '(' in m.group(1): continue
        depth=1; end=None
        for b in re.finditer(r'[{}]',masked[m.end():]):
            depth+=1 if b.group()=='{' else -1
            if not depth:end=m.end()+b.end();break
        assert end is not None
        tail=re.match(r'\s*;',src[end:]);assert tail is not None,m.group(2)
        end+=tail.end()
        body=masked[m.end():end-2]
        levels=0;count=0;has=False
        for c in body:
            if c in '([{':levels+=1;has=True
            elif c in ')]}':levels-=1
            elif c==',' and levels==0:count+=1;has=False
            elif not c.isspace():has=True
        if has:count+=1
        out.append(dict(name=m.group(2),type=m.group(1).strip(),array=m.group(3),
                        count=count,start=m.start(),end=end,text=src[m.start():end],body_start=m.end()-1))
    return out
