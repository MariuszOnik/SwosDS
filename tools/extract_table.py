#!/usr/bin/env python3
"""Mechanically extract a C# numeric array initializer and re-emit it as a C
array. Used for OpenSWOS SwosVm data tables (Rng.kRandomTable,
Tables.kSineCosineTable, Tables.kAngleTangent) -- these are literal byte-for-
byte constants copied from the original SWOS binary, so we extract the
initializer text mechanically instead of retyping ~1300 numbers by hand
(transcription of a single digit would silently desync RNG/trig output from
the original).

C#'s `{ ... }` array-initializer syntax (including nested `{ {..}, {..} }`
for 2D arrays) is token-for-token valid as a C initializer, so this is a
straight text extraction + brace-balanced slice, not a real translation.

Usage:
    python extract_table.py <source.cs> <VarName> <c_type> <c_name> >> out.h
"""
import re
import sys


def extract(path, var_name):
    with open(path, encoding='utf-8') as f:
        text = f.read()
    # Find "<var_name> = ... {" then balance braces from there.
    m = re.search(re.escape(var_name) + r'\s*=[^{]*\{', text)
    if not m:
        raise SystemExit(f'{path}: could not find initializer for {var_name}')
    start = m.end() - 1  # position of the opening '{'
    depth = 0
    i = start
    while i < len(text):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                break
        i += 1
    else:
        raise SystemExit(f'{path}: unbalanced braces for {var_name}')
    body = text[start:i + 1]
    # Strip // line comments (none expected inside these particular literals,
    # but be defensive).
    body = re.sub(r'//[^\n]*', '', body)
    return body


if __name__ == '__main__':
    if len(sys.argv) != 5:
        print(__doc__)
        sys.exit(1)
    src, var_name, c_type, c_name = sys.argv[1:5]
    body = extract(src, var_name)
    print(f'// Mechanically extracted from {src} :: {var_name} (see tools/extract_table.py)')
    print(f'static const {c_type} {c_name} = {body};')
