#!/usr/bin/env python3
"""ARBS Studio - i ben burimet C++ vetem ASCII (per MSVC ne cdo code page, /WX).
- ne stringje: karakteret jo-ASCII -> sekuenca \\xNN (UTF-8)
- ne komente: transliterim (e, c, -) ose '?'
Perdorimi: python Tools-ARBS/ascii_sources.py Src
"""
import pathlib, sys

TRANSLIT = {'ë': 'e', 'Ë': 'E', 'ç': 'c', 'Ç': 'C', '–': '-', '—': '-', '·': '.', '…': '...', 'é': 'e', '’': "'", '“': '"', '”': '"', '→': '->', 'λ': 'lambda', 'μ': 'mu', 'ρ': 'rho', '²': '2', '°': 'deg', '×': 'x', 'θ': 'theta', '≤': '<=', '≥': '>='}
HEX = set('0123456789abcdefABCDEF')

def escape_literal_body(body):
    out = []
    i = 0
    while i < len(body):
        ch = body[i]
        if ord(ch) < 128:
            out.append(ch); i += 1; continue
        esc = ''.join('\\x%02X' % b for b in ch.encode('utf-8'))
        out.append(esc)
        nxt = body[i + 1] if i + 1 < len(body) else ''
        if nxt in HEX:
            out.append('" "')  # nderpret sekuencen hex
        i += 1
    return ''.join(out)

def convert(text):
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        # komente
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(''.join(TRANSLIT.get(ch, ch if ord(ch) < 128 else '?') for ch in text[i:j])); i = j; continue
        if text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join(TRANSLIT.get(ch, ch if ord(ch) < 128 else '?') for ch in text[i:j])); i = j; continue
        # raw string R"delim( ... )delim"
        if c == 'R' and i + 1 < n and text[i + 1] == '"' and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_')):
            k = text.find('(', i + 2)
            delim = text[i + 2:k]
            end = text.find(')' + delim + '"', k)
            seg = text[i:end + len(delim) + 2]
            if any(ord(ch) > 127 for ch in seg):
                raise SystemExit('Raw string me karaktere jo-ASCII: ' + seg[:40])
            out.append(seg); i = end + len(delim) + 2; continue
        if c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            body = text[i + 1:j]
            out.append(c + escape_literal_body(body) + c); i = j + 1; continue
        out.append(c); i += 1
    return ''.join(out)

def main(root):
    changed = 0
    for p in pathlib.Path(root).rglob('*'):
        if p.suffix not in ('.cpp', '.hpp', '.h', '.c'):
            continue
        t = p.read_text(encoding='utf-8')
        if all(ord(ch) < 128 for ch in t):
            continue
        new = convert(t)
        assert all(ord(ch) < 128 for ch in new), p
        p.write_text(new, encoding='utf-8')
        changed += 1
        print('ASCII:', p)
    print('U konvertuan', changed, 'skedare')

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'Src')
