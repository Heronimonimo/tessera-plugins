# The firmware API (plugin API 0.11)

A plugin's code runs on the screen as an ESPHome component. It talks to Tessera's core through one header:

```cpp
#include "esphome/components/smart_display/plugin_api.h"
```

The header lives in the Tessera repository at `components/smart_display/plugin_api.h`; it is the reference, and this
page explains it. Everything is in namespace `tessera`.

A manifest names the plugin API it was written for: `api: "0.8"`, or `"0.9"`, `"0.10"` or `"0.11"` for the parts those brought; the plugin API 0.11 is the one the core offers now (on Tessera's dev branch; the release has 0.10 until the next one). Which cores
a plugin builds on is under "Versions" at the end of this page.

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

- checks that the manifest's `api` fits the core's plugin API ("Versions", below), and stops the build with one
  sentence when not;
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
| `add_card(id, make, wide)` | A card, by its id in the manifest's `cards`. `wide`: as wide as the glass instead of a hand's width. |
| `add_tap_action(id, run)` | A tap action for Home Assistant's own tiles, by its id in `tap_actions`. |
| `add_bar_item(id, read)` | An item for the top bar, by its id in `bar_items`. |
| `text(key)` | A text of part `screen` of the plugin's translations, `""` for an unknown key. |
| `plugin_id()`, `plugin_version()` | From the manifest. |
| `memory(tile)` | A tile type's `memory` from the manifest. |

The moments every plugin can hear (override what you need; each has an empty default):

| Moment | When |
|---|---|
| `on_ready()` | The screen's interface is up. |
| `on_interval(now_ms)` | Every 250 ms, the screen's own interval, with `millis()`. Keep it short: the screen draws and takes taps in the same loop. A tile's or a card's `on_tick` is another thing: once a second, with the clock. |
| `on_standby(dark)` | The screen dimmed or went dark (`true`), or woke up (`false`). |
| `before_update()` | A firmware update starts: let go of large buffers. |
| `settings(page)` | Once, when the interface is up: add rows to the screen's settings page (below). Return true when you added some. |
| `on_message(message)` | An answer of the app to `tessera::send()` (below). |
| `on_cards_closed()` | The cards closed: Back, standby, Back to page 1, another card. |
| `on_alert()` | An alert is about to show, a doorbell for example. |
| `on_touch()` | A tap the screen took, after the touch filter: a tile, a key, a button, a row of the settings, Back or the pager. It comes as the finger lets go, with the tap's action; for a click that answers the finger, take `on_press`. It runs inside the touch event, so start a sound there and never wait for it. A slider's release, a refused tap and the repeat of a held key do not count. |
| `on_press()` | A finger came down on something that takes a tap or a drag: a tile, a key, a slider, a row of the settings, the top bar (0.11). At once, before the screen knows whether it becomes a tap, a hold or a swipe: the moment for a click or a buzz that answers the finger. Not for the push that wakes a dimmed screen, nor between the tiles. Start a sound, never wait for one. |
| `on_step(step)` | A value under the finger stepped (0.11): a dragged slider, dial or seek bar passed a step (every 5 % of its range, or every position of one with 20 or fewer), or a key held down stepped once more. Once for every step, two at most for one move, as a detent clicks; not for the jump of a slider to where the finger lands. `step.direction` is +1 towards the larger end of the range, -1 towards the smaller, 0 when not known; `step.edge` says it reached either end, for a firmer click there. Play a click with `SoundPlayer::add()` (below), which spaces a run evenly. Every slider and dial of the screen says its steps, a plugin's own LVGL sliders on its cards too, without a line of code. |

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
   it). The tile's `tap` option set to "none" in the editor keeps taps away. `tessera::tap_point()` (0.10) says where
   the finger was, in the tile's own pixels (`0,0` the top left of `context.parent`), so a list knows which row, a
   row of posters which film; `-1,-1` when the screen could not say.
6. **Destructor**: the card shows something else. The core deletes the LVGL objects; free only memory of your own.

A change of the tile's size or options makes a new object: `create()` never has to handle a resize.

### `TileContext`

| Field | What |
|---|---|
| `parent` | The card's drawing area. Everything goes in here. |
| `width`, `height` | Its size in pixels, the card's padding already off. |
| `columns`, `rows` | The grid cells the tile covers (1x1, 2x1, ...). |
| `name` | The name the tile got in the editor, `""` for none. Valid during `create()` only: copy it. |
| `entity` | The Home Assistant entity the tile belongs to (a manifest tile with `domains`), `""` for none. Copy it. An entity of a domain Tessera draws no tile for needs the firmware after 0.56.0 ("A tile of an entity"). |
| `tile` | The tile's index in the layout: pass it to `tessera::open_card` so the card follows the tile. |
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

A tile with `domains` also gets its entity, and is sent again whenever that entity changes in Home Assistant:

```json
{ "state": "off", "name": "Waste", "attributes": { "message": "Paper", "start_time": 1791410400 } }
```

`attributes` holds only the ones the manifest's `attributes` names and the `fields` it takes out of them, a text
cut at 48 bytes. A list comes whole when the tile's data fits its 2.6 KB: two days of 96 prices
do. Lists that do not fit together are shortened, all to the same length, from the end. An
attribute whose name ends in `_at`, `_time` or `date` and holds a moment comes as seconds since 1970, so the tile says it
in the screen's words (`tessera::date_text`, `days_from_today`). `"wait": "wrong_entity"` means the entity chosen in the
editor is of another domain than the manifest names.

A tile without `data` and without `domains` gets `{}`, and so does a tile with `domains` before a person has chosen
its entity in the editor: say what to do (a word such as "Choose a sensor") until `state` is there. `state` is always
text as Home Assistant has it: `"1043.6"` for a sensor's number (read it with `atof` or `strtof`), or `"unavailable"`
and `"unknown"` while the entity has no value.

## A tile of an entity

In the manifest, `domains: [calendar]` and `attributes: [message, start_time]`. The editor then
offers the entities of those domains in the tile's inspector, also of a domain Tessera itself draws no tile for. To act on
the entity, as a tap on one of Tessera's tiles would:

```cpp
tessera::action("light.toggle", entity_);                                   // returns false when it was not sent
tessera::action("climate.set_temperature", entity_, "temperature", "21");
```

The screen must be allowed to perform actions, and the manifest names each action under
`permissions.home_assistant_actions`.

An entity of a domain Tessera draws no tile for (`plant`, `todo`, `calendar`, ...) works the same, with the firmware
after 0.56.0: up to 0.56.0 the tile gets that entity's state, name and attributes but an empty `entity`, and
`tessera::action` on it returns false. The fix is on Tessera's dev branch until its next release. To try it now, build
the preview with `TESSERA_REF=dev` ([PREVIEW.md](PREVIEW.md)) and a screen from the dev app (the repository URL with
`#dev` at the end). `tessera::send` to the app works with any entity on every firmware.

## A card

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

## A tap action

A tap action lets a person set one of Tessera's own tiles to do something of the plugin on a tap: a sensor tile that
opens the plugin's card. The manifest's `tap_actions` names the domains it is for; the editor offers it in the tap
choices of those tiles on a screen that runs the plugin. A hold still opens the tile's own card, and a screen without
the plugin does nothing on the tap.

```cpp
add_tap_action("upcoming", [this](const tessera::TapContext &c) {
  tessera::open_card(plugin_id(), "upcoming", c.entity, c.tile);   // c.entity, c.name, c.tile
});
```

## A top bar item

The manifest's `bar_items` lists them; a person places one on a page's top bar in the editor, under "From plugins".
The screen asks the plugin what it shows every few seconds and draws the bar again when that changed:

```cpp
add_bar_item("soon", [this]() {
  tessera::BarItem item;           // shown, icon (a codepoint of Tessera's set), text (a few words), tone
  if (tomorrow_) { item.shown = true; item.icon = 0xF044C; item.text = "Tomorrow: Paper"; }
  return item;
});
```

An item that is not shown takes no room. A screen without the plugin draws nothing for it.

**Its colour (0.8).** `item.tone` colours the icon by what it means, and the core picks the colour in both looks:
`Tone::NORMAL` the bar's own grey, `Tone::ACCENT` the screen's blue (it listens, it is on), `Tone::BUSY` Home
Assistant's amber (it works on something), `Tone::ALERT` its red (a camera that streams, something that went wrong),
`Tone::GOOD` its green (0.9: a level that is fine, a full battery).
The words keep the bar's colour. `ui::set_tone(label, tone, role)` gives a tile's icon the same colours (`role` for
`NORMAL`).

**Icon only.** A person can choose to show an item's icon without its words (Show, Icon only), so give every item an
icon that says it alone.

## Settings rows

Rows on the screen's own settings page (hold the top bar): Settings, then Plugins, then the plugin's page, drawn exactly
as Tessera's rows. Keep the values where Home Assistant sees them too: an ESPHome entity of `plugin.yaml` (a template
switch, number or select with `restore_value`, a text, a button). Name the same entities in the manifest's `settings`
and the editor shows them in the plugin's details on the screen's Plugins tab ([MANIFEST.md](MANIFEST.md), "Settings a
person changes"). These rows on the glass talk to the entities directly, so they work while Home Assistant is away.

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
most twelve rows. An action with `confirm` asks once, as Restart does. With `text` it says how it is going on its
right, read again every second while the page shows: a test that runs, a countdown. `.active(running)` after it
lights the row in the accent while `running` says so, the way a row that asks is lit; a tap on it is yours to
stop what runs.

```cpp
page.action(text("tone"), "\U000F0387", [this] { tone_ ? stop() : play_tone(); }, nullptr,
            [this]() -> std::string { return tone_ ? text("playing") : ""; })
    .active([this] { return tone_; });
```

## Questions to the app

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
most about 3 KB: longer texts are cut to 48 bytes, and lists shortened from the end only when it does not fit. An answer the manifest maps under `answers` comes as its fields: a day of prices as one
list of numbers instead of a list of objects ([MANIFEST.md](MANIFEST.md), "Answers").

## Sounds

A plugin that needs the feature `speaker` can bring sounds of its own (0.8): WAV files of 16-bit samples, mono or
stereo, in its component's folder. `smart_display.sound()` builds one into the firmware in `__init__.py`, and checks it
(a file that is not such a WAV stops the build with the reason); `tessera::SoundPlayer` from `plugin_sound.h` plays
them on `ts_speaker`, one at a time, at any rate the file has:

```python
from pathlib import Path
from esphome.components import smart_display, speaker

TICK = Path(__file__).parent / "sounds" / "tap.wav"
# in CONFIG_SCHEMA: cv.Required("speaker"): cv.use_id(speaker.Speaker), and a check that runs smart_display.read_sound(TICK)
# in to_code:
cg.add(var.set_speaker(await cg.get_variable(config["speaker"])))
cg.add(var.set_tick(smart_display.sound(config[CONF_ID], "tick", TICK)))
```

```cpp
#include "esphome/components/smart_display/plugin_sound.h"

tessera::Sound tick_;           // set_tick() keeps it
tessera::SoundPlayer sounds_;   // set_speaker() hands it the speaker

void loop() override { sounds_.loop(); }       // hands the speaker what it takes now, never waits
void on_press() override { sounds_.play(tick_); }  // plugin API 0.11; on_touch() before it
```

`play()` starts a sound in place of the one that plays, `stop()` ends it, `playing(sound)` says which one plays.
`add(sound, span_ms)` (0.11) plays a sound after the ones handed to it before, followed by silence until `span_ms` after
its start, so a run of clicks comes out evenly spaced however late the screen's loop comes round while it draws; two at
most wait, more are left out so a run never trails behind the finger. Tap sound plays each step with `add(step.edge ? tick_ : step_, 30)`. A
sound follows the screen's volume. On a board whose microphone and speaker share one bus (`AUDIO_HALF_DUPLEX`), a sound
waits while something listens, and the player drops it when the speaker took nothing of it for a second.

## Pictures on a tile

A tile can show pictures (0.10): a film's poster beside its title, a person's photo, a logo. The plugin lays out places
and names an address for each; the screen never fetches anything itself. Tessera's app fetches each picture once, cuts
or fits it to its place, and sends all the places of a tile as one picture, the way a camera tile or an album cover
reaches the screen; the screen keeps it as long as the tile is there and draws each place its part.

```cpp
void create(const tessera::TileContext &c) override {
  if (!ui::pictures()) return;                 // a board without the memory for pictures (the CYD) shows none
  for (int i = 0; i < 4; ++i) {
    poster_[i] = ui::picture(c.parent);        // a soft block until the picture is there
    lv_obj_set_size(poster_[i], w, h);
    lv_obj_set_pos(poster_[i], i * (w + gap), 0);
  }
}
void on_state(JsonObjectConst data) override {
  JsonArrayConst posters = data["posters"];    // the addresses, next to the titles
  for (int i = 0; i < 4; ++i)
    if (poster_[i]) ui::set_picture(poster_[i], posters[i] | "", tessera::Fit::COVER);
}
```

| Function | What |
|---|---|
| `ui::pictures()` | Whether this screen shows pictures at all. A board without the memory for them (the CYD, the Waveshare 3.5-inch, the Hosyond 4-inch) shows none: lay the tile out without the places then, as you would without a picture. |
| `ui::picture(parent)` | A place for a picture, sized and placed by you as any object (`lv_obj_set_size`, `lv_obj_set_pos`), in `create()`. A soft block (`theme::TRACK`) until its picture is there. Its corners are a key's; a radius of your own (`lv_obj_set_style_radius`) is taken over by the picture. |
| `ui::set_picture(place, url, fit)` | The picture a place shows. `Fit::COVER` fills the place and cuts away what is over (a poster in a poster's frame); `Fit::CONTAIN` shows the whole picture within it, on the card's own colour (a logo). The same address again does nothing, so call it on every `on_state`; another address asks the app again; `""` takes the picture away. |
| `ui::set_picture(place, url, fit, turn, mirror)` | The same, turned in perspective and with a reflection, as the covers of a cover flow. `Turn::LEFT` faces the left (its left edge far: a cover right of the middle one), `Turn::RIGHT` the right; the app draws the whole picture into the place, its near edge the place's height and its far edge 70 % of it. `mirror` puts a reflection under the picture that fades out: the place's lower part holds it, `MIRROR_PERCENT` (30) of the picture's height, so a place for a picture `h` high with its reflection is `h * 130 / 100` high. An older app draws the picture flat. |
| `ui::slide_pictures(dx, ms)` | Slides every place of the tile `dx` pixels sideways over `ms` (300 by default) and keeps them there until the tile's next picture is here, then they are back in their places with it, in one frame. Call it beside the `set_picture` calls of a change, in a call of the tile (`on_tap`, `on_tick`), so a row of posters that turns shows which way at once. |

Where a picture may come from, checked by the app for every address:

- a host the manifest names under `permissions.network` (`image.tmdb.org`), over https or http, as a fetch: public
  hosts only, no redirects, at most 2 MB a picture;
- for a tile of an entity (`domains`), a picture of Home Assistant's own: the entity's `entity_picture` when it starts
  with `/api/image/serve/`, `/api/image_proxy/` or `/local/` (a person's photo, an image entity), fetched with the
  app's own token.

The manifest's `pictures` of the tile says how many places it has at most ([MANIFEST.md](MANIFEST.md), "Tiles"); the
app paints no more. A place that lies outside the card, or has no size, gets no picture.

**Places may overlap.** They are the layers of the card: a place made later lies over one made earlier, and the app
paints them in that order, so a rounded corner or the transparent part of a turned picture shows what lies under it.
Make the places from the back to the front (the outermost covers of a cover flow first, the middle one last). Another
address keeps the picture a place shows until the next one is there, so a row of posters turns without a flash of
blocks. Pictures count in nothing the
layout memory counts: the screen keeps them in its picture store, within the same budget as the covers and cameras,
and a tile of eight posters is one picture there. A frame whose picture did not come stays the card's colour, and the
screen asks again later, each time after a longer while.

## Memory

A screen has two kinds of memory. Inside the chip there are a few tens of kilobytes, shared with Wi-Fi and the network
stack, and when they run out the screen loses Wi-Fi or restarts. A board with PSRAM (every board that shows pictures)
has megabytes beside it. A plain `std::string`, `std::vector` or `new` always takes the memory inside the chip: the
boards are built so that only what asks for PSRAM gets it.

So from plugin API 0.10:

- **Your tile and card objects are in PSRAM by themselves.** `tessera::Tile` and `tessera::Card` take PSRAM in their
  `operator new`, so what they hold directly (numbers, `char` arrays, pointers, a `Film films[16]`) is there already.
- **Keep texts and lists a tile holds as `tessera::Text` and `tessera::List<T>`.** They work as a `std::string` and a
  `std::vector` (`data()`, `size()`, `c_str()`, `+=`, `push_back`, `==` against another `Text` or a quoted text) and take
  PSRAM first, the memory inside the chip on a board without it. `roomy::plain(text)` makes a `std::string` of one for
  `ui::set_text`, `roomy::text(s)` the other way, `roomy::same(a, b)` compares the two kinds.
- **A short text you build to show is fine as a `std::string`** in a function: it is gone when the function returns.
- The core keeps your tile's options and data, and parses them for `on_state`, in PSRAM as well.

```cpp
struct Film { char title[52]; char poster[48]; float rating; };   // fixed fields: one block for the whole list
tessera::List<Film> films_;     // PSRAM
tessera::Text wait_;            // PSRAM
```

`tools/check.py` says so (a hint, not a failure) when a component of a plugin of API 0.10 or later keeps a member as a
`std::string` or `std::vector`. A plugin of an older API keeps building as it is.

## Drawing: `tessera::ui`

The screen's look is the same on every tile, so a plugin draws with the core's sizes, colours and fonts.

| Function | What |
|---|---|
| `ui::mm(n)` | `n` millimetres of glass in this board's pixels. For anything a finger touches (at least 7 mm). |
| `ui::px(n)` | A size of the reference look (a 4-inch Guition at 170 dpi) in this board's pixels. For paddings and gaps. |
| `ui::large()` | The standard look (4 inches and up), or the compact one (the 2.8-inch CYD). |
| `ui::font(Font)` | The screen's font. |
| `ui::line_height(Font)`, `ui::text_width(text, Font)` | To lay out before setting. |
| `ui::covers(text, Font)` (0.9) | Whether the font has every character of the text; a character it lacks draws as nothing. |
| `ui::label(parent, Font, Role)` | A one-line label, cut with dots when too long. From 0.9 it is one line of its font high, and `ui::set_font` keeps it so; before, give it that height yourself (`lv_obj_set_size(label, width, ui::line_height(font))`), or a long text wraps onto a second line under the tile. |
| `ui::set_text(label, text)`, `ui::set_font(label, Font)`, `ui::set_color(label, Role)`, `ui::set_tone(label, Tone, Role)` | Change it only when it differs: call them on every tick for free. |
| `ui::set_fill(block, Tone, Role)` (0.9) | A block's fill (a bar, a dot) in a tone's colour, or the role for `NORMAL`; only when it differs. |
| `ui::tone_color(Tone, Role)` (0.9) | A tone's colour in this look, for a part the two above do not reach. |
| `ui::text(parent, Font, Role)` | A label of several lines (0.10): wraps at its width, ends in dots at its height (two lines to start with; set your own). For a description, a title that may take two lines. |
| `ui::block(parent, Role)` | A rounded block with the radius of a key (a badge, a bar). |
| `ui::picture(parent)`, `ui::set_picture(place, url, fit)`, `ui::pictures()`, `ui::slide_pictures(dx, ms)` | Pictures on a tile (0.10), above. |
| `ui::color(Role)` | A colour by its role. |
| `ui::icon(codepoint)` | An icon of Tessera's set as text, for a label in `Font::ICON` or `ICON_SMALL`. |

Fonts (`tessera::Font`), largest first. Take the largest that fits.

| Font | Used for |
|---|---|
| `DIGITS` (0.9) | The largest: the digits of a climate card's setpoint (64 px on the standard look, 40 on the compact). Only `0`-`9`, `-`, `.`, `,`, `°` and `%`: take it where `ui::covers(value, Font::DIGITS)`, else step down to `VALUE`. It is a font every screen has already, so it costs no flash. |
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
| `theme::MARK_GREEN`, `MARK_AMBER`, `MARK_BLUE`, `MARK_PURPLE` | The four colours of the Tessera mark, the same in light and dark: for a dot or a block that names a kind (a bin, a line). |

A part can take a colour by what it means, in both looks: a tone (`tessera::Tone`). `GOOD` (0.9) is Home Assistant's
green, `BUSY` its amber, `ALERT` its red, `ACCENT` the look's accent, each made readable on the card. A label takes it
with `ui::set_tone(label, tone, normal)`, a block (a bar, a dot) with `ui::set_fill(block, tone, normal)` (0.9); `normal`
is the role for `NORMAL`. A level that is good, moderate or bad is `GOOD`, `BUSY` and `ALERT`. Never a colour as a
number. Before 0.9 a block has no colour by meaning: `TRACK` and `ACCENT`, or the mark's colours where a kind fits.

## The rest of the core

| Function | What |
|---|---|
| `tessera::epoch()` | The screen's clock, seconds since 1970; 0 until Home Assistant set it. |
| `tessera::local_time(epoch)` | That moment in the screen's time zone: `year, month, day, hour, minute, second, weekday`. |
| `tessera::clock_text(epoch)` | A time of day as the screen writes it (24 hours or AM/PM, as its settings say). |
| `tessera::format(text, n)` | `{n}` filled in; with `"1 day \| {n} days"` the form that fits `n` in the screen's language. |
| `tessera::fill(text, name, value)` | `{name}` filled in. |
| `tessera::refresh()` | Draw the plugin's cards again in the next pass, after a change outside `on_state` and `on_tick`. |
| `tessera::days_from_today(epoch)` | 0 today, 1 tomorrow, -1 yesterday, on the screen's calendar. `INT32_MIN` while the clock is not set. |
| `tessera::date_text(epoch)` | A day as the top bar writes it: "Fri 9 Oct", "vr 9 okt". |
| `tessera::days_text(days)` | "Tomorrow", "In 3 days" in the screen's language; `""` for today (bring your own word) and the past. |
| `tessera::action(service, entity, key, value)` | A Home Assistant action on an entity (see "A tile of an entity"). |
| `tessera::open_card(plugin_id, card, entity, tile, title)`, `close_card()` | Open or close a card. |
| `tessera::tap_point()` | Where the finger of the tile's `on_tap` was (0.10), in the tile's own pixels; `-1,-1` when unknown. Valid during `on_tap`. |
| `tessera::send(plugin, request)` | Ask the app (see "Questions to the app"). |

## Rules

`tools/check.py` refuses code that breaks the first six.

1. **No colour as a number** (`0xFF8800`, `lv_color_hex`): use a role.
2. **No font of your own**: use `tessera::Font`. Every font costs flash, and the 4 MB boards are close to full.
3. **No LVGL click handlers** (`LV_EVENT_CLICKED`, `LV_EVENT_PRESSED`): use `on_tap`, which has the touch filter in
   front of it.
4. **No timers, tasks or waits** (`lv_timer_create`, `xTaskCreate`, `delay`): use `on_tick` and `on_state`. A plugin
   that streams (sound to a speaker) may use its ESPHome component's own `loop()`, as long as nothing in it waits. A
   driver of hardware beside the plugin's own component, one that uses no plugin API and no LVGL, may run a task of
   its own the way ESPHome's drivers do (a camera captures on the other core); rules 1 to 4 are for the code that
   draws and takes taps, rules 5 and 6 for all of it.
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
  most of the API in one plugin: a tile of a calendar entity, a card that asks Home Assistant for the coming events, a
  tap action, a top bar item, two settings rows backed by ESPHome entities of `plugin.yaml`, and an input of kind entity.
- [`plugins/voice_assist/components/voice_assist/voice_assist.cpp`](../plugins/voice_assist/components/voice_assist/voice_assist.cpp):
  ESPHome's own components (`micro_wake_word`, `voice_assistant`) from `plugin.yaml` on the features it needs, a tile
  and a top bar item whose icon changes tone, settings rows and sounds of its own.
- [`plugins/screen_camera/`](../plugins/screen_camera): a board's hardware through a feature only a board brings
  (`camera_sensor`), a driver beside the plugin's own component, and a top bar item that follows the camera.
- [`plugins/tap_sound/components/tap_sound/tap_sound.cpp`](../plugins/tap_sound/components/tap_sound/tap_sound.cpp): the
  smallest plugin with sounds: a tick in `on_press` as a finger lands, a softer one in `on_step` for every step of a
  dragged slider.

## Not in the API yet

A plugin's own messages beyond Home Assistant commands, and pictures on a plugin's card over the page (places exist on
tiles, 0.10). A plugin can bring a camera (0.8): the feature `camera`, an ESPHome `esp_video_camera` with the id
`ts_camera` that Home Assistant shows as any ESPHome camera, made from a board's camera sensor (the feature
`camera_sensor`), as [`plugins/screen_camera/`](../plugins/screen_camera) does. Each will raise the minor.

## Versions

A manifest names the plugin API it was written for: `api: "0.8"`, `"0.9"`, `"0.10"` or `"0.11"`; the plugin API 0.11 is the one the core offers now (on Tessera's dev branch; the release has 0.10 until the next one). When a
screen is built, `smart_display.register_plugin()` compares it with the core's and stops the build with one sentence
when it does not fit. A plugin fits a core when all three hold:

1. the major is the same;
2. its minor is at most the core's;
3. no minor the core lists as a break lies after the plugin's own minor, up to the core's.

**From 1.0 on** a minor only adds, so the third rule never stops a plugin: only a break raises the major.

**While the major is 0** a minor may still change a name or a signature as the API settles. The core lists those
minors in `PLUGIN_API_BREAKS`, in `screen_manager/app/plugin_manifest.py` and in `components/smart_display/__init__.py`
(a test keeps the two equal). Today the list holds one: 0.4, where `Plugin::on_tick` became `on_interval`. So with a
core of 0.11:

| The manifest says | Builds? | Why |
|---|---|---|
| `api: "0.11"` | Yes | The core's own. |
| `api: "0.10"` | Yes | Same major, an older minor, and no break after 0.10. |
| `api: "0.9"` | Yes | Same major, an older minor, and no break after 0.9. |
| `api: "0.8"` | Yes | Same major, an older minor, and no break after 0.8. |
| `api: "0.7"` | Yes | Same major, an older minor, and no break after 0.7. |
| `api: "0.3"` | No | 0.4 changed a name it may use; the build says so in one sentence and asks to update the plugin. |
| `api: "0.12"` | No | Newer than the core; the build asks to update the plugin or the screen's firmware. |
| `api: "1.0"` | No | Another major. |

`tools/check.py` applies the same rules against the core it checks with (`api_fits` of `plugin_manifest.py`), so a
manifest that names an API this core cannot build fails there first. Tessera's own plugins move with every break in the
same release.

Name the lowest minor whose parts you use: the plugin then builds on every core from that minor on. What each minor
brought:

| Minor | What it brought |
|---|---|
| 0.1 | Tiles (`Tile`, `add_tile`, `TileContext`), the drawing helpers, the clock, and the plugin's moments: `on_ready`, `on_standby`, `before_update` and one every 250 ms. |
| 0.2 | Tiles of an entity (`TileContext::entity`, `tessera::action`), cards (`Card`, `add_card`, `open_card`, `close_card`), tap actions, top bar items, settings rows, questions to the app (`tessera::send`, `on_message`), date words such as `date_text` and `days_text`, `on_cards_closed` and `on_alert`. |
| 0.3 | `on_touch`, and a settings action that says how it is going (its `text`) with `.active()`. |
| 0.4 | A break: `Plugin::on_tick` (every 250 ms, with `millis()`) became `on_interval`, so that `on_tick` everywhere means once a second with the clock. |
| 0.5 | The app's side only: lists of an entity bounded by bytes instead of 16 items, a tile's `fields`, `answers`, the field kind `numbers`, `has_attributes`. |
| 0.6 | The app's side only: settings of kind text and button, a button's `status`, settings found by their name in the entity registry, and each plugin's settings in its details on the screen's Plugins tab. |
| 0.7 | The manifest's side only: `topics`, features (`provides`, `requires.features`), parts that need a feature, and plugins that come along with the one that needs them. |
| 0.8 | The features `camera` (`ts_camera`) and `camera_sensor`, the first feature only a board brings; a top bar item's `tone` and `ui::set_tone`; sounds of a plugin's own (`plugin_sound.h`, `smart_display.sound()`). |
| 0.9 | `Font::DIGITS`, the large digits of a climate card's setpoint, and `ui::covers()`; `Tone::GOOD`, `ui::set_fill()` and `ui::tone_color()`, the tones for a block; `ui::label` one line high. |
| 0.10 | Pictures on a tile: `ui::picture`, `ui::set_picture`, `ui::pictures`, `Fit`, `Turn` and a reflection, places that overlap, `ui::slide_pictures`, and the manifest's `pictures` of a tile; `ui::text`, a label of several lines; `tessera::tap_point()`, where a tile was tapped; memory in PSRAM: `tessera::Text`, `tessera::List`, and tiles and cards made there. |
| 0.11 | `on_press`, a finger landing on something that takes a tap or a drag, at once (for a click that answers the finger; `on_touch` comes as the finger lets go), `on_step(Step)`, a value under the finger stepping (a dragged slider, dial or seek bar, a held key), and `SoundPlayer::add()`, sounds evenly spaced one after another. |

A minor of the app's or the manifest's side only still counts: a plugin that uses what it brought needs an app that
knows it ([MANIFEST.md](MANIFEST.md), "Which minor brought a field").
