# Changelog

## 0.4.0 - 2026-10-19
- New, off until you switch it on: **Light level** (lux, an estimate) and **Motion** on the screen's device in Home
  Assistant, worked out from the camera on the screen itself: no picture leaves the screen for them. Switch on
  **Light level and motion** in the plugin's settings; the camera then keeps running. Optional top bar item
  **Camera senses motion**: an eye while it does. Switched off, the light level reads unknown.

## 0.3.0 - 2026-10-19
- Also the camera of the Guition ESP32-P4 screens (the OmniVision OV02C10 of the JC8012P4A1, JC1060P470 V2 and
  JC4880P443 in their camera version), on a Tessera whose board names it. Tried on the JC8012P4A1 V3.

## 0.2.1 - 2026-10-19
- Sharper pictures: they were compressed far too much (JPEG quality 12 of 100, now 80). Each picture is larger,
  about 150 to 300 KB.

## 0.2.0 - 2026-10-10
- Also on a board that clocks its camera from a pin, such as the M5Stack Tab5.

## 0.1.0 - 2026-10-10
- First version: the screen's own camera as a camera in Home Assistant, and a camera in the top bar while it is watched.
