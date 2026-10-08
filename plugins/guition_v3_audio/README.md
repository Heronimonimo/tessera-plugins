# Guition V3 audio

The speaker and microphone of the Guition JC8012P4A1 V3: a short click on every tap the screen takes, a speaker volume,
a switch that turns the microphone off, and two tests on the screen's settings page. Only JC8012P4A1 V3 screens are
offered it, and only screens that take it carry the audio code.

The ES8311 codec and the I2S-connected speaker and microphone are ESPHome components, so another component in the
screen's Override YAML can use `guition_v3_microphone` and `guition_v3_speaker`.

## Set up

1. Have a Guition JC8012P4A1 V3 screen (board `jc8012p4a1v3`) in Tessera.
2. Open **Plugins**, choose **Guition V3 audio** and tick the screen. It builds once with the plugin.
3. On the screen, hold the top bar, then open **Settings**, **Plugins**, **Audio**, and tap **Test the speaker**.

## On the screen

- **Microphone off**: the microphone hears nothing while it is on.
- **Speaker volume**: 0 to 100 %, including the tap sound.
- **Tap sound**: a 60 ms click on every tap the screen takes. It never interrupts a test or recording.
- **Test the speaker**: a tone of 600 ms. Tap the row again to stop it.
- **Test the microphone**: listens for five seconds, then plays what it heard. Tap the row again to stop it. The recording
  stays in PSRAM and is freed after playback.

The three settings are entities of the screen in Home Assistant too ("Microphone mute", "Speaker volume", "Tap sound"),
and stand under Screen settings in the editor.

## Good to know

- Recording uses the ES8311's left microphone channel; playback and speaker tests send the same mono signal to both
  speaker channels.
- The microphone's gain is set by the ES8311. Nothing listens or records unless you start the microphone test.
- The microphone and speaker share one I2S bus, so the screen does not listen while it plays.
- The linked hardware configuration uses a stereo speaker and a 16 kHz sample rate. The plugin does not configure a
  separate amplifier pin.
- Not yet tested on a physical Guition JC8012P4A1 V3.
