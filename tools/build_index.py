#!/usr/bin/env python3
"""Write index.json: every plugin the Tessera app can offer, with its manifest, texts and README at a fixed commit.

    python3 tools/build_index.py            # writes index.json
    python3 tools/build_index.py --check    # fails when index.json is not what it would write

Two kinds of entry:
- plugins/<id>/ in this repository. Each is pinned to the last commit that changed its folder, so a change elsewhere
  in the repository never makes a screen build something new.
- community/<id>.yaml: a plugin in its own repository (repo, path, ref). Closed until Tessera's own plugins run on plugin
  API 1 (docs/PUBLISHING.md); the format is ready.

The app reads index.json from raw.githubusercontent.com (main), at most every six hours, and runs nothing from it: a
manifest is a description (docs/MANIFEST.md). CI runs this script after every merge (.github/workflows/index.yml).
"""
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
REPO = 'https://github.com/MaxGramser/tessera-plugins'
sys.path.insert(0, str(ROOT / 'tools'))
from check import check_folder, plugin_api  # noqa: E402


def git(*args):
    return subprocess.run(['git', '-C', str(ROOT), *args], check=True, capture_output=True, text=True).stdout.strip()


def own_plugin(folder):
    manifest, translations, readme = check_folder(folder)
    sha = git('log', '-1', '--format=%H', '--', str(folder.relative_to(ROOT)))
    date = git('log', '-1', '--format=%cs', '--', str(folder.relative_to(ROOT)))
    if not sha:
        raise SystemExit(f'{folder.name}: not committed yet; commit it, then build the index')
    return {
        'id': manifest['id'], 'repo': REPO, 'path': str(folder.relative_to(ROOT)), 'label': 'tessera', 'status': 'ok',
        'release': {'version': manifest['version'], 'sha': sha, 'date': date},
        'manifest': manifest, 'translations': translations, 'readme': readme,
    }


def build():
    plugins = [own_plugin(folder) for folder in sorted((ROOT / 'plugins').iterdir())
               if (folder / 'tessera-plugin.yaml').is_file()]
    blocked = yaml.safe_load((ROOT / 'blocked.yaml').read_text()) or []
    return {'format': 1, 'plugin_api': plugin_api(), 'blocked': blocked, 'plugins': plugins}


def main():
    index = build()
    path = ROOT / 'index.json'
    old = json.loads(path.read_text()) if path.is_file() else {}
    same = {k: v for k, v in old.items() if k != 'generated'} == index
    if '--check' in sys.argv:
        if not same:
            raise SystemExit('index.json is out of date: run python3 tools/build_index.py')
        print('index.json is up to date')
        return
    if same:
        print('index.json unchanged')
        return
    index = {'format': 1, 'generated': datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'), **index}
    path.write_text(json.dumps(index, indent=1, ensure_ascii=False) + '\n')
    print(f'index.json: {len(index["plugins"])} plugins')


if __name__ == '__main__':
    main()
