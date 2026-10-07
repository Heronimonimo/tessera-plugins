# Instructions for AI assistants (and a summary for people)

You are helping someone make or change a plugin for Tessera, the ESP32 touch screens for Home Assistant. Read this
file first, then the docs in the order below. Everything here is checked by `tools/check.py`; run it before you say a
plugin is done.

## What a plugin is, in five sentences

1. A plugin is a folder: a manifest (`tessera-plugin.yaml`), an ESPHome package (`plugin.yaml`), an ESPHome component
   in C++ (`components/<id>/`), texts (`translations/<language>.json`) and a `README.md`.
2. The screen builds the component into its firmware; the component registers tile types with the core through
   `tessera::Plugin` (`esphome/components/smart_display/plugin_api.h` in the Tessera repository).
3. The Tessera app (in Home Assistant) reads only the manifest: it shows the plugin, its options and its README, and
   carries out the plugin's `fetch` (JSON from a web service) with its own code.
4. A tile of a plugin is `plugin:<plugin id>.<tile id>` in a screen's layout. The core gives it a card's drawing area
   while its page is on the glass, hands it the data the app sent (`on_state`), and ticks it once a second (`on_tick`).
5. The plugin API is version 0.1. The manifest says `api: "0.1"`; a plugin builds on that version only.

## Read in this order

1. [docs/MAKING_A_PLUGIN.md](docs/MAKING_A_PLUGIN.md): the files, the steps, trying it on a screen.
2. [docs/MANIFEST.md](docs/MANIFEST.md): every field of `tessera-plugin.yaml` and its limits.
3. [docs/FIRMWARE_API.md](docs/FIRMWARE_API.md): the C++ API, the life of a tile, the drawing rules.
4. [docs/FETCH.md](docs/FETCH.md): data from a web service, the map language, the limits.
5. [docs/TRANSLATIONS.md](docs/TRANSLATIONS.md), [docs/TESTING.md](docs/TESTING.md),
   [docs/PUBLISHING.md](docs/PUBLISHING.md), [docs/LIMITS.md](docs/LIMITS.md) when you need them.

The two plugins to copy from: [`template/`](template) (a tile without data, the smallest complete plugin) and
[`plugins/ov_departures/`](plugins/ov_departures) (a tile with data from a web service, a list of choices and a
countdown).

## Rules that are never optional

- **Start from the template**: `python3 tools/new_plugin.py <id>`. Do not write the folder from memory.
- **The id is the same everywhere**: the folder in `plugins/`, `id:` in the manifest, the component folder
  `components/<id>/`, the YAML key in `plugin.yaml` and the C++ namespace `esphome::<id>`. Lowercase letters, digits
  and `_`, starting with a letter, at most 32.
- **No words in code or manifest.** Every name, label and hint in the manifest is a key into
  `translations/en.json`, part `app`. Every word on the screen is a key of part `screen`, read with
  `plugin->text("key")`. English is complete; other languages may lack keys.
- **Draw like the rest of the screen**: sizes with `tessera::ui::mm()` / `ui::px()`, colours with a theme role
  (`theme::INK`, `theme::MUTED`, `theme::ACCENT`, ...), only the screen's fonts (`tessera::Font`). Never a colour as a
  number, never a font of your own, never an LVGL click handler (use `Tile::on_tap`), never a timer or task (use
  `on_tick`), never a network connection from the screen (data comes through `fetch`).
- **Nothing exists off the glass.** Make every LVGL object in `create()` inside `context.parent`. The core deletes them;
  a destructor only frees your own memory.
- **Paint, do not rebuild.** `on_state` and `on_tick` set texts and positions with `ui::set_text`, `ui::set_font`,
  `ui::set_color`, which change a property only when it differs.
- **Name what the plugin may do** in `permissions`: every host a fetch reaches in `network`, every Home Assistant entity
  the screen reads in `read_entities`, every action it calls in `home_assistant_actions`. A plugin with `network` has
  the attribute `cloud` and a `privacy` link.
- **Memory**: the manifest's `memory` for a tile is what one tile costs in the screen's layout memory. Keep it honest:
  a few hundred bytes for a handful of labels, about 1200 for a list.
- **Secrets stay in the app.** An API key is an input of `kind: secret`; it goes into a fetch's header or query, never
  into the URL path, never to the screen, never into YAML.
- **Plain English in READMEs**, with a `## Set up` section of numbered steps a person can follow in the Tessera app.

## When you are done

```sh
python3 tools/check.py plugins/<id>     # must print "<id>: ok"
```

Then build it on a real screen ([docs/TESTING.md](docs/TESTING.md)). A plugin that passes the check but was never on a
screen is not done: say so, and say which board you did not try.
