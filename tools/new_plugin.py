#!/usr/bin/env python3
"""Start a plugin from the template.

    python3 tools/new_plugin.py bin_day                 # plugins/bin_day/, a plugin of this repository
    python3 tools/new_plugin.py bin_day ../my-plugin    # a folder of its own, for a repository of your own

It copies template/ and renames everything that carries the template's name: the component folder and files, the
C++ namespace and class (my_plugin -> bin_day, MyPlugin -> BinDay) and the manifest's id. The tile still counts days:
change it into yours (docs/MAKING_A_PLUGIN.md, "Step by step"), then run python3 tools/check.py <folder>.
"""
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    if len(sys.argv) < 2 or not re.fullmatch(r'[a-z][a-z0-9_]{0,31}', sys.argv[1]):
        raise SystemExit('usage: new_plugin.py <id> [folder]; an id is 1 to 32 of a-z, 0-9 and _, starting with a letter')
    plugin = sys.argv[1]
    target = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / 'plugins' / plugin
    if target.exists():
        raise SystemExit(f'{target} already exists')
    camel = ''.join(part.capitalize() for part in plugin.split('_'))
    shutil.copytree(ROOT / 'template', target)
    (target / 'components' / 'my_plugin').rename(target / 'components' / plugin)
    for path in sorted(target.rglob('*')):
        if path.is_file() and path.name.startswith('my_plugin'):
            path = path.rename(path.with_name(path.name.replace('my_plugin', plugin)))
        if path.is_file() and path.suffix in ('.py', '.h', '.cpp', '.yaml', '.md', '.json'):
            text = path.read_text(encoding='utf-8')
            text = text.replace('my_plugin', plugin).replace('MyPlugin', camel)
            path.write_text(text, encoding='utf-8')
    print(f'{target}: made from the template. Next: edit tessera-plugin.yaml, translations/ and components/{plugin}/, '
          f'then python3 tools/check.py {target}')


if __name__ == '__main__':
    main()
