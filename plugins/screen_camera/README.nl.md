# Schermcamera

Bekijk de eigen camera van het scherm in Home Assistant. Het scherm wordt daar een camera van zijn apparaat: een beeld
op een dashboard, een stream als je hem opent, en alles wat Home Assistant met een camera doet (een momentopname in een
automatisering, een opname).

Het beeld wordt op het scherm zelf in hardware gemaakt, alleen zolang Home Assistant erom vraagt. Zolang dat zo is,
staat er een camera in de bovenbalk van het scherm, zodat wie ervoor staat het weet.

## Wat je nodig hebt

- Een scherm waarvan het bord een camera heeft die Tessera kan aanzetten en bereiken: de reTerminal D1001, de M5Stack
  Tab5, of een Guition ESP32-P4-scherm in de cameraversie ("C" in de modelnaam): de JC8012P4A1 (ook de V2 en V3), de
  JC1060P470 V2 en de JC4880P443. De Guition-schermen zijn nieuw en alleen op de JC8012P4A1 V3 geprobeerd.
- Verder niets in Home Assistant: de camera hoort bij het apparaat van het scherm in de ESPHome-integratie.

## Instellen

1. Open in Tessera **Plugins**, kies **Schermcamera** en vink je schermen aan. Elk scherm wordt een keer gebouwd.
2. Open in Home Assistant het apparaat van het scherm (Instellingen, Apparaten): de camera staat er als **Camera**.
   Zet hem op een dashboard met een kaart die camerabeeld toont (Picture entity of Picture glance).
3. Optioneel: zet **Camera in gebruik** in de bovenbalk van een pagina (Bovenbalk, Toevoegen, Uit plugins). Hij staat
   er zolang er beelden het scherm verlaten: "Live" bij een stream, "Camera" bij af en toe een beeld.

## Privacy

Er verlaat niets het scherm tot Home Assistant om een beeld vraagt, en elk beeld gaat alleen naar je eigen Home
Assistant. Iedereen die het apparaat van het scherm of een dashboard met de camera kan openen, kan erdoor kijken. Om dat
even te stoppen schakel je de camera-entiteit in Home Assistant uit; voorgoed haal je de plugin van het scherm.

## Voor makers

De camera is ESPHome's camera met id `ts_camera` (de feature `camera`), zodat een andere plugin erop kan bouwen. Het
camerastuurprogramma is `esp_video_camera` (github.com/n-IA-hane/esphome-esp-video-camera, uit ESPHome pull request
esphome/esphome#16944), ongewijzigd overgenomen in `components/esp_video_camera` onder de ESPHome-licentie, met zijn
NOTICE. De OmniVision OV02C10 van de Guition-schermen, waarvoor Espressifs camerabibliotheek geen stuurprogramma heeft,
is een eigen component ernaast, `components/ov02c10`: de sensordriver en zijn beeldafstemming (een voor rev3-chips, een
voor oudere), met een NOTICE die zegt waar ze vandaan komen.
