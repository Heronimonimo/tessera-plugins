# Screen camera

See the screen's own camera in Home Assistant. The screen becomes a camera of its device there: a picture on a
dashboard, a stream when you open it, and anything Home Assistant does with a camera (a snapshot in an automation, a
recording).

The picture is made on the screen in hardware, only while Home Assistant asks for one. While it does, a camera shows in
the screen's top bar, so whoever stands in front of it knows.

## What you need

- A screen whose board has a camera that Tessera knows how to power and reach: the reTerminal D1001, the M5Stack Tab5,
  or a Guition ESP32-P4 screen in its camera version ("C" in the model name): the JC8012P4A1 (also its V2 and V3),
  the JC1060P470 V2 and the JC4880P443. The Guition ones are new and tried on the JC8012P4A1 V3 only.
- Nothing else in Home Assistant: the camera comes with the screen's device in the ESPHome integration.

## Set up

1. In Tessera, open **Plugins**, choose **Screen camera** and tick your screens. Each screen builds once with it.
2. In Home Assistant, open the screen's device (Settings, Devices): its camera is there as **Camera**. Put it on a
   dashboard with a Picture entity or a Picture glance card.
3. Optional: add **Camera in use** to a page's top bar (Top bar, Add, From plugins). It shows while pictures leave the
   screen: "Live" for a stream, "Camera" for a picture now and then.

## Privacy

Nothing leaves the screen until Home Assistant asks for a picture, and every picture goes only to your Home Assistant.
Everyone who can open the screen's device or a dashboard with its camera can see through it. To stop that for a while,
disable the camera entity in Home Assistant; to stop it for good, remove the plugin from the screen.

## For makers

The camera is ESPHome's camera with id `ts_camera` (the feature `camera`), so another plugin can build on it. The
camera driver is `esp_video_camera` (github.com/n-IA-hane/esphome-esp-video-camera, from ESPHome pull request
esphome/esphome#16944), copied unchanged into `components/esp_video_camera` under the ESPHome License, with its NOTICE.
The OmniVision OV02C10 of the Guition screens, which Espressif's camera library has no driver for, is a component of its
own beside it, `components/ov02c10`: the sensor driver and its image tuning (one for rev3 chips, one for older ones),
with a NOTICE that says where they come from.
