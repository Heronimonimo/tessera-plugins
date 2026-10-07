#!/usr/bin/env python3
"""Write index.json: every plugin the Tessera app can offer, with its manifest, texts and README at a fixed commit.

    python3 tools/build_index.py            # writes index.json
    python3 tools/build_index.py --check    # fails when index.json is not what it would write
    python3 tools/build_index.py --entries [community/<id>.yaml ...]   # checks community entries at their release

Two kinds of entry:
- plugins/<id>/ in this repository. Each is pinned to the last commit that changed its folder, so a change elsewhere
  in the repository never makes a screen build something new.
- community/<id>.yaml: a plugin in its own repository (repo, path, maintainer, and a release tag, or none to follow the
  newest release). It is cloned at that release, checked with the same rules as Tessera's own (tools/check.py), and
  pinned to the commit. One that fails is left out of the index with the reason on stderr; the others still go in, and
  a release that fails leaves the plugin out until a release passes.

The app reads index.json from raw.githubusercontent.com (main), at most every ten minutes, and runs nothing from it: a
manifest is a description (docs/MANIFEST.md). CI runs this script after every merge and every hour, so a maker's new
release reaches the app within the hour without a pull request (.github/workflows/index.yml).
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
REPO = 'https://github.com/MaxGramser/tessera-plugins'
sys.path.insert(0, str(ROOT / 'tools'))
from check import check_entry, check_folder, plugin_api  # noqa: E402


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


def latest_release(owner, repo):
    """The tag of a repository's newest release, through GitHub's API (GITHUB_TOKEN when CI has one)."""
    request = urllib.request.Request(f'https://api.github.com/repos/{owner}/{repo}/releases/latest',
                                     headers={'Accept': 'application/vnd.github+json', 'User-Agent': 'tessera-plugins',
                                              **({'Authorization': f'Bearer {os.environ["GITHUB_TOKEN"]}'}
                                                 if os.environ.get('GITHUB_TOKEN') else {})})
    with urllib.request.urlopen(request, timeout=20) as response:
        return json.load(response)['tag_name']


def community_plugin(path):
    """An entry of community/: the plugin at its release, checked, pinned to the commit; None (and why on stderr) when
    it cannot go in."""
    try:
        entry = check_entry(path)
        owner, repo = re.match(r'^https://github\.com/([^/]+)/([^/]+)$', entry['repo']).groups()
        tag = entry.get('ref') or latest_release(owner, repo)
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run(['git', 'clone', '--quiet', '--depth', '1', '--branch', tag, entry['repo'], tmp], check=True,
                           capture_output=True, text=True, timeout=120)
            sha = subprocess.run(['git', '-C', tmp, 'rev-parse', 'HEAD'], check=True, capture_output=True, text=True).stdout.strip()
            date = subprocess.run(['git', '-C', tmp, 'log', '-1', '--format=%cs'], check=True, capture_output=True,
                                  text=True).stdout.strip()
            folder = Path(tmp) / entry.get('path', '.')
            manifest, translations, readme = check_folder(folder)
    except (SystemExit, subprocess.SubprocessError, OSError, ValueError, KeyError) as error:
        print(f'{path.stem}: left out ({error})', file=sys.stderr)
        return None
    if manifest['id'] != path.stem:
        print(f'{path.stem}: left out (its manifest says id {manifest["id"]})', file=sys.stderr)
        return None
    return {
        'id': manifest['id'], 'repo': entry['repo'], 'path': entry.get('path', '.'), 'label': 'community', 'status': 'ok',
        'release': {'version': manifest['version'], 'sha': sha, 'date': date, 'tag': tag},
        'manifest': manifest, 'translations': translations, 'readme': readme,
    }


def build():
    plugins = [own_plugin(folder) for folder in sorted((ROOT / 'plugins').iterdir())
               if (folder / 'tessera-plugin.yaml').is_file()]
    own = {plugin['id'] for plugin in plugins}
    for path in sorted((ROOT / 'community').glob('*.yaml')):
        found = community_plugin(path)
        if found and found['id'] not in own:
            plugins.append(found)
    blocked = yaml.safe_load((ROOT / 'blocked.yaml').read_text()) or []
    return {'format': 1, 'plugin_api': plugin_api(), 'blocked': blocked, 'plugins': plugins}


def entries(paths):
    """--entries: every community entry named (all without names) passes at its release, else SystemExit."""
    paths = [Path(p) for p in paths] or sorted((ROOT / 'community').glob('*.yaml'))
    failed = [path.stem for path in paths if community_plugin(path) is None]
    if failed:
        raise SystemExit(f'not listed: {", ".join(failed)} (the reasons are above)')
    print(f'{len(paths)} community entries pass')


def main():
    if '--entries' in sys.argv:
        entries([arg for arg in sys.argv[1:] if arg != '--entries'])
        return
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
