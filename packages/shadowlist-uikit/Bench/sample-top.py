#!/usr/bin/env python3
"""Inclusive main thread samples per function from a macOS `sample` report.

  ./sample-top.py report.txt [limit] [filter]

Counts each function once per stack. Recursion is not double counted. Idle time in the
run loop's mach_msg wait is reported separately.
"""
import re
import sys
from collections import defaultdict

LINE = re.compile(r'^(?P<indent>[ +!:|]*)(?P<count>\d+) (?P<name>.+?)  \(in (?P<image>[^)]+)\)')


def main():
    path = sys.argv[1]
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    pattern = re.compile(sys.argv[3]) if len(sys.argv) > 3 else None
    lines = open(path).read().splitlines()
    start = next(i for i, l in enumerate(lines) if 'Main Thread' in l)
    end = next(i for i in range(start + 1, len(lines)) if re.match(r'^    \d+ Thread_', lines[i]))
    total = int(re.search(r'(\d+) Thread_', lines[start]).group(1))
    inclusive = defaultdict(int)
    stack = []  # (depth, name)
    idle = 0
    for line in lines[start + 1:end]:
        m = LINE.match(line)
        if not m:
            continue
        depth = len(m.group('indent'))
        count = int(m.group('count'))
        name = re.sub(r' \+ \d+.*$', '', m.group('name'))
        while stack and stack[-1][0] >= depth:
            stack.pop()
        ancestors = {n for _, n in stack}
        if name not in ancestors:
            inclusive[name] += count
        if name.startswith('mach_msg2_trap'):
            idle += count
        stack.append((depth, name))
    busy = total - idle
    print('main thread samples %d, idle %d, busy %d (%.0f%%)' % (total, idle, busy, 100.0 * busy / total))
    rows = sorted(inclusive.items(), key=lambda kv: -kv[1])
    shown = 0
    for name, count in rows:
        if pattern and not pattern.search(name):
            continue
        if count > total * 0.9:
            continue
        print('%6d %5.1f%% of busy  %s' % (count, 100.0 * count / max(1, busy), name[:150]))
        shown += 1
        if shown >= limit:
            break


if __name__ == '__main__':
    main()
