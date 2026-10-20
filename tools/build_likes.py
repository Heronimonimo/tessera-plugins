#!/usr/bin/env python3
"""Write likes.json and stats.json: what people share about each plugin of the index, copied from Tessera's website.

    python3 tools/build_likes.py

A like, a rating and a count are given in the Tessera app and counted by the website; the app reads them from these
files on GitHub, as it reads index.json, and never asks the website. likes.json has how many people like each plugin
(for apps from before ratings); stats.json has per plugin its likes, its rating (null below three ratings), its number
of ratings, and the homes that use it of those who agreed to be counted. Both hold only plugins of index.json, sorted,
and no time, so they change only when a number does. When the website does not answer (an error, a timeout, an answer
of another shape), a file stays as it is. CI runs this after tools/build_index.py every hour (.github/workflows/index.yml).
"""
import json
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SITE = 'https://tessera-maxgramser.on-forge.com/api/v1/addon/plugins'
LIKES = ROOT / 'likes.json'
STATS = ROOT / 'stats.json'
FORMAT = 1


def fetch(path, name):
    """The website's `data` for one path, or None (and why on stderr) when it gave none."""
    request = urllib.request.Request(f'{SITE}/{path}', headers={'Accept': 'application/json', 'User-Agent': 'tessera-plugins'})
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            return json.loads(response.read(512 * 1024))['data']
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'{name}: the website gave nothing ({error}); {name}.json stays as it is', file=sys.stderr)
        return None


def count(value):
    return isinstance(value, int) and not isinstance(value, bool) and value >= 0


def numbers(value):
    """Whether one plugin's stats are what the website promises: three counts, and a rating from 1 to 5 or null."""
    rating = value.get('rating') if isinstance(value, dict) else None
    return (isinstance(value, dict) and set(value) == {'likes', 'rating', 'ratings', 'homes'}
            and all(count(value[key]) for key in ('likes', 'ratings', 'homes'))
            and (rating is None or (isinstance(rating, (int, float)) and not isinstance(rating, bool) and 1 <= rating <= 5)))


def write(path, key, found, listed):
    data = {'format': FORMAT, key: {plugin: found[plugin] for plugin in sorted(found) if plugin in listed}}
    text = json.dumps(data, indent=1, ensure_ascii=False) + '\n'
    if path.is_file() and path.read_text() == text:
        print(f'{path.name} unchanged')
        return
    path.write_text(text)
    print(f'{path.name}: {len(data[key])} plugins')


def main():
    listed = {plugin['id'] for plugin in json.loads((ROOT / 'index.json').read_text()).get('plugins') or []}
    likes = fetch('likes', 'likes')
    if likes is not None and not (isinstance(likes, dict) and all(count(n) for n in likes.values())):
        print('likes: the website\'s answer is no list of counts; likes.json stays as it is', file=sys.stderr)
        likes = None
    stats = fetch('stats', 'stats')
    if stats is not None and not (isinstance(stats, dict) and all(numbers(n) for n in stats.values())):
        print('stats: the website\'s answer is no list of numbers; stats.json stays as it is', file=sys.stderr)
        stats = None
    for path, key, found in ((LIKES, 'likes', likes), (STATS, 'plugins', stats)):
        if found is not None:
            write(path, key, found, listed)
        elif not path.is_file():
            path.write_text(json.dumps({'format': FORMAT, key: {}}, indent=1) + '\n')


if __name__ == '__main__':
    main()
