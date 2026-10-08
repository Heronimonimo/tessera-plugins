#!/usr/bin/env python3
"""Check plugins the way the Tessera app reads them, before a push or a pull request.

    python3 tools/check.py                      # every folder in plugins/ and template/
    python3 tools/check.py plugins/my_plugin    # one plugin (any folder with a tessera-plugin.yaml)

It runs the app's own manifest check: plugin_manifest.py from the Tessera repository (homeassistant_espscreen,
screen_manager/app/plugin_manifest.py), fetched into tools/.cache/ and fetched again when it is an hour old (offline,
the copy there serves). The same file decides in the app whether a
plugin is shown, so a plugin that passes here is read the same way there. Then the rules the manifest check cannot see:
the folder layout, the translations (English complete), the README, the licence, and the drawing rules for the
firmware code (docs/FIRMWARE_API.md, "Rules").

TESSERA_MANIFEST=/path/to/plugin_manifest.py uses a local copy instead (a checkout of the Tessera repository).
"""
import importlib.util
import json
import os
import re
import sys
import time
import urllib.request
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
SOURCE = 'https://raw.githubusercontent.com/MaxGramser/homeassistant_espscreen/{ref}/screen_manager/app/plugin_manifest.py'
CACHE = ROOT / 'tools' / '.cache' / 'plugin_manifest.py'

# Code rules for a plugin's C++ (docs/FIRMWARE_API.md): what the core forbids itself, so a plugin draws like the rest.
CODE_RULES = [
    (re.compile(r'lv_color_hex\s*\(|0x[0-9a-fA-F]{6}\b'), 'a colour as a number: use a theme role (tessera::ui::color)'),
    (re.compile(r'heap_caps_get_largest_free_block|heap_caps_get_info'), 'a walk of the heap: it shifts the picture of an RGB panel'),
    (re.compile(r'LV_EVENT_CLICKED|LV_EVENT_PRESSED'), 'an LVGL tap of its own: use Tile::on_tap, which has the touch filter in front'),
    (re.compile(r'lv_timer_create|xTaskCreate|delay\s*\(\s*\d'), 'a timer, a task or a wait of its own: use on_tick'),
    (re.compile(r'lv_font_t\s+\w+\s*=|font:\s*$|LV_FONT_DECLARE'), 'a font of its own: use the screen\'s fonts (tessera::Font)'),
    (re.compile(r'http_request|HTTPClient|esp_http_client|WiFiClient'), 'a connection from the screen: data comes through the app (fetch)'),
]


def manifest_module():
    local = os.environ.get('TESSERA_MANIFEST')
    if local:
        path = Path(local)
    else:
        path = CACHE
        if not path.is_file() or time.time() - path.stat().st_mtime > 3600:
            path.parent.mkdir(parents=True, exist_ok=True)
            ref = os.environ.get('TESSERA_REF', 'dev')
            try:
                with urllib.request.urlopen(SOURCE.format(ref=ref), timeout=20) as response:
                    path.write_bytes(response.read())
            except OSError:
                if not path.is_file():
                    raise
    spec = importlib.util.spec_from_file_location('plugin_manifest', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PM = None


def pm():
    global PM
    if PM is None:
        PM = manifest_module()
    return PM


def plugin_api():
    return '.'.join(map(str, pm().PLUGIN_API))


# Where the docs name the API the core offers now. The phrase "plugin API <n>" (lowercase plugin) always means the
# current one; a part's own version is written "API 0.2" or "(0.2)". So the docs cannot fall behind the core unnoticed.
DOC_FILES = ('README.md', 'AGENTS.md', 'llms.txt', 'docs/FIRMWARE_API.md', 'docs/MANIFEST.md', 'docs/MAKING_A_PLUGIN.md')
CURRENT_API = re.compile(r'plugin API (?:is )?(\d+\.\d+)')


def check_docs():
    """Every doc that names the plugin API the core offers names the one the manifest check knows."""
    wanted = plugin_api()
    for name in DOC_FILES:
        text = (ROOT / name).read_text(encoding='utf-8')
        found = CURRENT_API.findall(text)
        if not found:
            fail(name, f'names no plugin API; it should say "plugin API {wanted}" where it means the current one')
        stale = sorted(set(found) - {wanted})
        if stale:
            fail(name, f'says plugin API {", ".join(stale)}; the core offers {wanted}')


def fail(folder, message):
    raise SystemExit(f'{folder}: {message}')


def check_folder(folder):
    """(raw manifest, translations, readmes) of a plugin folder, or SystemExit with what is wrong."""
    folder = Path(folder)
    name = folder.name
    try:
        raw = yaml.safe_load((folder / 'tessera-plugin.yaml').read_text(encoding='utf-8'))
    except (OSError, yaml.YAMLError) as error:
        fail(name, f'tessera-plugin.yaml: {error}')
    translations = {}
    for path in sorted((folder / 'translations').glob('*.json')):
        try:
            translations[path.stem] = json.loads(path.read_text(encoding='utf-8'))
        except ValueError as error:
            fail(name, f'translations/{path.name}: {error}')
    if 'en' not in translations:
        fail(name, 'translations/en.json is required')
    try:
        manifest = pm().check(raw, translations['en'])
    except pm().ManifestError as error:
        fail(name, f'tessera-plugin.yaml: {error}')
    if manifest['id'] != name and folder.parent.name == 'plugins':
        fail(name, f'the folder must be called {manifest["id"]}, as the plugin\'s id')
    english = set(translations['en'].get('screen') or {})
    for lang, data in translations.items():
        extra = set(data.get('screen') or {}) - english
        if extra:
            fail(name, f'translations/{lang}.json: screen texts English does not have: {", ".join(sorted(extra))}')
    if not (folder / 'plugin.yaml').is_file():
        fail(name, 'plugin.yaml is required (what a screen gets)')
    components = folder / 'components'
    if not components.is_dir() or not any(components.iterdir()):
        fail(name, 'components/<name>/ is required (the ESPHome component)')
    readme = {}
    for path in sorted(folder.glob('README*.md')):
        language = path.stem.split('.', 1)[1] if '.' in path.stem else 'en'
        readme[language] = path.read_text(encoding='utf-8')
    if 'en' not in readme:
        fail(name, 'README.md is required')
    if not re.search(r'(?m)^##\s+(Set ?up|Setup)\b', readme['en']):
        fail(name, 'README.md needs a "## Set up" section')
    if folder.parent.name == 'plugins' and not (folder / 'LICENSE').is_file() and not (folder.parent.parent / 'LICENSE').is_file():
        fail(name, 'a LICENSE is required')
    for path in sorted(components.rglob('*')):
        if path.suffix not in ('.h', '.cpp', '.c', '.hpp'):
            continue
        text = path.read_text(encoding='utf-8', errors='replace')
        code = re.sub(r'//[^\n]*|/\*.*?\*/', '', text, flags=re.S)
        for rule, why in CODE_RULES:
            match = rule.search(code)
            if match:
                line = code[:match.start()].count('\n') + 1
                fail(name, f'{path.relative_to(folder)}:{line}: {why}')
    return raw, translations, readme


ENTRY_FIELDS = {'repo', 'path', 'ref', 'maintainer', 'topics'}


def check_entry(path):
    """An entry of community/<id>.yaml: a public GitHub repository of the maintainer, the plugin's folder in it, and
    optionally the release tag to list (the newest release when left out). SystemExit with what is wrong."""
    path = Path(path)
    try:
        entry = yaml.safe_load(path.read_text(encoding='utf-8'))
    except (OSError, yaml.YAMLError) as error:
        fail(path.name, str(error))
    if not isinstance(entry, dict) or set(entry) - ENTRY_FIELDS or not {'repo', 'maintainer'} <= set(entry):
        fail(path.name, f'an entry has repo and maintainer, and may have {", ".join(sorted(ENTRY_FIELDS - {"repo", "maintainer"}))}')
    if not re.fullmatch(r'[a-z][a-z0-9_]{0,31}', path.stem):
        fail(path.name, 'the file is named after the plugin\'s id: community/<id>.yaml')
    match = re.fullmatch(r'https://github\.com/([A-Za-z0-9_.-]+)/([A-Za-z0-9_.-]+)', str(entry['repo']))
    if not match:
        fail(path.name, 'repo is https://github.com/<owner>/<repository>')
    if str(entry['maintainer']).lower() != match.group(1).lower():
        fail(path.name, 'the maintainer is the owner of the repository')
    if '..' in str(entry.get('path', '.')) or str(entry.get('path', '.')).startswith('/'):
        fail(path.name, 'path is a folder inside the repository')
    return entry


def main():
    folders = [Path(arg) for arg in sys.argv[1:]] or [
        *(f for f in sorted((ROOT / 'plugins').iterdir()) if (f / 'tessera-plugin.yaml').is_file()), ROOT / 'template']
    for folder in folders:
        check_folder(folder)
        print(f'{folder.name}: ok')
    if not sys.argv[1:]:
        for entry in sorted((ROOT / 'community').glob('*.yaml')):
            check_entry(entry)
            print(f'community/{entry.name}: ok')
        check_docs()
        print('docs: ok')
    print(f'plugin API {plugin_api()}: {len(folders)} checked')


if __name__ == '__main__':
    main()
