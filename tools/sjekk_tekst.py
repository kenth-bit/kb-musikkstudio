#!/usr/bin/env python3
"""Finner tekstbokstaver med norske tegn (æøå) som mangler _nb.

JUCE tolker "..." som ASCII. Står det æ, ø eller å i teksten uten _nb, blir tegnene
feil (og debug-bygg stopper med en assert). Kjør:  python3 tools/sjekk_tekst.py
Avslutter med kode 1 hvis noe er feil, så den kan brukes som test."""
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent.parent
literal = re.compile(r'"((?:[^"\\\n]|\\.)*)"(_nb)?')
errors = []


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text, flags=re.S)
    out = []
    for line in text.split('\n'):
        pos = 0
        cut = len(line)
        in_string = False
        while pos < len(line):
            c = line[pos]
            if c == '\\' and in_string:
                pos += 2
                continue
            if c == '"':
                in_string = not in_string
            elif not in_string and line.startswith('//', pos):
                cut = pos
                break
            pos += 1
        out.append(line[:cut])
    return '\n'.join(out)


for folder in ('src', 'tools', 'tests'):
    for path in sorted((root / folder).rglob('*')):
        if path.suffix not in ('.cpp', '.h') or path.name == 'Text.h':
            continue
        code = strip_comments(path.read_text(encoding='utf-8'))
        for number, line in enumerate(code.split('\n'), 1):
            if 'static_assert' in line or 'printf' in line or 'std::cout' in line or 'std::cerr' in line \
                    or 'fromUTF8' in line or 'return fail (' in line or 'return "' in line:
                continue
            for match in literal.finditer(line):
                if re.search(r'[^\x00-\x7f]', match.group(1)) and not match.group(2):
                    errors.append(f'{path.relative_to(root)}:{number}: mangler _nb: "{match.group(1)[:50]}"')

for e in errors:
    print(e)
print(f'{len(errors)} feil' if errors else 'Alle tekster med norske tegn bruker _nb.')
sys.exit(1 if errors else 0)
