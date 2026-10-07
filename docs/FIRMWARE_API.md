# The firmware API (plugin API 0.1)

A plugin's code runs on the screen as an ESPHome component. It talks to Tessera's core through one header:

```cpp
#include "esphome/components/smart_display/plugin_api.h"
```

The header lives in the Tessera repository at `components/smart_display/plugin_api.h`; it is the reference, and this
page explains it. Everything is in namespace `tessera`.

## The component: `__init__.py`

```python
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

my_idea_ns = cg.esphome_ns.namespace("my_idea")
MyIdea = my_idea_ns.class_("MyIdea", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(MyIdea)}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
```

`smart_display.register_plugin(var, __file__)` finds `tessera-plugin.yaml` above the component folder and:

- checks that the manifest's `api` fits the core's plugin API, and stops the build with a clear sentence when not;
- hands the C++ object the plugin's id and version (`plugin_id()`, `plugin_version()`);
- hands it each tile's `memory` from the manifest, which the screen counts in its layout budget;
- hands it the texts of `translations/<language>.json`, part `screen`, in the language the screen is built in, with
  English for every key that language lacks (`text("key")`).

A plugin with configuration of its own (a pin, a name) adds it to `CONFIG_SCHEMA` as any ESPHome component does, and
`plugin.yaml` passes the values in (`pin: ${PIN}`, from the manifest's `inputs`).

## The plugin: `tessera::Plugin`

```cpp
class MyIdea : public Component, public tessera::Plugin {
 public:
  void setup() override { add_tile("next", [this]() { return new NextTile(this); }); }
  float get_setup_priority() const override { return setup_priority::DATA; }
};
```

| Member | What |
|---|---|
| `add_tile(id, make)` | A tile type, by its id in the manifest. Call it in `setup()`. `make` returns a new tile object; the core deletes it. |
| `text(key)` | A text of part `screen` of the plugin's translations, `""` for an unknown key. |
| `plugin_id()`, `plugin_version()` | From the manifest. |
| `memory(tile)` | A tile type's `memory` from the manifest. |

The moments every plugin can hear (override what you need; each has an empty default):

| Moment | When |
|---|---|
| `on_ready()` | The screen's interface is up. |
| `on_tick(now_ms)` | Every 250 ms, with `millis()`. Keep it short: the screen draws and takes taps in the same loop. |
| `on_standby(dark)` | The screen dimmed or went dark (`true`), or woke up (`false`). |
| `before_update()` | A firmware update starts: let go of large buffers. |

## A tile: `tessera::Tile`

One tile object per card on the glass. A board with PSRAM keeps the cards of every page, so the object lives as long as
the layout; a board without (the CYD) makes a new object when its page comes back. Never keep anything a tile needs
across objects in a global: what a tile shows comes from its options and its data.

```cpp
class NextTile : public tessera::Tile {
 public:
  explicit NextTile(const tessera::Plugin *plugin) : plugin_(plugin) {}
  void create(const tessera::TileContext &c) override;
  void on_state(JsonObjectConst data) override;
  void on_tick(uint32_t epoch) override;
  void on_theme() override;
  void on_tap() override;
};
```

### The life of a tile

1. **`create(context)`**: once, first. Make every LVGL object inside `context.parent`, read the options.
2. **`on_state(data)`**: right after `create()`, and again whenever the app sent new data, the next time the card is
   drawn. Keep what you need from `data` in members; the JSON is gone after the call.
3. **`on_tick(epoch)`**: right after every `on_state()`, and then once a second while the card is on the glass. Set the
   texts here, from the members and the clock.
4. **`on_theme()`**: the screen went light or dark. Set every colour again.
5. **`on_tap()`**: a short tap on the card, after the touch filter (a finger that moved or was too short never reaches
   it). The tile's `tap` option set to "none" in the editor keeps taps away.
6. **Destructor**: the card shows something else. The core deletes the LVGL objects; free only memory of your own.

A change of the tile's size or options makes a new object: `create()` never has to handle a resize.

### `TileContext`

| Field | What |
|---|---|
| `parent` | The card's drawing area. Everything goes in here. |
| `width`, `height` | Its size in pixels, the card's padding already off. |
| `columns`, `rows` | The grid cells the tile covers (1x1, 2x1, ...). |
| `name` | The name the tile got in the editor, `""` for none. Valid during `create()` only: copy it. |
| `options` | The tile's options, the manifest's defaults filled in. Read with ArduinoJson: `c.options["walk"] \| 0`. |

### What `on_state` gets

For a tile with `data: <fetch>` in the manifest, the mapped answer of that fetch ([FETCH.md](FETCH.md)):

```json
{ "items": [ { "line": "15", "to": "Station Sloterdijk", "at": 1791386619 } ] }
```

or, for a map without `items`, the fields of one object. Two more keys can be there:

| Key | Meaning |
|---|---|
| `"stale": true` | The service did not answer the last time; this is the last good answer. Say so if it matters. |
| `"wait": "<why>"` | There is no answer to show: `not_filled` (an option the URL needs is empty), `asking` (the first answer is on its way), `failed` (the service did not answer and there is no older answer), `too_large`, `refused`. |

A tile without `data` gets `{}`.

## Drawing: `tessera::ui`

The screen's look is the same on every tile, so a plugin draws with the core's sizes, colours and fonts.

| Function | What |
|---|---|
| `ui::mm(n)` | `n` millimetres of glass in this board's pixels. For anything a finger touches (at least 7 mm). |
| `ui::px(n)` | A size of the reference look (a 4-inch Guition at 170 dpi) in this board's pixels. For paddings and gaps. |
| `ui::large()` | The standard look (4 inches and up), or the compact one (the 2.8-inch CYD). |
| `ui::font(Font)` | The screen's font. |
| `ui::line_height(Font)`, `ui::text_width(text, Font)` | To lay out before setting. |
| `ui::label(parent, Font, Role)` | A one-line label, cut with dots when too long. |
| `ui::set_text(label, text)`, `ui::set_font(label, Font)`, `ui::set_color(label, Role)` | Change it only when it differs: call them on every tick for free. |
| `ui::block(parent, Role)` | A rounded block with the radius of a key (a badge, a bar). |
| `ui::color(Role)` | A colour by its role. |
| `ui::icon(codepoint)` | An icon of Tessera's set as text, for a label in `Font::ICON` or `ICON_SMALL`. |

Fonts (`tessera::Font`), largest first. Take the largest that fits.

| Font | Used for |
|---|---|
| `VALUE` | The big number of a card. |
| `HEADLINE` | Large words. |
| `TITLE` | A card's name (bold). |
| `BODY_LARGE` | Words on a key. |
| `BODY` | A card's second line. |
| `ICON`, `ICON_SMALL` | Icons of Tessera's set. |

Colours are theme roles (`theme::Role`, from `theme.h`): each has a light and a dark value, so a tile follows the
screen's look. The ones a tile needs most:

| Role | For |
|---|---|
| `theme::INK` | Text, the most important. |
| `theme::INK_SOFT`, `theme::MUTED`, `theme::SUBTLE` | Less important text, in that order. |
| `theme::ACCENT` | The one thing that stands out (a badge, a bar). |
| `theme::ON_ACCENT` | Text on the accent. |
| `theme::CARD`, `theme::RAISED`, `theme::LINE`, `theme::TRACK` | Surfaces and lines inside a card. |

## The rest of the core

| Function | What |
|---|---|
| `tessera::epoch()` | The screen's clock, seconds since 1970; 0 until Home Assistant set it. |
| `tessera::local_time(epoch)` | That moment in the screen's time zone: `year, month, day, hour, minute, second, weekday`. |
| `tessera::clock_text(epoch)` | A time of day as the screen writes it (24 hours or AM/PM, as its settings say). |
| `tessera::format(text, n)` | `{n}` filled in; with `"1 day \| {n} days"` the form that fits `n` in the screen's language. |
| `tessera::fill(text, name, value)` | `{name}` filled in. |
| `tessera::refresh()` | Draw the plugin's cards again in the next pass, after a change outside `on_state` and `on_tick`. |

## Rules

`tools/check.py` refuses code that breaks the first six.

1. **No colour as a number** (`0xFF8800`, `lv_color_hex`): use a role.
2. **No font of your own**: use `tessera::Font`. Every font costs flash, and the 4 MB boards are close to full.
3. **No LVGL click handlers** (`LV_EVENT_CLICKED`, `LV_EVENT_PRESSED`): use `on_tap`, which has the touch filter in
   front of it.
4. **No timers, tasks or waits** (`lv_timer_create`, `xTaskCreate`, `delay`): use `on_tick` and `on_state`.
5. **No network from the screen** (`http_request`, an HTTP client): data comes through the app's `fetch`.
6. **No walk of the heap** (`heap_caps_get_largest_free_block`, `heap_caps_get_info`): it shifts the picture of an
   RGB panel while it runs.
7. **Make objects in `create()` only.** `on_state` and `on_tick` change what exists; they create and delete nothing.
8. **Keep `memory` honest.** Each LVGL label is about 100 bytes, a block about 80, plus your members.
9. **Icons from Tessera's set only.** Another codepoint shows as nothing.
10. **Text from `translations/`.** No words in the code.

## The two examples

- [`template/components/my_plugin/my_plugin.cpp`](../template/components/my_plugin/my_plugin.cpp): a number of days, a
  line of text, the largest font that fits, light and dark. About 90 lines.
- [`plugins/ov_departures/components/ov_departures/ov_departures.cpp`](../plugins/ov_departures/components/ov_departures/ov_departures.cpp):
  data from a fetch, a single-cell layout and a list layout, badges, a countdown every second, waiting states.

## What changes in later versions

Planned for the API, not in 0.1: a settings page of a plugin on the screen, a card of its own (a full screen opened by
a tap), items in the top bar, a tap action for existing tiles, messages between the screen and the app, and tiles that
belong to a Home Assistant entity. While the API is 0.x any of these may change it; the core says so in its release
notes, and a plugin raises its `api` when it moves along.
