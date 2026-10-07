# What a plugin can and cannot do

## Works

| Idea | How |
|---|---|
| The next bus from your stop, with your line chosen in the editor | A tile, a `fetch`, an option with `options_from` (the `ov_departures` plugin). |
| Today's waste collection from your municipality's API | A tile and a `fetch`. |
| Hourly energy prices of your own supplier | A `fetch` (up to 48 items) and a tile that draws them. |
| A countdown to a date | A tile on the screen's clock alone (the template). |
| A sensor or a relay on the board's free pins | An ESPHome component with `inputs` of kind `gpio`; anything ESPHome can do. |
| Waking the screen when someone stands in front of it | A plugin with a radar sensor and `on_tick`. |

## Works with a way around

| Idea | Way around |
|---|---|
| An API that needs OAuth | A Home Assistant integration that offers the data as an entity; then a normal tile. |
| An API that gives XML or GTFS-realtime | The Home Assistant integration for it. |
| Messages from outside (a webhook, a bot) | A webhook in Home Assistant that sets an entity. |

## Cannot, on purpose

| Idea | Why |
|---|---|
| Change how all light tiles look | A plugin draws only its own tiles. A better card for everyone belongs in Tessera itself. |
| Block taps (a child lock) or scroll freely | Touch and navigation stay with the core. |
| Run code in the Tessera app or the editor | The app holds the key to all of Home Assistant; it runs only its own code. |
| Reach your home network through the app | A fetch goes only to public hosts the manifest names. |
| Fetch from the screen itself | The app does it once for every screen, without holding up drawing. |
| Change without a build | A plugin is part of the firmware: every change builds the screen. |
| Live video | Pictures reach a screen as snapshots. |

## Cannot, yet

Planned for later versions of the plugin API: a settings page on the screen, a card of its own (a full screen), items
in the top bar, a tap action for existing tiles, tiles that belong to a Home Assistant entity, and messages between the
screen and the app.

## Physical limits

- **Flash**: a 4 MB board (the CYD) has little room left; the app shows how much a plugin takes there (`flash_kb`).
- **Memory**: every tile counts in the screen's layout memory (`memory`).
- **Time**: the screen draws, takes taps and runs plugins in one loop; `on_tick` and `on_state` must be quick.
