# Electricity prices

A tile for tomorrow's day-ahead electricity prices. It reads forecast attributes from a Home Assistant sensor, shows
their average and unit, and draws the available prices as a small chart on larger tiles.

It supports the `raw_tomorrow` attribute used by Nord Pool sensors (`start` and `value` in each row) and the
`prices_tomorrow` attribute used by ENTSO-E sensors (`time` and `price` in each row). The tile uses up to the first 16
forecast rows, in the order supplied by the sensor.

## Set up

1. Add this plugin to a screen in Tessera and let the screen build.
2. In Layout, add **Price forecast** from the Plugins group.
3. In the tile inspector, choose a Home Assistant sensor that has a `raw_tomorrow` or `prices_tomorrow` forecast
   attribute.

## Good to know

- The large number is the average of the forecast prices the tile receives. The sensor's unit is shown below it.
- Tiles larger than one cell also show a bar for each available forecast row, scaled from the lowest to the highest
  price.
- Tessera sends only the selected sensor's state and the attributes named in this plugin's manifest to the screen.
  The plugin makes no internet requests.
