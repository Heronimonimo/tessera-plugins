#!/usr/bin/env python3
"""The picture of a plugin on the Plugins page: store.png beside its tessera-plugin.yaml, made from the bench.

    python3 tools/store_image.py plugins/my_idea                 # renders on the bench, writes plugins/my_idea/store.png
    python3 tools/store_image.py ../my-plugin --from ../my-plugin/.preview   # from a bench run that has these screens
    python3 tools/store_image.py plugins/my_idea --large jc8012p4a1 --small guition --out /tmp/store.png

It is a thumbnail, as a video has one: the plugin's name and summary (translations/en.json, `app`) large at the top
left, and three of its screens filling the rest. It runs the bench (tools/preview.py) on a large screen lying down, a
small square one and the CYD, and puts a page of each in a frame of its own: the large one and the CYD light, the
square one dark. A page is laid out here from
the bench's pictures of the plugin's tiles, the way a person would fill it: the tile at its larger sizes first, every
example of preview.yaml once where there is room, and every cell used when the sizes allow it. A plugin with a card
shows the card a tap on its first example opens on the small screen instead. A plugin with a screensaver and no tile
shows its first screensaver on each screen that shows it, as the bench photographed it in standby (plugin API 0.14).
The top bar of each page is the screen's own, from the bench.

The words are the English ones: the picture is the same in every language of the app.
docs/PUBLISHING.md, "The picture on the Plugins page", says what the app takes (store.png, 1280 x 720, at most 1 MB) and
that a picture of your own is just as welcome.
"""
import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

try:
    import yaml
    from PIL import Image, ImageDraw, ImageFilter, ImageFont
except ImportError:   # the bench's own Python has Pillow and PyYAML; run there
    import preview  # noqa: E402
    python = preview.python_of_venv()
    if not python.exists():
        raise SystemExit('store_image: run python3 tools/preview.py --setup once first (it makes a Python with Pillow)')
    raise SystemExit(subprocess.run([str(python), __file__, *sys.argv[1:]]).returncode)


WIDTH, HEIGHT = 1280, 720          # what the app takes: 16:9, and sharp on a large display
SCALE = 2                          # drawn at twice the size, then made smaller, for smooth edges
MAX_BYTES = 1024 * 1024
LARGE, SMALL, TINY = 'jc8012p4a1', 'guition', 'cyd'
# Tessera's mark (components/smart_display/theme.h, MARK_*): the colours of the glow behind the screens.
MARK = {'blue': (0x00, 0x9F, 0xE3), 'amber': (0xFF, 0xC1, 0x07), 'purple': (0x92, 0x6B, 0xC7), 'green': (0x4C, 0xAF, 0x50)}
GROUND = (242, 242, 239)           # a calm, light ground
INK, MUTED = (17, 17, 17), (92, 96, 104)
TILE_MARGIN = 3                    # the bench photographs a tile with this much of the page around it


def say(text):
    print(text, flush=True)


# ---- The bench's pictures ----

def bench(folder, screens, out):
    """Run the bench on these screens into `out`; it builds what changed only."""
    command = [sys.executable, str(ROOT / 'tools' / 'preview.py'), str(folder), '--out', str(out)]
    for key in screens:
        command += ['--board', key]
    say(f'store_image: rendering on {", ".join(screens)} (a first build takes several minutes)')
    done = subprocess.run(command)
    if done.returncode not in (0, 1):   # 1: findings, or a plugin of hardware the bench cannot build
        say(f'store_image: the bench stopped ({done.returncode}); the picture shows what it has')


def screen_of(summary, key):
    return next((s for s in summary['screens'] if s['key'] == key), None)


def page_geometry(folder, screen, page_file):
    """Where the tiles of a page go: (left, top, cell w, cell h, gap, columns, rows), from the boxes the bench logged
    for its own tiles on this screen and the grid it used."""
    boxes = []
    for line in (folder / 'program.log').read_text(errors='replace').splitlines():
        found = re.search(r'tile_box index=\d+ box=(-?\d+),(-?\d+),(-?\d+),(-?\d+)', line)
        if found:
            x1, y1, x2, y2 = map(int, found.groups())
            if x2 > x1 and y2 > y1:
                boxes.append((x1, y1, x2, y2))
    view = screen['views'][0]
    columns, rows = (int(n) for n in view['grid'].split('x'))
    single = next((t for t in view['tiles'] if t['span'] == '1x1'), None)
    if not boxes or single is None:
        return None
    left, top = min(b[0] for b in boxes), min(b[1] for b in boxes)
    cell_w, cell_h = single['px']
    gap = (screen['width'] - 2 * left - columns * cell_w) / max(1, columns - 1) if columns > 1 else 0
    return left, top, cell_w, cell_h, gap, columns, rows


def rank(example):
    """The place of an example in preview.yaml: the bench names it so ("1-living-room"). The first ones are what the
    maker shows first; the later ones are often a state (no data, an old answer)."""
    found = re.match(r'(\d+)-', example)
    return int(found.group(1)) if found else 99


def candidates(screen, theme, first_only=False):
    """Every tile picture of the screen's own grid, or of its first example alone: (example, columns, rows, file)."""
    out = []
    for tile in screen['views'][0]['tiles']:
        span = tile.get('span') or ''
        if not re.fullmatch(r'\d+x\d+', span) or not tile.get(theme):
            continue
        columns, rows = (int(n) for n in span.split('x'))
        out.append((tile['example'], columns, rows, tile[theme]))
    if first_only and out:
        first = min(rank(example) for example, *_ in out)
        out = [picture for picture in out if rank(picture[0]) == first]
    return out


def arrange(pictures, columns, rows, budget=200_000):
    """The best page of these pictures on a grid: every cell used where possible, the larger sizes, every example once
    before any comes twice, and few enough tiles that each reads. No tile takes more than a quarter of a large grid: a
    tile that big is mostly room, and a page of a few sizes shows more of the plugin. [(column, row, picture)]."""
    best = {'score': None, 'placed': []}
    grid = [[False] * columns for _ in range(rows)]
    largest = max(4, columns * rows // 4)
    order = sorted((p for p in pictures if p[1] * p[2] <= largest), key=lambda p: -(p[1] * p[2]))
    steps = [0]

    def score(placed):
        cells = sum(p[1] * p[2] for _, _, p in placed)
        examples = len({p[0] for _, _, p in placed})
        twice = len(placed) - examples
        same = len(placed) - len({(p[0], p[1], p[2]) for _, _, p in placed})
        first = sum(p[1] * p[2] * max(0, 4 - rank(p[0])) for _, _, p in placed)
        return cells * 100 + examples * 30 + first * 6 - len(placed) * 12 - twice * 25 - same * 60

    def free(c, r, w, h):
        return c + w <= columns and r + h <= rows and all(not grid[y][x] for y in range(r, r + h) for x in range(c, c + w))

    def mark(c, r, w, h, value):
        for y in range(r, r + h):
            for x in range(c, c + w):
                grid[y][x] = value

    def walk(placed, used):
        steps[0] += 1
        if steps[0] > budget:
            return
        cell = next(((c, r) for r in range(rows) for c in range(columns) if not grid[r][c]), None)
        if cell is None:
            value = score(placed)
            if best['score'] is None or value > best['score']:
                best['score'], best['placed'] = value, list(placed)
            return
        c, r = cell
        for picture in order:
            example, w, h, _ = picture
            if used.count((example, w, h)) >= 3 or not free(c, r, w, h):
                continue
            mark(c, r, w, h, True)
            placed.append((c, r, picture))
            walk(placed, used + ((example, w, h),))
            placed.pop()
            mark(c, r, w, h, False)
        grid[r][c] = 'empty'   # the cell stays empty
        walk(placed, used)
        grid[r][c] = False

    walk([], ())
    return best['placed']


def compose_page(folder, screen, theme, first_only=False):
    """A page of the screen in this theme: its top bar from the bench's first page, then the arranged tiles."""
    page_file = folder / f'page-1-{theme}.png'
    geometry = page_geometry(folder, screen, page_file)
    if not page_file.is_file() or geometry is None:
        return None
    original = Image.open(page_file).convert('RGB')
    left, top, cell_w, cell_h, gap, columns, rows = geometry
    ground = original.getpixel((2, original.height // 2))
    page = Image.new('RGB', original.size, ground)
    page.paste(original.crop((0, 0, original.width, max(0, top - 1))), (0, 0))
    placed = arrange(candidates(screen, theme, first_only), columns, rows)
    if not placed:
        return None
    for c, r, (_, _, _, file) in placed:
        picture = Image.open(folder / file).convert('RGB')
        x = round(left + c * (cell_w + gap)) - TILE_MARGIN
        y = round(top + r * (cell_h + gap)) - TILE_MARGIN
        page.paste(picture, (x, y))
    return page


def card_page(folder, screen):
    """The plugin's own card on the small screen: the one a tap on its first example opens, with that tile's data (a
    card opened by its id alone may wait for an entity). None for a plugin without a card."""
    view = screen['views'][0]
    if not view.get('cards'):
        return None
    taps = [tap for tap in view.get('taps') or [] if not tap.get('actions') and (folder / tap['file']).is_file()]
    if not taps:
        return None
    first = min(taps, key=lambda tap: rank(tap['file'].removeprefix('tap-')))
    return Image.open(folder / first['file']).convert('RGB')


def plain_page(folder, screen, theme):
    """The page of a plugin with no tile but items in the top bar: the bench renders the first page with them."""
    if screen['views'][0].get('tiles'):
        return None
    for page in screen['views'][0].get('pages') or []:
        if page['theme'] == theme and page['page'] == 1 and (folder / page['file']).is_file():
            return Image.open(folder / page['file']).convert('RGB')
    return None


def saver_page(folder, screen, theme='dark'):
    """The plugin's first screensaver on this screen, as the bench photographed it in standby (plugin API 0.14): the
    screen in this look, the saver dark in both. None when the screen showed none (a face of the music or the photos on
    a screen without pictures, a screen that never goes into standby)."""
    for item in screen['views'][0].get('savers') or []:
        file = item.get(theme) or item.get('light') or item.get('dark')
        if file and (folder / file).is_file():
            return Image.open(folder / file).convert('RGB')
    return None


def settings_page(folder, screen):
    """The plugin's own page on the screen's settings page, else the list of plugins there that leads to it."""
    pages = [s for s in screen['views'][0].get('settings') or [] if (folder / s['file']).is_file()]
    if not pages:
        return None
    own = next((s for s in pages if s['title'] != 'Settings, Plugins'), pages[0])
    return Image.open(folder / own['file']).convert('RGB')


def shot(source, summary, key, wanted):
    """What a screen shows on the picture: the first of `wanted` the bench has for it. ('tiles', theme) a page of the
    plugin's tiles, ('first', theme) of its first example alone, ('card',) the card a tap opens, ('bar', theme) the page of
    a plugin that only adds to the top bar, ('saver', theme) its first screensaver in standby, ('settings',) its page on
    the settings page."""
    screen = screen_of(summary, key) if summary else None
    if screen is None:
        return None
    folder = source / key
    for kind, *theme in wanted:
        image = {'tiles': lambda: compose_page(folder, screen, theme[0]),
                 'first': lambda: compose_page(folder, screen, theme[0], first_only=True),
                 'card': lambda: card_page(folder, screen),
                 'bar': lambda: plain_page(folder, screen, theme[0]),
                 'saver': lambda: saver_page(folder, screen, *theme),
                 'settings': lambda: settings_page(folder, screen)}[kind]()
        if image is not None:
            return image
    return None


def bar_band(page):
    """The top bar of a page picture, by its look: the rows from the top that hold something until a run of rows of
    the page's own colour. (top, bottom) or None."""
    ground, width = page.getpixel((1, page.height - 2)), page.width
    pixels = page.load()
    def busy(y):
        return any(max(abs(a - b) for a, b in zip(pixels[x, y], ground)) > 24 for x in range(0, width, 2))
    start = next((y for y in range(page.height // 4) if busy(y)), None)
    if start is None:
        return None
    quiet = 0
    for y in range(start, page.height // 3):
        quiet = 0 if busy(y) else quiet + 1
        if quiet >= 6:
            return max(0, start - 8), y - quiet + 1 + 8
    return None


def bar_callout(page, width):
    """The right half of a page's top bar, where a plugin's items stand beside the clock, made large on a card of the
    page's colour: a loupe over the bar."""
    band = bar_band(page)
    if band is None:
        return None
    top, bottom = band
    ground, pixels = page.getpixel((1, page.height - 2)), page.load()
    busy = [x for x in range(round(page.width * 0.3), page.width)
            if any(max(abs(a - b) for a, b in zip(pixels[x, y], ground)) > 24 for y in range(top, bottom))]
    if not busy:
        return None
    strip = page.crop((max(0, busy[0] - 16), top, min(page.width, busy[-1] + 17), bottom))
    inner = min(width - 48, round(strip.width * 150 / strip.height))   # the bar 150 px high, or as wide as there is room
    strip = strip.resize((inner, round(strip.height * inner / strip.width)), Image.LANCZOS)
    width = inner + 48
    size = (width, strip.height + 48)
    card = Image.new('RGBA', size, (0, 0, 0, 0))
    card.paste(Image.new('RGBA', size, page.getpixel((1, page.height - 2)) + (255,)), (0, 0), rounded(size, 36))
    card.paste(strip, (24, 24))
    ImageDraw.Draw(card).rounded_rectangle((1, 1, size[0] - 2, size[1] - 2), 36, outline=(0, 0, 0, 40), width=2)
    return card


def plugin_icon(name, size, colour):
    """The plugin's icon (an MDI name of Tessera's set) in white on a rounded square of `colour`, as an app's icon."""
    import preview
    tree = preview.core()
    sys.path.insert(0, str(tree / 'screen_manager' / 'app'))
    try:
        import tile_icons
        codepoint = tile_icons.GLYPHS.get(name) or 'F0A66'
    except Exception:
        codepoint = 'F0A66'   # puzzle-outline, as the app shows a plugin whose icon it does not know
    tile = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(tile)
    draw.rounded_rectangle((0, 0, size - 1, size - 1), round(size * 0.23), fill=colour)
    glyph = ImageFont.truetype(str(tree / 'fonts' / 'materialdesignicons-webfont.ttf'), round(size * 0.62))
    draw.text((size / 2, size / 2), chr(int(codepoint, 16)), font=glyph, fill=(255, 255, 255), anchor='mm')
    return tile


# ---- The picture ----

def rounded(size, radius):
    mask = Image.new('L', size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius, fill=255)
    return mask


def framed(page, width):
    """A page in a display's frame, `width` wide (the frame included): a black bezel with a faint edge of light."""
    bezel = max(10, round(width * 0.022))
    inner_w = width - 2 * bezel
    inner_h = round(page.height * inner_w / page.width)
    screen = page.resize((inner_w, inner_h), Image.LANCZOS)
    size = (width, inner_h + 2 * bezel)
    radius = round(bezel * 1.6)
    frame = Image.new('RGBA', size, (0, 0, 0, 0))
    body = Image.new('RGBA', size, (12, 13, 16, 255))
    frame.paste(body, (0, 0), rounded(size, radius))
    edge = ImageDraw.Draw(frame)
    edge.rounded_rectangle((1, 1, size[0] - 2, size[1] - 2), radius, outline=(70, 74, 84, 255), width=max(2, bezel // 8))
    frame.paste(screen, (bezel, bezel), rounded(screen.size, max(4, round(radius * 0.45))))
    return frame


def shadow(layer, blur, opacity):
    alpha = layer.split()[-1].point(lambda a: a * opacity)
    soft = Image.new('RGBA', layer.size, (0, 0, 0, 0))
    soft.putalpha(alpha)
    pad = blur * 3
    out = Image.new('RGBA', (layer.width + 2 * pad, layer.height + 2 * pad), (0, 0, 0, 0))
    out.paste(soft, (pad, pad))
    return out.filter(ImageFilter.GaussianBlur(blur)), pad


def font(weight, size):
    """Roboto, the screens' own font, from the copy of Tessera the bench keeps."""
    import preview
    for path in sorted(preview.CACHE.glob(f'tessera-*/fonts/Roboto-{weight}.ttf')):
        return ImageFont.truetype(str(path), size)
    raise SystemExit('store_image: no Roboto; run the bench once (python3 tools/preview.py --setup, then a render)')


def lines_of(text, face, width, most):
    """`text` in lines no wider than `width`, at most `most` of them (the last one ends in an ellipsis when cut)."""
    words, lines = text.split(), []
    while words and len(lines) < most:
        line = words.pop(0)
        while words and face.getlength(f'{line} {words[0]}') <= width:
            line += ' ' + words.pop(0)
        lines.append(line)
    if words:
        last = lines[-1]
        while last and face.getlength(last + '\u2026') > width:
            last = last.rsplit(' ', 1)[0] if ' ' in last else last[:-1]
        lines[-1] = last.rstrip(' ,.;:') + '\u2026'
    return lines


def mark(draw, x, y, size):
    """Tessera's mark: amber, blue, purple and green, as the top bar of every screen draws it."""
    u = size / 100
    box = lambda a, b, c, d: (x + a * u, y + b * u, x + c * u, y + d * u)
    draw.rounded_rectangle(box(0, 0, 46, 46), 13 * u, fill=MARK['amber'])
    draw.rounded_rectangle(box(53, 0, 100, 62), 13 * u, fill=MARK['blue'])
    draw.rounded_rectangle(box(0, 53, 62, 100), 13 * u, fill=MARK['purple'])
    draw.ellipse(box(69, 69, 100, 100), fill=MARK['green'])


def words(draw, name, summary, left, top, width, bottom):
    """The plugin's name, on one line as large as fits (two lines only when one would be too small), and what it does
    under it, in the colours of a light screen, ending above `bottom` (cut with an ellipsis when it is longer)."""
    for size in range(176, 119, -8):
        title = font(700, size)
        if title.getlength(name) <= width:
            name_lines = [name]
            break
    else:
        # Two lines, no larger than leaves room for two lines of the summary under them.
        most = min(152, int((bottom - top - 80 - 44 - 26 - 2 * 80) / 2.16))
        for size in range(most, 71, -4):
            title = font(700, size)
            name_lines = lines_of(name, title, width, 2)
            if not name_lines[-1].endswith('\u2026'):
                break
    mark(draw, left, top, 80)
    y = top + 80 + 44
    for line in name_lines:
        draw.text((left, y), line, font=title, fill=INK, anchor='lt')
        y += round(size * 1.08)
    y += 26
    # The summary whole, a size smaller where it would be cut, and cut with an ellipsis only at the smallest.
    for size in (60, 54, 48):
        body, step = font(400, size), round(size * 1.33)
        lines = lines_of(summary, body, width, min(3, max(1, (bottom - y) // step)))
        if not lines or not lines[-1].endswith('\u2026'):
            break
    for line in lines:
        draw.text((left, y), line, font=body, fill=MUTED, anchor='lt')
        y += step


def place(canvas, layer, x, y, lift=28):
    soft, pad = shadow(layer, 46, 0.32)
    canvas.alpha_composite(soft, (x - pad, y - pad + lift))
    canvas.alpha_composite(layer, (x, y))


def picture(large, small, tiny, bar, icon, name, summary):
    """A thumbnail, as a video has one: the plugin's name large and what it does at the top left, and the rest filled
    with what the plugin shows. With its tiles on a large screen: that screen running off the right edge, the small
    square one in front of it and the CYD at the left. Without (a card, its settings, the top bar): the small screen large
    on the right with the plugin's icon before it. With nothing a screen shows (hardware the bench cannot build): the
    plugin's icon, large. Items in the top bar get a loupe over the bar."""
    width, height = WIDTH * SCALE, HEIGHT * SCALE
    canvas = Image.new('RGBA', (width, height), GROUND + (255,))
    words(ImageDraw.Draw(canvas), name, summary, 130, 110, 1000, 880 if large is not None else (640 if tiny is not None else 1300))
    if large is not None:
        # One group, back to front: the large screen, the square one over its lower left, the CYD over the square
        # one's lower left.
        place(canvas, framed(large, 1680), 1260, 300)
        if small is not None:
            square = framed(small, 720)
            place(canvas, square, 1150, height - square.height - 60)
        if tiny is not None:
            cyd = framed(tiny, 600)
            place(canvas, cyd, 760 if small is not None else 1050, height - cyd.height - 40)
        if bar is not None:
            place(canvas, bar, width - bar.width - 90, 120, lift=18)
    elif small is not None or tiny is not None:
        main = framed(small, 960) if small is not None else framed(tiny, 1180)
        x, y = width - main.width - 150, round((height - main.height) / 2) + 40
        place(canvas, main, x, y)
        if small is not None and tiny is not None:
            cyd = framed(tiny, 600)
            place(canvas, cyd, x - 330, height - cyd.height - 60)
        if bar is not None:
            place(canvas, bar, x - 140, y - bar.height - 20, lift=18)
        place(canvas, icon(300), x - 150, y + main.height - 260, lift=20)
    else:
        tile = icon(760)
        place(canvas, tile, width - tile.width - 300, round((height - tile.height) / 2), lift=36)
    return canvas.convert('RGB').resize((WIDTH, HEIGHT), Image.LANCZOS)


def save(image, path):
    """PNG, made smaller with a palette when it is over the app's limit."""
    image.save(path, optimize=True)
    if path.stat().st_size > MAX_BYTES:
        image.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG).save(path, optimize=True)
    return path.stat().st_size


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('plugin', type=Path, help='the plugin folder (with tessera-plugin.yaml)')
    parser.add_argument('--from', dest='source', type=Path, help='a bench run (its --out folder) to take the pictures from')
    parser.add_argument('--large', default=LARGE, help=f'the large screen, lying down (default {LARGE})')
    parser.add_argument('--small', default=SMALL, help=f'the small square screen (default {SMALL})')
    parser.add_argument('--tiny', default=TINY, help=f'the smallest screen, lying down (default {TINY})')
    parser.add_argument('--out', type=Path, help='where to write the picture (default <plugin>/store.png)')
    args = parser.parse_args()
    folder = args.plugin.resolve()
    manifest = yaml.safe_load((folder / 'tessera-plugin.yaml').read_text())
    out = (args.out or folder / 'store.png').resolve()
    app = json.loads((folder / 'translations' / 'en.json').read_text(encoding='utf-8')).get('app') or {}
    name, text = app.get('name') or manifest['id'], app.get('summary') or ''
    with tempfile.TemporaryDirectory() as tmp:
        source = args.source.resolve() if args.source else Path(tmp)
        if not args.source:
            bench(folder, [args.large, args.small, args.tiny], source)
        try:
            summary = json.loads((source / 'summary.json').read_text())
        except (OSError, ValueError):
            summary = None   # the bench built nothing (hardware it cannot build): the icon tells it
        # An empty page with only the top bar shows little: the bar comes as a loupe instead (below).
        # A plugin of screensavers alone shows them on every screen that has one (the CYD has no picture for the music).
        large = shot(source, summary, args.large, [('tiles', 'light'), ('saver', 'light')])
        small = shot(source, summary, args.small, [('card',), ('first', 'dark'), ('saver', 'dark'), ('settings',)])
        tiny = shot(source, summary, args.tiny, [('first', 'light'), ('saver', 'light'), ('settings',)])
        bar = None
        if manifest.get('bar_items'):
            page = shot(source, summary, args.large, [('bar', 'light')]) or shot(source, summary, args.small, [('bar', 'light')])
            if page is None and summary and screen_of(summary, args.large):
                page = Image.open(source / args.large / 'page-1-light.png').convert('RGB') \
                    if (source / args.large / 'page-1-light.png').is_file() else None
            bar = bar_callout(page, 900) if page is not None else None
        colour = list(MARK.values())[sum(manifest['id'].encode()) % len(MARK)]
        icon = lambda size: plugin_icon(manifest.get('icon', ''), size, colour)
        size = save(picture(large, small, tiny, bar, icon, name, text), out)
    say(f'store_image: {out} ({WIDTH} x {HEIGHT}, {size // 1024} KB)')


if __name__ == '__main__':
    main()
