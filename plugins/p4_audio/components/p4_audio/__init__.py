"""Audio on the Waveshare ESP32-P4 86 panel: a click on every tap and two tests on the screen's settings page.

The speaker, the microphone, the codecs and the three settings are ESPHome components of plugin.yaml; this component
takes them and does what the screen asks of them (p4_audio.cpp). register_plugin() takes the id, version and the
screen's texts from the manifest and translations/ beside this folder.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import audio_dac, microphone, number, smart_display, speaker, switch
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

CONF_SPEAKER = "speaker"
CONF_MICROPHONE = "microphone"
CONF_DAC = "dac"
CONF_AMPLIFIER = "amplifier"
CONF_MUTE = "mute"
CONF_TAP_SOUND = "tap_sound"
CONF_VOLUME = "volume"

p4_audio_ns = cg.esphome_ns.namespace("p4_audio")
P4Audio = p4_audio_ns.class_("P4Audio", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(P4Audio),
        cv.Required(CONF_SPEAKER): cv.use_id(speaker.Speaker),
        cv.Required(CONF_MICROPHONE): cv.use_id(microphone.Microphone),
        cv.Required(CONF_DAC): cv.use_id(audio_dac.AudioDac),
        cv.Required(CONF_AMPLIFIER): cv.use_id(switch.Switch),
        cv.Required(CONF_MUTE): cv.use_id(switch.Switch),
        cv.Required(CONF_TAP_SOUND): cv.use_id(switch.Switch),
        cv.Required(CONF_VOLUME): cv.use_id(number.Number),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_speaker(await cg.get_variable(config[CONF_SPEAKER])))
    cg.add(var.set_microphone(await cg.get_variable(config[CONF_MICROPHONE])))
    cg.add(var.set_dac(await cg.get_variable(config[CONF_DAC])))
    cg.add(var.set_amplifier(await cg.get_variable(config[CONF_AMPLIFIER])))
    cg.add(var.set_mute(await cg.get_variable(config[CONF_MUTE])))
    cg.add(var.set_tap_sound(await cg.get_variable(config[CONF_TAP_SOUND])))
    cg.add(var.set_volume(await cg.get_variable(config[CONF_VOLUME])))
