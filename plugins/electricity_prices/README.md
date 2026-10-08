# Electricity prices

A tile for live and day-ahead electricity prices from a Home Assistant sensor. A small tile shows the sensor's current
price. A larger tile or the chart opened by tapping the tile shows available prices for today and tomorrow.

It supports Nord Pool's `raw_today` and `raw_tomorrow` attributes (`value` in each row) and ENTSO-E's `prices_today`
and `prices_tomorrow` attributes (`price` in each row). The tile uses up to 16 rows per day, in the order supplied by
the sensor.

## Set up

1. Add this plugin to a screen in Tessera and let the screen build.
2. In Layout, add **Price forecast** from the Plugins group.
3. In the tile inspector, choose a Home Assistant sensor whose state is the current price and whose attributes include
   a supported price forecast.

## Good to know

- Tap any tile to open its price chart.
- Forecast bars are scaled from the lowest to highest available price. Today and tomorrow use different colours.
- The sensor's unit is shown with both the current price and chart.
- Tessera sends only the selected sensor's state and the attributes named in this plugin's manifest to the screen.
  The plugin makes no internet requests.
