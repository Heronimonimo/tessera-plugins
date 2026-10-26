# Changelog

## 0.6.3 - 2026-10-26
- Camera field of view on the Guition 10.1-inch: Jeroen Peters found with his test builds that the Full modes stall
  after one frame whenever the lines are halved, while the columns halved alone keep running. Both Full modes now
  ask for every halved line (960 x 544 instead of 960 x 528). When a Full mode still stalls, the log says what the
  P4's camera input saw (from his test build). Testers: try both Full modes again and send the log lines with
  "Field of view" and a screenshot.

## 0.6.2 - 2026-10-25
- Camera field of view on the Guition 4.3-inch (GitHub issue #326): both Full modes gave no picture there. They now
  start from the table Sharp uses, which leaves starting the camera to the screen; the one before turned the
  camera on halfway through being set up. Same lane rate and timing as before.
- A Full mode whose picture stops in its first ten seconds, or that the camera does not recover from, goes back to
  Sharp by itself now, whatever takes the pictures (the video, a still, light level and motion); before, it could
  stay on Full with no picture until a restart. The log then says what the sensor made of the mode.

## 0.6.1 - 2026-10-25
- Camera field of view on the Guition: the first test showed more of the room, but moved towards the top right
  and with stripes. Both Full modes now keep their window's offsets even and where they are when the picture is
  mirrored or flipped, and ten seconds after a change the log says what the sensor makes of it: its binning, flip
  and mirror, the part of the array it reads, and the size and place of its output. Testers: try both Full modes
  again, and send that log line with a screenshot of each.

## 0.6.0 - 2026-10-25
- A new setting, **Camera field of view**, for the Guition ESP32-P4 screens' camera (OV02C10), to try. **Sharp** is
  the old behaviour, the middle of the camera's picture, and the default. **Full (binning)** and **Full (skip)** show
  the whole of what the camera sees at half the resolution (960 x 528), as OmniVision's datasheet for the sensor
  describes it: binning averages each two pixels and lines, skip reads every other one, so fine patterns (blinds,
  tiles) may shimmer with skip. Neither is proven on a Guition yet. Guition owners with a camera and the part Video:
  switch back to Sharp if anything looks odd, and say for Full which of the two worked, how much of the room you see
  compared with Sharp, the colours, the frames a second (the log says it ten seconds after the change, and whether the
  sensor took the binning), and whether it stays stable over a few minutes. A mode that gives no picture goes back to
  Sharp by itself. Every other camera keeps Sharp (the M5Stack Tab5's and the reTerminal D1001's sensor has no such
  mode we know of). Changing it restarts the camera and the video's encoder at the new size.

## 0.5.1 - 2026-10-25
- The video's server counts its connections (its listener and its three viewers) into the ones the screen's firmware
  allows. With four live camera tiles on the glass beside the video's viewers the screen ran out of them, and a fifth
  camera tile's picture found no connection. Update when it is offered; the screen builds again.

## 0.5.0 - 2026-10-25
- The camera stands upright by itself: on a screen whose board mounts it upside down (the M5Stack Tab5) and on a
  screen turned half way round. The sensor turns its own picture, so the pictures in Home Assistant, the video and the
  light level and motion all follow, at no cost. A new setting, **Camera upside down**, turns it once more for a screen
  mounted the other way round. Needs Tessera's firmware with plugin API 0.19 (dev); update the screen with it.
- The video's line in the log says how long the encoder takes a frame and how much internal RAM it holds.
- Tab5 owners: check that the camera stands upright in Home Assistant, and say so.

## 0.4.3 - 2026-10-24
- Nothing changes on the screen or in Home Assistant: the camera's code is laid out anew for what comes next. The
  camera's frames reach light level and motion and the video through one hub of their own instead of through the
  video, the sensors the camera finds by itself are a table of the plugin's own, and the video's encoder, server and
  sound are each a file. Update when it is offered; the screen builds again.

## 0.4.2 - 2026-10-22
- Builds on an ESPHome build server on Windows. The camera's colour tuning was handed to the build as a Windows path,
  which lost its backslashes on the way, so the build stopped at a file `sc202cs_ipa.json` it could not find.

## 0.4.1 - 2026-10-22
- Video on the Guition ESP32-P4 screens. It asked for exactly 1280 x 720, which their camera (OV02C10) did not give
  (1288 x 728), so Video showed nothing there. That camera now has a 1280 x 720 mode, its first, and Video takes the
  camera's own size on every board: a size that is not in whole macroblocks of 16 is cut to its middle on the way
  (at a cost in frames a second, the log says so), and a camera above 1280 x 800 asks for a smaller mode in the part's
  YAML. No board names a size. Not yet tried on a Guition screen: please say what you see.

## 0.4.0 - 2026-10-21
- **Video**, a part that is on when you add the plugin: the camera as live video with sound in Home Assistant, a camera
  **Video** on the screen's device that plays in the browser and the app, about a third of a second behind. H.264 from
  the screen's video encoder (1280 x 720, 15 frames a second) and the microphone as Opus, in sync. Install Video in
  Home Assistant once under Tessera's Settings (needs Tessera with that card); Tessera then adds each screen by itself.
  With the part the camera runs from the start and **Camera** leaves the device: **Video** gives the stills. Light level
  and motion keep working. A screen that has the plugin already keeps it as it is until you tick Video.

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
