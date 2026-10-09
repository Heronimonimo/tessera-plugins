# Tab5 audio

The M5Stack Tab5's built-in speaker and microphone, with a short click on every tap the screen takes, a speaker volume,
a microphone mute switch, and speaker and microphone tests on the screen's settings page. The audio uses ESPHome's
ES8388 DAC and ES7210 ADC.

The microphone and speaker are ESPHome components named `tab5_microphone` and `tab5_speaker`. Capture and playback take
turns on the Tab5's shared I2S bus.

## Set up

1. Use a M5Stack Tab5 screen running Tessera's **Tab5** board profile.
2. Open **Plugins**, choose **Tab5 audio**, and add it to the screen. The firmware builds with the plugin.
3. Hold the top bar, then open **Settings → Plugins → Audio** and tap **Test the speaker**.

## On the screen

- **Microphone off** mutes the microphone.
- **Speaker volume** controls playback and tap sounds.
- **Tap sound** plays a short click when the screen accepts a tap. It does not interrupt other audio.
- **Test the speaker** plays a 600 ms tone; tap it again to stop.
- **Test the microphone** records for five seconds and plays the recording; tap it again to stop.

The plugin selects the ES8388's **LINE1** output for the onboard speaker and controls the speaker-enable output through
the Tab5's I/O expander.

## Good to know

- The microphone records only while its test is running; the recording stays in PSRAM until playback ends.
- The speaker, microphone and codecs are ESPHome components, so another screen component can use them too.
- This plugin targets the Tab5 board profile (`tab5`) and needs PSRAM.
- The firmware configuration follows the [ESPHome Tab5 audio setup](https://devices.esphome.io/devices/m5stack-tab5/).
- It has not yet been built on or tested with a physical Tab5.
