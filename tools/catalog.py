#!/usr/bin/env python3
"""RTTI seeds and the function catalog for tp.exe, with pcrecomp's own tools.

Runs pcrecomp's cpp/rtti.py (vtable slots named by RTTI become seeds: a method
only ever called virtually is named by no `call`) and then disasm/disasm32.py.
About three minutes; skipped when the catalog exists (--force redoes it).

    py -3 tools/catalog.py
Output: work/tp.rtti.json, work/tp.seeds.json, work/tp.functions.json, log in work/tp.catalog.log
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(os.environ.get('PCRECOMP', os.path.join(ROOT, '..', 'pcrecomp')), 'tools')
WORK = os.path.join(ROOT, 'work')


def main():
    img = os.path.join(ROOT, 'game', 'tp.exe')
    cat = os.path.join(WORK, 'tp.functions.json')
    if os.path.exists(cat) and '--force' not in sys.argv:
        print('catalog already there (--force redoes it)')
        return 0
    os.makedirs(WORK, exist_ok=True)
    seeds = os.path.join(WORK, 'tp.seeds.json')
    with open(os.path.join(WORK, 'tp.catalog.log'), 'w') as log:
        for cmd in ([os.path.join(TOOLS, 'cpp', 'rtti.py'), img, '-o', os.path.join(WORK, 'tp.rtti.json'), '--seeds', seeds],
                    [os.path.join(TOOLS, 'disasm', 'disasm32.py'), img, '-o', cat, '--seed-functions', seeds]):
            if subprocess.run([sys.executable] + cmd, stdout=log, stderr=subprocess.STDOUT).returncode:
                print('%s failed, see work/tp.catalog.log' % os.path.basename(cmd[0]))
                return 1
    for line in open(os.path.join(WORK, 'tp.catalog.log')):
        if 'Functions:' in line or 'coverage' in line:
            print(line.strip())
    return 0


if __name__ == '__main__':
    sys.exit(main())
