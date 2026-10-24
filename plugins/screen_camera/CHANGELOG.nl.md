# Wijzigingen

## 0.4.3 - 2026-10-24
- Er verandert niets op het scherm of in Home Assistant: de code van de camera is anders ingedeeld voor wat komt. De
  beelden van de camera gaan via een eigen verdeler naar lichtniveau en beweging en naar de video, in plaats van via
  de video, de sensoren die de camera zelf vindt staan in een tabel van de plugin, en de encoder, de server en het
  geluid van de video hebben elk een eigen bestand. Werk bij als het wordt aangeboden; het scherm bouwt opnieuw.

## 0.4.2 - 2026-10-22
- Bouwt op een ESPHome-bouwserver onder Windows. De kleurafstelling van de camera ging als Windows-pad naar de bouw,
  dat onderweg zijn backslashes kwijtraakte, dus de bouw stopte bij een bestand `sc202cs_ipa.json` dat niet te vinden
  was.

## 0.4.1 - 2026-10-22
- Video op de Guition ESP32-P4-schermen. Het vroeg precies 1280 x 720, en hun camera (OV02C10) gaf dat niet
  (1288 x 728), dus daar liet Video niets zien. Die camera heeft nu een modus van 1280 x 720, zijn eerste, en Video
  neemt op elk bord de maat van de camera zelf: een maat die niet in hele macroblokken van 16 past wordt onderweg tot
  zijn midden geknipt (dat kost beelden per seconde, het log zegt het), en een camera boven 1280 x 800 vraagt om een
  kleinere modus in de YAML van het onderdeel. Geen bord noemt een maat. Nog niet geprobeerd op een Guition-scherm:
  zeg wat je ziet.

## 0.4.0 - 2026-10-21
- **Video**, een onderdeel dat aan staat als je de plugin toevoegt: de camera als live video met geluid in Home
  Assistant, een camera **Video** bij het apparaat van het scherm die speelt in de browser en de app, ongeveer een derde
  seconde later. H.264 uit de video-encoder van het scherm (1280 x 720, 15 beelden per seconde) en de microfoon als Opus,
  in sync. Installeer eenmalig Video in Home Assistant onder Instellingen van Tessera (Tessera met die kaart nodig);
  Tessera voegt daarna elk scherm zelf toe. Met het onderdeel loopt de camera vanaf de start en verdwijnt **Camera** van
  het apparaat: **Video** geeft de stilstaande beelden. Lichtniveau en beweging blijven werken. Een scherm dat de plugin
  al heeft houdt hem zoals hij is tot je Video aanvinkt.

## 0.3.2 - 2026-10-20
- Echtere kleuren op de M5Stack Tab5 en de reTerminal D1001: zwart is weer zwart. Hun camera gaf zwart door als een
  donkergrijs dat de chip van het scherm er niet af kon halen, zodat donkere delen van het beeld roze waren en het hele
  beeld flets. De plugin zet nu het zwartniveau van de sensor zelf.

## 0.3.1 - 2026-10-20
- **Beweging wekt het scherm** (eerst uit) en **Beweging blijft aan** (5 seconden tot 10 minuten, eerst 10) zijn ook
  instellingen.
- De eigen stip van Tessera in de rechterbovenhoek van het scherm staat er nu zolang de camera in gebruik is, ook bij
  het meten van licht en beweging; de bovenbalk-items **Camera in gebruik** en **Camera ziet beweging** van de plugin
  zijn weg. Werkt met Tessera met plugin-API 0.17.

## 0.3.0 - 2026-10-20
- Scherpere beelden: ze werden veel te sterk gecomprimeerd (JPEG-kwaliteit 12 van 100, nu 80). Elk beeld is groter,
  ongeveer 150 tot 300 KB.
- Ook de camera van de Guition ESP32-P4-schermen (de OmniVision OV02C10 van de JC8012P4A1, JC1060P470 V2 en
  JC4880P443 in hun cameraversie), op een Tessera waarvan het bord hem noemt. Geprobeerd op de JC8012P4A1 V3.
- Nieuw, uit tot je het aanzet: **Light level** (lux, een schatting) en **Motion** bij het apparaat van het scherm in
  Home Assistant, bepaald door de camera op het scherm zelf: hiervoor gaat geen beeld van het scherm af. Zet
  **Lichtniveau en beweging** aan bij de instellingen van de plugin; de camera blijft dan draaien. Uitgezet is het
  lichtniveau onbekend.

## 0.2.0 - 2026-10-10
- Ook op een bord dat zijn camera vanaf een pin van klok voorziet, zoals de M5Stack Tab5.

## 0.1.0 - 2026-10-10
- Eerste versie: de eigen camera van het scherm als camera in Home Assistant, en een camera in de bovenbalk zolang er iemand kijkt.
