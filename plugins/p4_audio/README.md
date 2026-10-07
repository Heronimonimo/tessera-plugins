# P4 panel audio

The speaker and the microphone of the Waveshare ESP32-P4-86-Panel-ETH-2RO: a short click on every tap the screen takes,
a speaker volume, a switch that turns the microphone off, and two tests on the screen's settings page. Only screens of
that board are offered it, and only the screens that take it carry the audio code.

The microphone and the speaker are ESPHome's own components (`microphone`, `speaker`, the ES7210 and ES8311 codecs), so
anything else that uses them, a voice assistant of your own in the screen's Override YAML for example, finds them as
`p4_microphone` and `p4_speaker`.

## Set up

1. Have a screen of the Waveshare ESP32-P4 86 panel (board `wavesharep4`) in Tessera.
2. Open **Plugins**, choose **P4 panel audio** and tick the screen. It builds once with the plugin.
3. On the screen: hold the top bar, then **Settings**, **Plugins**, **Audio**, and tap **Test the speaker**.

## On the screen

- **Microphone off**: the microphone hears nothing while it is on.
- **Speaker volume**: 0 to 100 %, the tap sound included.
- **Tap sound**: a 60 ms click on every tap of a tile, a key or a button. It never interrupts a test or a recording.
- **Test the speaker**: a tone of 600 ms. A second tap stops it.
- **Test the microphone**: listens for five seconds, then plays what it heard. A second tap stops it. The recording
  stays in the screen's memory and is gone once it has played.

The three settings are entities of the screen in Home Assistant too ("Microphone mute", "Speaker volume", "Tap sound"),
and stand under Screen settings in the editor.

## Good to know

- Capture and playback take turns on the panel's one I2S bus: the screen does not listen while it plays.
- The microphone's gain is fixed at 30 dB. Nothing listens or records unless you start the microphone test.
- The panel routes the speaker's signal back to a second input of the ES7210, which makes echo cancellation possible.
  That needs another audio stack than ESPHome's own, and is not in this version.
- Built for the pre-v3 ESP32-P4 the board file targets; tried in a build, not yet on the panel itself.

## Credits

The wiring, the codec settings and the first audio for this panel come from pull request
[#142](https://github.com/MaxGramser/homeassistant_espscreen/pull/142) by [@woozer](https://github.com/woozer), who made
the panel play and listen first, echo cancellation included.

## How it works

- `tessera-plugin.yaml`: the board it is for (`boards: [wavesharep4]`), plugin API 0.3, and the three settings.
- `plugin.yaml`: the codecs on the touch panel's I2C bus, the I2S bus, the microphone and the speaker, the speaker's
  amplifier on GPIO53, and the settings as two template switches and a number.
- `components/p4_audio/`: the settings page, the click (`on_touch`), and the two tests, played from PSRAM without ever
  waiting in the screen's loop.
