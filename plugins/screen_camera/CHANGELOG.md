# Changelog

## 0.3.2 - 2026-10-20
- Truer colours on the M5Stack Tab5 and the reTerminal D1001: black is black again. Their camera sent black as a dark
  grey that the screen's chip could not take off, so dark parts of the picture looked pink and the whole picture
  looked faded. The plugin now sets the sensor's own black level.

## 0.3.1 - 2026-10-20
- **Motion wakes the screen** (off to start with) and **Motion time-out** (5 seconds to 10 minutes, 10 to start
  with) are settings now.
- Tessera's own dot in the top right corner of the screen now shows while the camera is in use, also while it senses
  light and motion; the plugin's top bar items **Camera in use** and **Camera senses motion** are gone. Needs Tessera
  with plugin API 0.17.

## 0.3.0 - 2026-10-20
- Sharper pictures: they were compressed far too much (JPEG quality 12 of 100, now 80). Each picture is larger,
  about 150 to 300 KB.
- Also the camera of the Guition ESP32-P4 screens (the OmniVision OV02C10 of the JC8012P4A1, JC1060P470 V2 and
  JC4880P443 in their camera version), on a Tessera whose board names it. Tried on the JC8012P4A1 V3.
- New, off until you switch it on: **Light level** (lux, an estimate) and **Motion** on the screen's device in Home
  Assistant, worked out from the camera on the screen itself: no picture leaves the screen for them. Switch on
  **Light level and motion** in the plugin's settings; the camera then keeps running. Switched off, the light level
  reads unknown.

## 0.2.0 - 2026-10-10
- Also on a board that clocks its camera from a pin, such as the M5Stack Tab5.

## 0.1.0 - 2026-10-10
- First version: the screen's own camera as a camera in Home Assistant, and a camera in the top bar while it is watched.
