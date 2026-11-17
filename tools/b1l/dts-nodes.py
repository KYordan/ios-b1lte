#!/usr/bin/env python3
"""Print DTS nodes whose header line matches a regex, with full bodies.
Usage: dts-nodes.py FILE REGEX  (phandle numbers are normalised to <ph>)."""
import re, sys
text = open(sys.argv[1]).read().splitlines()
pat = re.compile(sys.argv[2])
i = 0
while i < len(text):
    line = text[i]
    if line.rstrip().endswith('{') and pat.search(line):
        depth, j = 0, i
        while j < len(text):
            depth += text[j].count('{') - text[j].count('}')
            print(re.sub(r'<0x[0-9a-f]+>', lambda m: m.group(0) if len(m.group(0)) > 8 else '<ph>', text[j]))
            j += 1
            if depth == 0:
                break
        print('----')
        i = j
    else:
        i += 1
