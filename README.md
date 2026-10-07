# Tessera plugins

Plugins add something to a [Tessera](https://github.com/MaxGramser/homeassistant_espscreen) touch screen that not
everyone needs: a tile with the next bus, hardware on one board, a feature of your own. A plugin is built into the
firmware of the screens you choose, and the Tessera app in Home Assistant gives its tiles their data.

This repository holds:

| Folder | What |
|---|---|
| [`plugins/`](plugins) | The plugins Tessera ships, one folder each. Every folder is a complete plugin. |
| [`template/`](template) | A small plugin that works, to start your own from. |
| [`docs/`](docs) | How plugins work and how to make one, step by step. |
| [`tools/`](tools) | `check.py` (check a plugin), `new_plugin.py` (start one), `build_index.py` (write the index). |
| `index.json` | The list the Tessera app reads. Written by `tools/build_index.py`, never by hand. |
| `blocked.yaml` | Plugin versions the app refuses to build. |

## The plugins

| Plugin | What it does | Boards |
|---|---|---|
| [Public transport (NL)](plugins/ov_departures) | The next bus, tram, metro or ferry from your stop, live from OVapi, counted down on the screen. | All |
| [Waste collection](plugins/waste_collection) | When the next bin goes out, from a calendar in Home Assistant, on a tile, a card and in the top bar. | All |
| [P4 panel audio](plugins/p4_audio) | The speaker and microphone of the Waveshare ESP32-P4 86 panel: a click on every tap, a volume, tests on the screen. | Waveshare ESP32-P4 86 panel |

## Using a plugin

1. Open Tessera in Home Assistant and go to **Plugins**, or to the **Plugins** tab of a screen.
2. Open a plugin, read what it does and what it may do, tick the screens it goes on, and apply. Each screen builds its
   firmware once with the plugin in it.
3. In **Layout**, place the plugin's tile from the library's **Plugins** group and set its options in the inspector.

Plugins are new and only in the dev version of the app for now (the app added with `#dev` at the end of the repository
URL, or a local copy). The plugin API is version 0.1: it can still change, and a plugin names the version it was
written for.

## Making a plugin

```sh
git clone https://github.com/MaxGramser/tessera-plugins
cd tessera-plugins
python3 tools/new_plugin.py my_idea          # plugins/my_idea/, copied from template/
python3 tools/check.py plugins/my_idea       # the same check the app does
```

Then read [docs/MAKING_A_PLUGIN.md](docs/MAKING_A_PLUGIN.md). It goes through every file, how to try the plugin on
your own screen before anyone else sees it, and how it gets into the list.

A plugin can also live in a repository of your own. List it once with a pull request that adds a four-line file to
`community/`; after that every release you publish reaches the app by itself within the hour, without a pull request
here ([docs/PUBLISHING.md](docs/PUBLISHING.md)).

Working with an AI assistant? Point it at [AGENTS.md](AGENTS.md) first: it has the rules and the order to read the
docs in.

## What a plugin can and cannot do

- **On the screen** a plugin is an ESPHome component written in C++. It draws its own tiles with the screen's fonts,
  colours and sizes, and hears about the screen's moments (start, a tick, standby, an update).
- **In the app** a plugin is only a description. The app reads its manifest and carries out what it asks with its own
  code: fetch JSON from a web service it names, keep a secret key, show its options and its README. The app never runs
  a plugin's code.
- A plugin draws only its own tiles. It cannot change how other tiles look, block taps, or reach your home network
  through the app.

[docs/LIMITS.md](docs/LIMITS.md) has the full list, with the way around where there is one.

## Licence

The tools, the template and Tessera's own plugins are MIT ([LICENSE](LICENSE)). A plugin is built into firmware that
is AGPL-3.0, so every plugin's licence must go with AGPL-3.0 ([docs/MANIFEST.md](docs/MANIFEST.md), "license").
