"""Tap sound: a tick on the screen's speaker the moment a finger lands on something that takes a tap (Plugin::on_press,
plugin API 0.11), and a softer one for every step of a dragged slider (Plugin::on_step).

The tick is sounds/tap.wav beside this file, the step sounds/step.wav, built in by smart_display.sound() and played by the core's sound player
(plugin_sound.h); a WAV file of 16-bit samples, swap it to change it. register_plugin() takes the id, version and the
screen's texts from the manifest and translations/ beside this folder.
"""
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display, speaker, switch
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display", "speaker"]

CONF_SPEAKER = "speaker"
CONF_SOUND_ON = "sound_on"
TICK = Path(__file__).parent / "sounds" / "tap.wav"
STEP = Path(__file__).parent / "sounds" / "step.wav"

tap_sound_ns = cg.esphome_ns.namespace("tap_sound")
TapSound = tap_sound_ns.class_("TapSound", cg.Component)


def check_tick(config):
    smart_display.read_sound(TICK)
    smart_display.read_sound(STEP)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TapSound),
            cv.Required(CONF_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Required(CONF_SOUND_ON): cv.use_id(switch.Switch),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    check_tick,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_speaker(await cg.get_variable(config[CONF_SPEAKER])))
    cg.add(var.set_sound_on(await cg.get_variable(config[CONF_SOUND_ON])))
    cg.add(var.set_tick(smart_display.sound(config[CONF_ID], "tick", TICK)))
    cg.add(var.set_step(smart_display.sound(config[CONF_ID], "step", STEP)))
