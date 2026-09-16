#!/usr/bin/env python3
"""Mechanically extract EVERY `private/public static readonly <type>[] NAME =
{ ... };` array declaration from a C# file and re-emit each as a C array.
Generalizes extract_table.py (which targets one named array) to "every array
in the file" -- used for AnimationTablesData.cs's ~60 frame-indices streams,
where hand-retyping each one risks a silently-wrong animation frame index.

Usage:
    python extract_all_arrays.py <source.cs> <c_type> > out.h

Only handles simple non-generic `<type>[] name = { ... };` declarations
(with or without a `new <type>[]` prefix before the `{`). Skips anything it
doesn't recognize rather than guessing -- run with --list first to see what
would be extracted.
"""
import re
import sys

DECL_RE = re.compile(
    r'^\s*(?:private|public|internal)\s+static\s+readonly\s+(?P<cstype>\w+)\[\]\s+(?P<name>\w+)\s*=',
    re.MULTILINE,
)


def find_all(path, only_cstype=None):
    with open(path, encoding='utf-8') as f:
        text = f.read()
    results = []
    for m in DECL_RE.finditer(text):
        cstype = m.group('cstype')
        if only_cstype and cstype != only_cstype:
            continue
        name = m.group('name')
        # Find the '{' that starts the initializer (skip an optional `new T[]`).
        brace_start = text.index('{', m.end())
        depth = 0
        i = brace_start
        while i < len(text):
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        body = text[brace_start:i + 1]
        body = re.sub(r'//[^\n]*', '', body)
        results.append((name, body))
    return results


if __name__ == '__main__':
    if len(sys.argv) not in (3, 4) or (len(sys.argv) == 4 and sys.argv[3] != '--list'):
        print(__doc__)
        sys.exit(1)
    src, c_type = sys.argv[1], sys.argv[2]
    list_only = len(sys.argv) == 4
    # c_type doubles as the C# element type filter (e.g. "short" only matches
    # `short[]` declarations, skipping `int[]` ones which hold computed
    # addresses, not literal data).
    arrays = find_all(src, only_cstype=c_type)
    if list_only:
        for name, body in arrays:
            print(name)
        sys.exit(0)
    # C# element type -> emitted C type. Was hardcoded to int16_t (fine while
    # every caller passed "short"); TacticsLoader.cs's byte[] tables need
    # uint8_t, so this is now an explicit map instead of a silent guess.
    c_emit_type = {'short': 'int16_t', 'byte': 'uint8_t', 'int': 'int32_t', 'uint': 'uint32_t'}.get(c_type)
    if c_emit_type is None:
        print(f'error: no emitted-C-type mapping for C# type "{c_type}" -- add one to extract_all_arrays.py', file=sys.stderr)
        sys.exit(1)
    print(f'// Mechanically extracted from {src} (see tools/extract_all_arrays.py) -- DO NOT EDIT BY HAND.')
    for name, body in arrays:
        print(f'static const {c_emit_type} {name}[] = {body};')
