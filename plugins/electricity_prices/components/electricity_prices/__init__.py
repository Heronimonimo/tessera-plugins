"""Electricity prices: the ESPHome side; the plugin reads forecast data from sensor entity attributes."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

# The core component every Tessera screen has; the plugin API lives in it.
DEPENDENCIES = ["smart_display"]

electricity_prices_ns = cg.esphome_ns.namespace("electricity_prices")
ElectricityPrices = electricity_prices_ns.class_("ElectricityPrices", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(ElectricityPrices)}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
