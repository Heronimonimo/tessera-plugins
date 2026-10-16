# Changelog

## 0.2.1 - 2026-10-16
- Builds with the current firmware again: a screen that still had the first 0.2.0 failed its update with
  "on_step marked override, but does not override". Update the plugin, then the screen.

## 0.2.0 - 2026-10-15
- The tick comes the moment your finger lands on a tile, a key, a slider or a row of the settings, no longer after you
  let go. A push that wakes a dimmed screen and a touch between the tiles stay silent.
- Dragging a slider ticks softly at every step, evenly however fast you drag: every 5 %, or every position of a slider
  with only a few. The end of a slider ticks firmer, and holding - or + ticks at every step.
- Needs Tessera's firmware with plugin API 0.11.

## 0.1.0 - 2026-10-10
- First version: a short, soft tick on the screen's speaker for every tap, with a setting to turn it off.
