# Making a plugin

This page takes you from an idea to a plugin on your own screen. It assumes you know a little C++ and YAML; you do not
need to know Tessera's code.

## The idea first

A plugin is right for something that not every screen needs and that the screen can draw on its own:

- a tile with data from a web service (the next bus, today's waste collection, the energy price of your supplier);
- a tile that only needs the screen's clock (a countdown, a timer of your own);
- hardware on the board or on its free pins (a sensor, a relay, a speaker).

It is the wrong tool for:

- something Home Assistant already has as an entity: a normal tile shows it, no plugin needed;
- a better card for a kind of entity everyone has (lights, thermostats): that belongs in Tessera itself, as a pull
  request on the main repository;
- anything listed in [LIMITS.md](LIMITS.md).

## The files

```
plugins/my_idea/
  tessera-plugin.yaml          the manifest: id, version, what it may do, its tiles, its fetches
  plugin.yaml                  what a screen gets: the component, configured
  components/my_idea/
    __init__.py                the ESPHome side: schema and code generation (a few lines)
    my_idea.h                  the plugin class
    my_idea.cpp                the tiles: how they draw
  translations/
    en.json                    every word: part "screen" (on the glass) and part "app" (in the editor)
    nl.json                    more languages, optional
  README.md                    what it does and how to set it up; the app shows it
  README.nl.md                 optional, per language
```

Every file has a counterpart in [`template/`](../template); read it next to this page.

## Step by step

### 1. Copy the template

```sh
python3 tools/new_plugin.py my_idea
```

This makes `plugins/my_idea/` with every name changed. For a plugin in a repository of your own, give a folder:
`python3 tools/new_plugin.py my_idea ../tessera-my-idea`.

### 2. Describe it in the manifest

Open `tessera-plugin.yaml`. Set the `version` (start at `0.1.0`), `maintainer` (your GitHub name), `icon` (a name from
Tessera's icon set, see [MANIFEST.md](MANIFEST.md#icon)) and describe your tile under `tiles`:

```yaml
tiles:
  - id: next
    name: tile_name                 # a key in translations/en.json, part "app"
    icon: bus
    sizes: { min: 1x1, max: 2x2 }   # the sizes the editor offers
    memory: 900                     # what one tile costs of the screen's layout memory
    data: departures                # the fetch whose answer the tile gets (leave out for none)
    options:                        # what the inspector shows
      - { id: stop, kind: text, label: stop, hint: stop_hint }
```

[MANIFEST.md](MANIFEST.md) lists every field. If the tile needs data from the internet, add a `fetch`
([FETCH.md](FETCH.md)).

### 3. Write the words

Every key you used goes into `translations/en.json`:

```json
{
  "_meta": { "name": "English" },
  "screen": { "now": "now", "minutes": "{n} min" },
  "app": { "name": "Next bus", "summary": "When the next bus leaves.", "tile_name": "Next bus", "stop": "Stop" }
}
```

`app` needs `name` and `summary` plus every key the manifest names. `screen` is yours: what the C++ shows.
[TRANSLATIONS.md](TRANSLATIONS.md) has the rules for plurals and placeholders.

### 4. Draw the tile

In `components/my_idea/my_idea.cpp` a tile is a class with four moments:

```cpp
class NextTile : public tessera::Tile {
 public:
  void create(const tessera::TileContext &c) override;   // make the labels, once
  void on_state(JsonObjectConst data) override;          // the app sent new data
  void on_tick(uint32_t epoch) override;                 // once a second: set the texts
  void on_theme() override;                              // light or dark: set the colours
};
```

and the plugin registers it:

```cpp
void MyIdea::setup() { add_tile("next", [this]() { return new NextTile(this); }); }
```

[FIRMWARE_API.md](FIRMWARE_API.md) has the whole API and the rules for drawing.

### 5. Check it

```sh
python3 tools/check.py plugins/my_idea
```

It runs the Tessera app's own manifest check and the rules for the C++. Fix what it says until it prints `ok`.

### 6. Put it on your screen

Copy the plugin's folder into Home Assistant's config, next to the `esphome` folder:

```
/config/tessera-plugins/my_idea/      (the folder as it is in plugins/my_idea/)
```

Open Tessera, go to Plugins: your plugin is there with the label **Test**. Add it to a screen; the app writes the
screen's plugins file and builds the screen. Place its tile in Layout. After every change in the folder, open the
plugin in the screen's Plugins tab and press **Build again**. [TESTING.md](TESTING.md) has more ways to try it,
including a build on your own computer.

### 7. Publish it

See [PUBLISHING.md](PUBLISHING.md): a plugin in this repository through a pull request, or in a repository of your own
listed in the index.

## How the pieces meet

```
Home Assistant config                               Tessera app (in Home Assistant)
  esphome/kitchen.yaml        <- packages:            reads tessera-plugin.yaml (index or folder)
    tessera_plugins: !include kitchen.plugins.yaml    writes kitchen.plugins.yaml
  esphome/kitchen.plugins.yaml                        runs the plugin's fetches, sends each tile its data
     packages: plugin_my_idea  -> plugin.yaml
     external_components       -> components/         ESPHome builds the screen with the plugin in it
                                                       the screen's hello says: plugins [my_idea]
Screen
  core (smart_display)  <- tessera::Plugin registers tile "next"
  layout tile "plugin:my_idea.next" -> core makes a NextTile in that cell, hands it its data, ticks it
```

- The screen's own YAML gets one line under `packages:` once, and every build of that screen (the app's, ESPHome
  Device Builder's, a computer that shares the folder) builds the same plugins.
- The plugins file pins a plugin from the index to one commit. A new release of the plugin reaches a screen only when
  someone updates it in the app.
- A screen that does not have a plugin draws its tiles as a plain card with the tile's name and "Plugin missing". It
  never fails or restarts over it.

## Names a plugin.yaml may use

`plugin.yaml` is merged into the screen's own configuration, so it can point at parts the board already has. Only these
names stay the same on every board and in every update (Tessera's docs/PROFILES.md, "What an override may rely on"):

| Name | What |
|---|---|
| `my_display` | the display |
| `ts_touch` | the touch panel |
| `gpio_backlight_pwm`, `back_light` | the output that drives the backlight, and the light on it |
| `touch_bus` | the I2C bus the touch panel is on, on every board that has one (the M5Stack Tab5: `tab5_bus`) |

A chip on the touch panel's bus (the audio codecs of the Waveshare P4 panel, in `plugins/p4_audio/plugin.yaml`) takes
`i2c_id: touch_bus`. Any other id of a board file can change in an update; give your own parts ids that start with your
plugin's id (`p4_audio_amp`), so they never meet one of Tessera's.

## Common mistakes

| What happens | Why | Fix |
|---|---|---|
| The build says "No tessera-plugin.yaml above ..." | The component is not in `components/<id>/` next to the manifest. | Keep the folder layout of the template. |
| The build says the plugin wants another plugin API | `api:` in the manifest is not the screen's. | Use the API the core offers (`0.3` now). |
| The tile shows "Plugin missing" | The screen was not built with the plugin, or the tile id differs from `add_tile("...")`. | Build again; make the ids match. |
| The tile stays empty | `on_state` got `{"wait": ...}`: the fetch is not filled in or failed. | Show the reason (see the bus plugin); check the options. |
| Text cut with dots | The label is wider than its room. | Take a smaller font from `tessera::Font`, or give the label more width. |
| Squares instead of letters or icons | A character or icon the screen's fonts do not have. | Use plain text, and icons from Tessera's set only. |
