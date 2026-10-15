# Tap sound

A short, soft tick on the screen's speaker the moment your finger lands on something you can tap: a tile, a key, a
slider, a row of the settings, the top bar. Drag a slider and it ticks more softly at every step: every 5 %, or every
position of a slider with only a few. It plays at the screen's volume, so turn the screen down and the tick is quieter
too.

## What you need

- Tessera's firmware with plugin API 0.11.
- A screen with a speaker: a board that brings one (the reTerminal D1001, the Waveshare ESP32-P4-86 panel) or a plugin
  that does.

## Set up

1. In Tessera, open **Plugins**, choose **Tap sound** and tick your screens. Each screen builds once with it.
2. That is all: the screen ticks on every touch from then on.

## On the screen

Hold the top bar, then **Settings**, **Plugins**, **Tap sound**: **Tap sound** turns the tick off and on. The same switch
is an entity of the screen in Home Assistant ("Tap sound") and in the plugin's details on the screen's Plugins tab. The
screen's **Volume** is under **Extras**.

## The sound

It is two files: `components/tap_sound/sounds/tap.wav` for a finger landing and `step.wav` for a step of a slider, WAV
files of 16-bit samples, mono, at 48 kHz. A plugin of your
own made from this one can swap them; the build says so when a file is not such a WAV. On a screen whose microphone and
speaker share one bus (the Waveshare panel), a tick waits while something listens and is left out when that takes
longer than a second.
