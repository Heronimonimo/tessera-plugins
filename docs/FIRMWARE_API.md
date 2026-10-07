# The firmware API (plugin API 0.3)

A plugin's code runs on the screen as an ESPHome component. It talks to Tessera's core through one header:

```cpp
#include "esphome/components/smart_display/plugin_api.h"
```

The header lives in the Tessera repository at `components/smart_display/plugin_api.h`; it is the reference, and this
page explains it. Everything is in namespace `tessera`.

**Versions.** A manifest names the API it was written for (`api: "0.2"`). A plugin builds on every core with the same
major and at least its minor: something new raises the minor, only a break raises the major. 0.1 has tiles and the
moments; 0.2 adds tiles of an entity, cards, tap actions, top bar items, settings rows, questions to the app and the
date words; 0.3 adds `on_touch`, and a settings action that says how it is going and is lit while it runs. Name the lowest minor whose parts you use, so the plugin builds on as many screens as possible.

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
| `add_card(id, make, wide)` | A card (0.2), by its id in the manifest's `cards`. `wide`: as wide as the glass instead of a hand's width. |
| `add_tap_action(id, run)` | A tap action for Home Assistant's own tiles (0.2), by its id in `tap_actions`. |
| `add_bar_item(id, read)` | An item for the top bar (0.2), by its id in `bar_items`. |
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
| `settings(page)` | Once, when the interface is up: add rows to the screen's settings page (0.2, below). Return true when you added some. |
| `on_message(message)` | An answer of the app to `tessera::send()` (0.2, below). |
| `on_cards_closed()` | The cards closed: Back, standby, Back to page 1, another card (0.2). |
| `on_alert()` | An alert is about to show, a doorbell for example (0.2). |
| `on_touch()` | A tap the screen took, after the touch filter (0.3): a tile, a key, a button, a row of the settings, Back or the pager. For a click or a buzz. It runs inside the touch event, so start a sound there and never wait for it. A slider's release, a refused tap and the repeat of a held key do not count. |

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
| `entity` | The Home Assistant entity the tile belongs to (0.2, a manifest tile with `entity`), `""` for none. Copy it. |
| `tile` | The tile's index in the layout (0.2): pass it to `tessera::open_card` so the card follows the tile. |
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

A tile with `entity` (0.2) also gets its entity, and is sent again whenever that entity changes in Home Assistant:

```json
{ "state": "off", "name": "Waste", "attributes": { "message": "Paper", "start_time": 1791410400 } }
```

`attributes` holds only the ones the manifest's `attributes` names, a text cut at 48 bytes and a list at 16 items. An
attribute whose name ends in `_at`, `_time` or `date` and holds a moment comes as seconds since 1970, so the tile says it
in the screen's words (`tessera::date_text`, `days_from_today`). `"wait": "wrong_entity"` means the entity chosen in the
editor is of another domain than the manifest names.

A tile without `data` and without `entity` gets `{}`.

## A tile of an entity (0.2)

In the manifest, `entity: calendar` (or a list of domains) and `attributes: [message, start_time]`. The editor then
offers the entities of those domains in the tile's inspector, also of a domain Tessera itself draws no tile for. To act on
the entity, as a tap on one of Tessera's tiles would:

```cpp
tessera::action("light.toggle", entity_);                                   // returns false when it was not sent
tessera::action("climate.set_temperature", entity_, "temperature", "21");
```

The screen must be allowed to perform actions, and the manifest names each action under
`permissions.home_assistant_actions`.

## A card (0.2)

A card is a screen of its own over the page, opened by a tile's `on_tap`, a tap action or a settings row. Tessera draws
its frame: the page's ground, a round back key at the top left and the title in the middle; the card draws everything
under it. One card is open at a time, and it closes as Tessera's own cards close: Back, standby, Back to page 1, another
card.

```cpp
class Upcoming : public tessera::Card {
 public:
  void open(const tessera::CardContext &c) override;   // make the parts in c.parent (c.width x c.height)
  void on_state(JsonObjectConst data) override;         // the tile it was opened from changed
  void on_tick(uint32_t epoch) override;                // once a second while open
  void on_theme() override;
  bool on_back() override { return false; }             // true: stay open (you went back a step of your own)
};

add_card("upcoming", [this]() { return new Upcoming(this); });
tessera::open_card(plugin_id(), "upcoming", entity_, tile_);   // from a tile's on_tap
```

`CardContext` has `parent`, `width`, `height`, `entity` (what it was opened for, `""` for none) and `tile` (-1 for
none). `on_state` gets a plugin tile's data, or `{"state", "name"}` of one of Tessera's tiles. The title is the tile's
name unless `open_card` passes one; `tessera::close_card()` closes it as Back does.

## A tap action (0.2)

A tap action lets a person set one of Tessera's own tiles to do something of the plugin on a tap: a sensor tile that
opens the plugin's card. The manifest's `tap_actions` names the domains it is for; the editor offers it in the tap
choices of those tiles on a screen that runs the plugin. A hold still opens the tile's own card, and a screen without
the plugin does nothing on the tap.

```cpp
add_tap_action("upcoming", [this](const tessera::TapContext &c) {
  tessera::open_card(plugin_id(), "upcoming", c.entity, c.tile);   // c.entity, c.name, c.tile
});
```

## A top bar item (0.2)

The manifest's `bar_items` lists them; a person places one on a page's top bar in the editor, under "From plugins".
The screen asks the plugin what it shows every few seconds and draws the bar again when that changed:

```cpp
add_bar_item("soon", [this]() {
  tessera::BarItem item;           // shown, icon (a codepoint of Tessera's set), text (a few words)
  if (tomorrow_) { item.shown = true; item.icon = 0xF044C; item.text = "Tomorrow: Paper"; }
  return item;
});
```

An item that is not shown takes no room. A screen without the plugin draws nothing for it.

## Settings rows (0.2)

Rows on the screen's own settings page (hold the top bar): Settings, then Plugins, then the plugin's page, drawn exactly
as Tessera's rows. Keep the values where Home Assistant sees them too: an ESPHome entity of `plugin.yaml` (a template
switch, number or select with `restore_value`).

```cpp
bool settings(tessera::SettingsPage &page) override {
  page.icon = "\U000F044C";                       // the row's icon in the list of plugins; the title is the name
  page.toggle(text("in_bar"), [this] { return in_bar_->state; },
              [this](bool on) { on ? in_bar_->turn_on() : in_bar_->turn_off(); });
  page.number(text("days_ahead"), 0, 3, 1, "", [this] { return (int) days_->state; },
              [this](int v) { auto call = days_->make_call(); call.set_value(v); call.perform(); });
  page.card(text("open_card"), "\U000F00ED", "upcoming");   // a row that opens a card
  return true;
}
```

Also `choice(label, {words...}, read, write)`, `action(label, icon, run, confirm, text)` and `info(label, text)`. At
most twelve rows. An action with `confirm` asks once, as Restart does. With `text` (0.3) it says how it is going on its
right, read again every second while the page shows: a test that runs, a countdown. `.active(running)` after it
lights the row in the accent while `running` says so (0.3), the way a row that asks is lit; a tap on it is yours to
stop what runs.

```cpp
page.action(text("tone"), "\U000F0387", [this] { tone_ ? stop() : play_tone(); }, nullptr,
            [this]() -> std::string { return tone_ ? text("playing") : ""; })
    .active([this] { return tone_; });
```

## Questions to the app (0.2)

A plugin may ask Home Assistant something the screen cannot: the events of a calendar, a sensor's history. The app asks
it on the plugin's behalf, only a command the manifest names under `permissions.ha_commands`, and logs every one.

```cpp
JsonDocument request;
request["ask"] = "call_service:calendar.get_events";      // or a websocket command: "history/history_during_period"
request["data"]["entity_id"] = "calendar.waste";
request["data"]["duration"]["days"] = 28;
asked_ = tessera::send(this, request.as<JsonObjectConst>());   // 0 when it could not be sent

void on_message(JsonObjectConst m) override {             // {"re": asked_, "ok": true, "result": {...}}
  if ((m["re"] | 0u) != asked_) return;                    //  or {"re", "ok": false, "error": "not_allowed"}
}
```

`call_service:<domain>.<service>` sends the action with `return_response` and hands back its response; `entity_id`,
`device_id` and `area_id` go as the target, the rest as the action's data. A request is at most 512 bytes; an answer at
most about 3 KB: longer texts are cut to 48 bytes and lists shortened from the end.

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
| `tessera::days_from_today(epoch)` | 0 today, 1 tomorrow, -1 yesterday, on the screen's calendar (0.2). `INT32_MIN` while the clock is not set. |
| `tessera::date_text(epoch)` | A day as the top bar writes it: "Fri 9 Oct", "vr 9 okt" (0.2). |
| `tessera::days_text(days)` | "Tomorrow", "In 3 days" in the screen's language; `""` for today (bring your own word) and the past (0.2). |
| `tessera::action(service, entity, key, value)` | A Home Assistant action on an entity (0.2, see "A tile of an entity"). |
| `tessera::open_card(plugin_id, card, entity, tile, title)`, `close_card()` | Open or close a card (0.2). |
| `tessera::send(plugin, request)` | Ask the app (0.2, see "Questions to the app"). |

## Rules

`tools/check.py` refuses code that breaks the first six.

1. **No colour as a number** (`0xFF8800`, `lv_color_hex`): use a role.
2. **No font of your own**: use `tessera::Font`. Every font costs flash, and the 4 MB boards are close to full.
3. **No LVGL click handlers** (`LV_EVENT_CLICKED`, `LV_EVENT_PRESSED`): use `on_tap`, which has the touch filter in
   front of it.
4. **No timers, tasks or waits** (`lv_timer_create`, `xTaskCreate`, `delay`): use `on_tick` and `on_state`. A plugin
   that streams (sound to a speaker) may use its ESPHome component's own `loop()`, as long as nothing in it waits.
5. **No network from the screen** (`http_request`, an HTTP client): data comes through the app's `fetch`.
6. **No walk of the heap** (`heap_caps_get_largest_free_block`, `heap_caps_get_info`): it shifts the picture of an
   RGB panel while it runs.
7. **Make objects in `create()` only.** `on_state` and `on_tick` change what exists; they create and delete nothing.
8. **Keep `memory` honest.** Each LVGL label is about 100 bytes, a block about 80, plus your members.
9. **Icons from Tessera's set only.** Another codepoint shows as nothing.
10. **Text from `translations/`.** No words in the code.

## The examples

- [`template/components/my_plugin/my_plugin.cpp`](../template/components/my_plugin/my_plugin.cpp): a number of days, a
  line of text, the largest font that fits, light and dark. About 90 lines.
- [`plugins/ov_departures/components/ov_departures/ov_departures.cpp`](../plugins/ov_departures/components/ov_departures/ov_departures.cpp):
  data from a fetch, a single-cell layout and a list layout, badges, a countdown every second, waiting states.
- [`plugins/waste_collection/components/waste_collection/waste_collection.cpp`](../plugins/waste_collection/components/waste_collection/waste_collection.cpp):
  everything of 0.2 in one plugin: a tile of a calendar entity, a card that asks Home Assistant for the coming events, a
  tap action, a top bar item, two settings rows backed by ESPHome entities of `plugin.yaml`, and an input of kind entity.
- [`plugins/p4_audio/components/p4_audio/p4_audio.cpp`](../plugins/p4_audio/components/p4_audio/p4_audio.cpp): a board's
  own hardware as a plugin (`boards: [wavesharep4]`): ESPHome's speaker and microphone from `plugin.yaml`, a click in
  `on_touch`, settings actions that say how they are going, and sound streamed from PSRAM in `loop()` without a wait.

## What changes in later versions

Not in the API yet: a plugin's own messages beyond Home Assistant commands, and pictures (a camera of the plugin's
own). Each will raise the minor; a plugin
raises its `api` only when it uses one.
