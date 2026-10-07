"""Public transport (NL): a tile with the next departures of a stop, counted down on the screen's own clock.

The add-on fetches the times (the manifest's `fetch`); this component only draws them. register_plugin() takes the
id, version, tile memory and the screen's texts from the manifest and translations/ beside this folder.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

ov_departures_ns = cg.esphome_ns.namespace("ov_departures")
OvDepartures = ov_departures_ns.class_("OvDepartures", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(OvDepartures)}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
