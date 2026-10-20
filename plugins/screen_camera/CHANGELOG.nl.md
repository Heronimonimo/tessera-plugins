# Wijzigingen

## 0.3.0 - 2026-10-20
- Scherpere beelden: ze werden veel te sterk gecomprimeerd (JPEG-kwaliteit 12 van 100, nu 80). Elk beeld is groter,
  ongeveer 150 tot 300 KB.
- Ook de camera van de Guition ESP32-P4-schermen (de OmniVision OV02C10 van de JC8012P4A1, JC1060P470 V2 en
  JC4880P443 in hun cameraversie), op een Tessera waarvan het bord hem noemt. Geprobeerd op de JC8012P4A1 V3.
- Nieuw, uit tot je het aanzet: **Light level** (lux, een schatting) en **Motion** bij het apparaat van het scherm in
  Home Assistant, bepaald door de camera op het scherm zelf: hiervoor gaat geen beeld van het scherm af. Zet
  **Lichtniveau en beweging** aan bij de instellingen van de plugin; de camera blijft dan draaien. Optioneel item voor
  de bovenbalk **Camera ziet beweging**: een oog zolang hij dat doet. Uitgezet is het lichtniveau onbekend.

## 0.2.0 - 2026-10-10
- Ook op een bord dat zijn camera vanaf een pin van klok voorziet, zoals de M5Stack Tab5.

## 0.1.0 - 2026-10-10
- Eerste versie: de eigen camera van het scherm als camera in Home Assistant, en een camera in de bovenbalk zolang er iemand kijkt.
