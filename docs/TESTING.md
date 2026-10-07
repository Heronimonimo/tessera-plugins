# Testing a plugin

Three layers, from quick to real. A plugin is done when it ran on a real screen.

## 1. The check (seconds)

```sh
python3 tools/check.py plugins/my_idea
```

It runs the Tessera app's own manifest check (`plugin_manifest.py`, downloaded from the Tessera repository into
`tools/.cache/`) and the rules the app cannot see: the files, the translations, the README and the code rules of
[FIRMWARE_API.md](FIRMWARE_API.md). Working on Tessera itself? Point it at your checkout:
`TESSERA_MANIFEST=../homeassistant_espscreen/screen_manager/app/plugin_manifest.py python3 tools/check.py`.

## 2. A test folder in Home Assistant (minutes)

The quickest way onto a real screen, with nothing published:

1. Copy the plugin's folder into Home Assistant's config, beside the `esphome` folder, under its id:

   ```
   /config/tessera-plugins/my_idea/tessera-plugin.yaml
   /config/tessera-plugins/my_idea/plugin.yaml
   /config/tessera-plugins/my_idea/components/my_idea/...
   ```

   The Samba share, the File editor or `scp` all work.
2. Open Tessera, go to Plugins. The plugin is there with the label **Test**. A folder that is not a valid plugin shows
   what is wrong with it under the list.
3. Add it to a screen. The app writes the screen's plugins file with the folder in it and builds the screen; the
   build's log is under Firmware & USB.
4. Place the tile in Layout and set its options.
5. After a change in the folder, open the plugin in the screen's Plugins tab and press **Build again**.

A test folder has no version check and no updates: every build takes what the folder holds.

## 3. A build on your own computer

The screen's YAML and its plugins file build anywhere ESPHome runs:

```yaml
# kitchen.yaml, the screen's own YAML (Tessera writes it; the line under packages is added with the first plugin)
packages:
  display:
    url: https://github.com/MaxGramser/homeassistant_espscreen
    ref: dev
    files: [packages/guition.yaml]
  tessera_plugins: !include kitchen.plugins.yaml
```

```yaml
# kitchen.plugins.yaml, written by Tessera; for a plugin you are working on, point it at your folder:
packages:
  plugin_my_idea: !include ../tessera-plugins/my_idea/plugin.yaml
external_components:
  - source: { type: local, path: ../tessera-plugins/my_idea/components }
```

```sh
esphome run kitchen.yaml
```

Building with plugins needs a core with the plugin API: Tessera's `dev` branch until it is released.

## What to look at on the glass

- Every size the tile offers (`sizes` in the manifest), on the smallest screen you have (a CYD if you can) and a large
  one.
- Light and dark (the screen's settings page), so `on_theme` is right.
- Every waiting state: no option filled in, the service down (a wrong host in a copy of the manifest), the first
  seconds after a start.
- A page with the tile, then another page and back: the tile shows the right data at once.
- Memory: the editor's Layout shows how full the screen's memory is; a tile's `memory` that is too low makes that number
  wrong.

## Reporting a problem with a plugin

Open an issue on the plugin's repository, with the board, the screen's firmware version (Screen settings), the plugin's
version and what the screen shows. The screen's log (ESPHome's logs) says when a plugin's tile is made and what it got.
