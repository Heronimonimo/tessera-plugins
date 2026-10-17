"""Flip clock: the ESPHome side. This file stays this short: smart_display.register_plugin() reads the id, the
version, whether its screensaver moves itself and the screen's texts from the manifest and translations/ beside
this folder (docs/FIRMWARE_API.md)."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

# The core component every Tessera screen has; the plugin API lives in it.
DEPENDENCIES = ["smart_display"]

flip_clock_ns = cg.esphome_ns.namespace("flip_clock")
FlipClock = flip_clock_ns.class_("FlipClock", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(FlipClock)}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
