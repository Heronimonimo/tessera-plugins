# The manifest: tessera-plugin.yaml

The manifest tells the Tessera app what a plugin is, what it may do and what to show. It holds no code and no words:
names, labels and hints are keys into the plugin's `translations/<language>.json`, part `app`.

The app checks every manifest with the same code before it shows a plugin (`plugin_manifest.py` in the Tessera
repository; `tools/check.py` runs it). A field the app does not know is an error, so a typo shows up at once; a field
whose name starts with `x-` is ignored, for notes of your own.

## A complete example

```yaml
id: ov_departures
version: 1.0.0
api: "0.1"
icon: bus
maintainer: MaxGramser
license: MIT
requires:
  esphome: 2026.6.2
boards: any
flash_kb: 12

permissions:
  network: [v0.ovapi.nl]
attributes: [cloud]
privacy: https://github.com/MaxGramser/tessera-plugins/tree/main/plugins/ov_departures#privacy

tiles:
  - id: next
    name: tile_name
    icon: bus
    sizes: { min: 1x1, max: 3x2 }
    memory: 1200
    data: departures
    example: tile_example
    options:
      - { id: stop, kind: text, label: stop, hint: stop_hint }
      - { id: line, kind: choice, options_from: lines, label: line, hint: line_hint }
      - { id: walk, kind: number, min: 0, max: 30, step: 1, unit: min, default: 0, label: walk, hint: walk_hint }

fetch:
  - id: departures
    url: http://v0.ovapi.nl/tpc/{stop}
    every: 60s
    map:
      items: "$.{stop}.Passes[*]"
      fields:
        line: LinePublicNumber
        to: DestinationName50
        at: { path: ExpectedDepartureTime, as: epoch, tz: Europe/Amsterdam }
      where: { line: "{line}" }
      sort: at
      limit: 6
  - id: lines
    url: http://v0.ovapi.nl/tpc/{stop}
    every: 1h
    map:
      items: "$.{stop}.Passes[*]"
      value: LinePublicNumber
      label: [LinePublicNumber, DestinationName50]
```

## The fields

An id (of the plugin, a tile, an option, an input, a part or a fetch) is 1 to 32 characters of `a-z`, `0-9` and `_`,
starting with a letter.

### Who and what

| Field | Required | What |
|---|---|---|
| `id` | yes | The plugin's id. Unique in the index, the same as its folder and its component. |
| `version` | yes | Three numbers, `1.0.0`. Raise it for every change that reaches screens. |
| `api` | yes | The plugin API it was written for, in quotes: `"0.1"`. While the API is 0.x a plugin builds on its own minor only; from 1.0 on every core with the same major and at least that minor. |
| `icon` | yes | <a id="icon"></a>A Material Design Icons name from Tessera's icon set (`screen_manager/app/tile_icons.py` in the Tessera repository, such as `bus`, `train`, `calendar`, `thermometer`, `lightbulb`). The screen's icon font holds only that set. |
| `maintainer` | yes | The GitHub name of whoever looks after the plugin. |
| `license` | yes | An SPDX name that goes with AGPL-3.0: `MIT`, `Apache-2.0`, `BSD-2-Clause`, `BSD-3-Clause`, `ISC`, `MPL-2.0`, `LGPL-2.1-or-later`, `LGPL-3.0-or-later`, `GPL-3.0-or-later`, `GPL-3.0-only`, `AGPL-3.0-or-later`, `AGPL-3.0-only`, `Unlicense`, `0BSD`, `CC0-1.0`. |

The plugin's name and one-line summary are not fields: they are `name` and `summary` in `translations/en.json`, part
`app`, which every plugin must have.

### What it needs

| Field | Default | What |
|---|---|---|
| `requires.esphome` | none | The oldest ESPHome it builds with, such as `2026.6.2`. |
| `requires.psram` | `false` | `true` for a plugin that needs a board with PSRAM (not the CYD). |
| `requires.plugins` | `[]` | Plugins that must be on the screen first. |
| `boards` | `any` | `any`, or a list of board keys from Tessera's `boards.yaml` (`[cyd, guition]`) for a plugin with hardware of one board. |
| `flash_kb` | `0` | About how much flash it adds, in KB. The app uses it to say whether it fits a 4 MB board. |

### What it may do

The screen runs a plugin's code with every right the screen has, so a plugin names what it does. The app shows it
before anyone adds the plugin, and asks again when an update asks for more.

| Field | What |
|---|---|
| `permissions.network` | The hosts its fetches reach: names only, no address, nothing on a home network (`.local`, `192.168.x.x`). A fetch to any other host is refused. |
| `permissions.read_entities` | Home Assistant entities the screen itself reads (an ESPHome `homeassistant` sensor). |
| `permissions.home_assistant_actions` | Home Assistant actions the screen calls. |
| `attributes` | Any of `cloud` (uses a service outside the home), `commercial`, `ai-developed`, `experimental`. A plugin with `permissions.network` has `cloud`. |
| `privacy` | An https link to what the service sees. Required with `cloud`. A section of the README is fine. |

### What a person fills in: `inputs`

Asked when the plugin is added to a screen.

```yaml
inputs:
  - { id: api_key, kind: secret, scope: all, label: api_key, hint: api_key_hint }
  - { id: pin, kind: gpio, scope: screen, label: pin }
```

| Field | What |
|---|---|
| `kind` | `secret` (kept by the app, never shown again, never on the screen or in YAML; used in a fetch), `text`, or `gpio` (a free pin of the board). |
| `scope` | `all`: asked once for every screen. `screen`: per screen. Default: `all` for a secret, `screen` for the rest. |
| `label`, `hint` | Text keys. |

A `text` or `gpio` input reaches the screen's build as a substitution in capitals: input `pin` is `${PIN}` in
`plugin.yaml`. A `secret` never does. At most 8 inputs.

### Optional parts: `parts`

A part is an extra ESPHome file a person turns on per screen (large test tools, for example).

```yaml
parts:
  - { id: tests, file: parts/tests.yaml, label: tests, hint: tests_hint, flash_kb: 440, default: false }
```

### Tiles: `tiles`

| Field | Required | What |
|---|---|---|
| `id` | yes | The tile type's id; `add_tile("<id>", ...)` in the C++ uses the same. A layout calls it `plugin:<plugin>.<id>`. |
| `name` | yes | Text key: what the library and the inspector call it. |
| `icon` | no | From Tessera's icon set; the plugin's icon when left out. The editor shows it, and a screen without the plugin draws it on its placeholder. |
| `sizes` | yes | `{ min: 1x1, max: 2x2 }`, columns x rows. The editor offers the sizes in between that fit the screen's grid. |
| `memory` | yes | What one tile costs of the screen's layout memory, in bytes (64 to 16384). The screen and the app add it to the layout's budget. About 400 for a few labels, 1200 for a list. |
| `entity` | no | A Home Assistant domain (or a list) when the tile belongs to an entity. Not used by the app yet in API 0.1. |
| `data` | no | The id of the fetch whose answer the tile gets in `on_state`. |
| `example` | no | Text key: a line the editor shows on the tile's placeholder. |
| `options` | no | At most 12 options the inspector shows, below. |

An option:

| Field | What |
|---|---|
| `id`, `kind`, `label` | Required. `kind` is `text`, `choice`, `number` or `toggle`. |
| `hint` | Text key under the option. |
| `default` | The value when nothing is chosen. It must fit the option. |
| `choices` | For `choice`: `[{ value: "a", label: key }]`, at most 48. |
| `options_from` | For `choice`: the id of a fetch that fills the list (its `map` has `value` and `label`). |
| `min`, `max`, `step`, `unit` | For `number`: the range (both required), the step (default 1), a unit of up to 8 characters. |

A text option's value is at most 64 characters. The tile gets every option in `context.options` of `create()`, with the
defaults filled in. Options are also the placeholders of the tile's fetch: `{stop}` in a URL is the option `stop`.

### Fetches: `fetch`

At most 8. [FETCH.md](FETCH.md) explains them in full.

| Field | What |
|---|---|
| `id` | The fetch's id; a tile names it in `data`, an option in `options_from`. |
| `url` | `https://` (or `http://` for a fetch that carries no secret), a host from `permissions.network`, placeholders such as `{stop}` from the options of the tiles that use it and from the inputs. A secret may stand in the query, never in the path. |
| `headers` | Up to 8, with placeholders: `{ Authorization: "Bearer {api_key}" }`. |
| `every` | How old an answer may get: `30s`, `5m`, `1h`, `1d`. At least 30 seconds. |
| `map` | What of the answer reaches the screen. |

## Limits at a glance

| What | Most |
|---|---|
| Tiles per plugin | 8 |
| Options per tile | 12 |
| Inputs | 8 |
| Parts | 4 |
| Fetches | 8 |
| Fields per mapped item | 8 |
| Items in a mapped list | 48 |
| Bytes of a tile's data on the wire | about 2.6 KB (the last items are dropped to fit) |
| Text of a mapped field | 48 bytes |
| An answer of a web service | 64 KB, in 10 seconds |
