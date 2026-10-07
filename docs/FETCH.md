# Data from a web service: fetch

A screen never fetches anything from the internet itself: its HTTP client would hold up drawing and taps, and a key
does not belong in firmware. Instead the plugin's manifest describes a fetch, the Tessera app in Home Assistant carries
it out with its own code, and the screen gets only the fields the manifest maps.

## What the app does

- It asks once per distinct question (the URL and headers with every value filled in), for every tile and every screen
  together. Two tiles with the same stop share one ask; so do the departures and the list of lines of the bus plugin,
  which read the same URL.
- It asks again when the answer is older than the fetch's `every`, and only while a tile on some screen uses it (or the
  editor asks for a list of choices).
- When the answer changed, it sends the tiles that show it their new data. The screen draws them the next time they
  are on the glass.
- When the service fails, the tiles keep the last good answer with `"stale": true`, and the app waits 1, 2, 5 and then
  15 minutes before it asks again.

## The rules

- **Hosts**: only those in `permissions.network`, by name. Never an address, never a name on the home network, and the
  address a name resolves to is checked again when the app connects, so a DNS answer cannot point it at a router or at
  Home Assistant.
- **https**, or plain http only for a fetch without a secret in it.
- **No redirects** are followed.
- **At most 64 KB** of answer, in **10 seconds**, and it must be **JSON**.
- **Every 30 seconds** at the most often.
- **Secrets** (`inputs` of kind `secret`) go into a header or the query, never the URL path, and never appear in a log,
  in the editor or on a screen.

## Placeholders

A URL and the headers can hold `{name}`:

- an option of a tile that uses the fetch (`{stop}` is the tile's option `stop`);
- an input of the plugin (`{api_key}`), with `scope: all`.

A value in the URL is percent-encoded. A placeholder with no value means no ask: the tile gets
`{"wait": "not_filled"}`.

```yaml
fetch:
  - id: prices
    url: https://api.example.org/v1/prices?area={area}
    headers: { Authorization: "Bearer {api_key}" }
    every: 15m
    map: ...
```

## The map

`map` says what of the answer reaches the screen. Everything else is dropped in the app.

### Paths

A small path language, on purpose without filters or expressions:

| Step | Means |
|---|---|
| `$` | The whole answer (a path in `items` starts with it). |
| `.name` | A field of an object. |
| `.{option}` | A field named by an option's value (OVapi keys its answer by the stop code). |
| `[*]` | Every item of a list, or every value of an object. |
| `[3]` | One item of a list. |
| `[:5]` | The first five items. |

A field path (inside `fields`, `value`, `label`) starts at an item, without `$`: `name`, `departure.time`, `stops[0]`.

### A list: `items` with `fields`

```yaml
map:
  items: "$.{stop}.Passes[*]"          # where the list is
  fields:                               # each item becomes { line, to, at }
    line: LinePublicNumber
    to: DestinationName50
    at: { path: ExpectedDepartureTime, as: epoch, tz: Europe/Amsterdam }
  where: { line: "{line}" }             # keep only items whose field equals; an empty option keeps all
  skip: { state: [PASSED, CANCEL] }     # drop items whose field is one of these
  sort: at                              # soonest first (-at for the other way round)
  limit: 6                              # at most this many (48 at the most)
```

The screen gets `{"items": [{"line": "15", "to": "Station Sloterdijk", "at": 1791386619}, ...]}`.

### One object: `fields` alone

```yaml
map:
  fields:
    price: { path: current.price, as: number }
    unit: current.unit
```

The screen gets `{"price": 0.23, "unit": "EUR/kWh"}`.

### A field

A field is a path, or `{ path, as, tz }`:

| `as` | The screen gets |
|---|---|
| `text` (default) | Text, at most 48 bytes. |
| `number` | A number (a text such as `"0,23"` is read as one). |
| `epoch` | Seconds since 1970, from a number (seconds or milliseconds) or an ISO 8601 time. A time without a zone is read in `tz` (`Europe/Amsterdam`), or in Home Assistant's time zone without one. |

At most 8 fields. A field that is missing in an item is `null`.

### A list of choices: `value` and `label`

For an option `{ kind: choice, options_from: <fetch> }`: the editor's inspector shows a list filled by the app.

```yaml
map:
  items: "$.{stop}.Passes[*]"
  value: LinePublicNumber                          # what the option stores
  label: [LinePublicNumber, DestinationName50]     # what the list shows, joined with " · "
```

Each value appears once, at most 48 of them. The editor asks for the list with the tile's other options filled in (the
stop of the bus tile), so a list can depend on them.

## Size on the screen

A tile's data travels in one message of at most 4 KB, with the rest of the tile. The app keeps the data under about
2.6 KB by dropping the last items of a list. Ask only for the fields the tile shows, and set `limit` to what fits on
the largest size of your tile: the bus plugin asks for six departures.

## When a service is not JSON, or needs a login

- An XML or binary service (GTFS-realtime): use the Home Assistant integration for it, and a normal tile, or wait for
  tiles that belong to an entity (a later plugin API).
- OAuth: `fetch` knows fixed keys only. A Home Assistant integration that offers the data as an entity is the way.
- Data that only exists inside Home Assistant: a normal tile shows any entity; no plugin needed.

## Trying a map

The app's code is in the Tessera repository (`screen_manager/app/plugin_fetch.py`). Without a screen:

```python
import json, urllib.request, yaml, sys
sys.path.insert(0, 'path/to/homeassistant_espscreen/screen_manager/app')
import plugin_manifest, plugin_fetch
manifest = plugin_manifest.check(yaml.safe_load(open('tessera-plugin.yaml')))
fetch = manifest['fetch'][0]
data = json.load(urllib.request.urlopen('http://v0.ovapi.nl/tpc/30003025'))
print(plugin_fetch.apply_map(fetch['map'], data, {'stop': '30003025', 'line': ''}))
```
