"""Waste collection: the next collection from a calendar in Home Assistant, on a tile, a card and in the top bar.

The values the top bar's item and the settings rows need are ESPHome entities of plugin.yaml; this component reads them
and registers the tile, the card, the tap action and the bar item (waste_collection.cpp). register_plugin() takes the
id, version, tile memory and the screen's texts from the manifest and translations/ beside this folder.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, smart_display, switch, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

CONF_MESSAGE = "message"
CONF_START = "start"
CONF_IN_BAR = "in_bar"
CONF_DAYS_AHEAD = "days_ahead"
CONF_CALENDAR = "calendar"

waste_ns = cg.esphome_ns.namespace("waste_collection")
WasteCollection = waste_ns.class_("WasteCollection", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WasteCollection),
        cv.Required(CONF_MESSAGE): cv.use_id(text_sensor.TextSensor),
        cv.Required(CONF_START): cv.use_id(text_sensor.TextSensor),
        cv.Required(CONF_IN_BAR): cv.use_id(switch.Switch),
        cv.Required(CONF_DAYS_AHEAD): cv.use_id(number.Number),
        cv.Required(CONF_CALENDAR): cv.entity_id,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_message(await cg.get_variable(config[CONF_MESSAGE])))
    cg.add(var.set_start(await cg.get_variable(config[CONF_START])))
    cg.add(var.set_in_bar(await cg.get_variable(config[CONF_IN_BAR])))
    cg.add(var.set_days_ahead(await cg.get_variable(config[CONF_DAYS_AHEAD])))
    cg.add(var.set_calendar(config[CONF_CALENDAR]))
