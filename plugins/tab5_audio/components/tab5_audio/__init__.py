"""Tab5 audio: the audio devices are ESPHome components in plugin.yaml; this component controls them for the screen."""
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

tab5_audio_ns = cg.esphome_ns.namespace("tab5_audio")
Tab5Audio = tab5_audio_ns.class_("Tab5Audio", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Tab5Audio),
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
