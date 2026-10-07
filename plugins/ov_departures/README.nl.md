# Openbaar vervoer (NL)

Een tegel die laat zien wanneer de volgende bus, tram, metro of veerboot bij jouw halte vertrekt, met de looptijd naar
de halte er al af. De tijden komen live van [OVapi](http://v0.ovapi.nl), de open dienst achter veel Nederlandse
vertrekborden. Er is geen account of sleutel voor nodig.

Op één cel toont de tegel het eerstvolgende vertrek zo groot als de cel toelaat, met lijn en bestemming erboven. Is er
dan nog ruimte, dan staan de twee daarna eronder; op een klein scherm zoals de CYD houdt de tijd de ruimte. Op een
tegel van twee kolommen of breder is het een lijst, een regel per vertrek, zoveel als er passen.

## Instellen

1. Zoek de code van je halte. Open [ovzoeker.nl](https://www.ovzoeker.nl), klik je halte op de kaart aan en kopieer het
   nummer achter *haltenummer* (acht cijfers, zoals `30003025`). Een halte heeft een code per richting: kies de kant
   van de straat waar je vertrekt.
2. Voeg de plugin in Tessera toe aan een scherm (Plugins, of de tab Plugins van het scherm). Het scherm wordt één keer
   gebouwd.
3. Zet in Indeling de tegel **Eerstvolgende vertrek** uit de groep Plugins van de bibliotheek op een pagina.
4. Vul in de inspector de haltecode in. De lijst **Lijn** vult zich dan met de lijnen die daar stoppen; laat hem leeg
   voor alle lijnen.
5. Stel bij **Lopen naar de halte** in hoeveel minuten je nodig hebt. Vertrekken die je niet meer haalt, worden
   overgeslagen.

## Goed om te weten

- Een vertrek dat een minuut of meer later is, krijgt "+2" achter de bestemming. Zet **Vertraging tonen** uit in de
  inspector om dat weg te laten.
- Tessera vraagt OVapi één keer per minuut per halte op, voor alle schermen en tegels samen. Daartussen telt het
  scherm zelf af.
- Een vertrek over een uur of meer staat er als tijdstip.
- Antwoordt OVapi niet, dan houdt de tegel de laatste tijden en zegt hij dat ze oud kunnen zijn.
- Werkt op elk bordje, ook op de CYD.

## Privacy

Tessera (de app in Home Assistant, niet het scherm) vraagt `v0.ovapi.nl` om de vertrektijden van de haltecodes die je
invult. OVapi ziet het adres van je Home Assistant en die haltecodes, verder niets. Er gaat geen sleutel, account of
persoonlijke informatie mee. De vraag gaat over gewoon http, omdat het https-certificaat van OVapi niet bij zijn naam
past; de plugin stuurt niets geheims.
