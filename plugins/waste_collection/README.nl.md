# Afvalkalender

Wanneer de volgende container buiten moet, uit een kalender in Home Assistant: op een tegel, de dag ervoor in de
bovenbalk, en op een kaart met de ophaaldagen van de komende vier weken.

De tegel toont wat er opgehaald wordt (papier, GFT, plastic) met zijn kleur, wanneer ("Morgen", "Over 3 dagen") zo groot
als de tegel toelaat, en de datum eronder. Een tik opent de kaart. De bovenbalk zegt "Morgen: Papier" vanaf de dag ervoor,
of zoveel dagen vooruit als je op het scherm instelt.

## Instellen

1. Zorg voor een kalender met de ophaaldagen in Home Assistant. [Waste Collection Schedule](https://github.com/mampfes/hacs_waste_collection_schedule)
   en Afvalwijzer maken er een voor de meeste gemeenten; een Local Calendar met de dagen erin getypt werkt ook.
2. Open in Tessera **Plugins**, kies **Afvalkalender**, vink je schermen aan en kies de kalender bij
   **Afvalkalender**. Elk scherm wordt één keer gebouwd met de plugin.
3. Zet de tegel **Volgende ophaaldag** op een pagina en kies de kalender in de inspector.
4. Optioneel: zet **Volgende ophaaldag** in de bovenbalk van een pagina (Bovenbalk, Toevoegen, Van plugins).
5. Optioneel: een sensortegel van je afvalintegratie kan de kaart ook openen: zet zijn tik op **Komende ophaaldagen tonen**.

## Op het scherm

Houd de bovenbalk vast, dan **Instellingen**, **Plugins**, **Afvalkalender**:

- **In de bovenbalk**: of de bovenbalk het überhaupt zegt.
- **Dagen vooruit**: vanaf hoeveel dagen voor een ophaaldag de bovenbalk het zegt (0 is de dag zelf, 1 de dag ervoor).
- **Komende ophaaldagen**: opent de kaart.

Beide instellingen zijn ook entiteiten van het scherm in Home Assistant ("Waste in top bar", "Waste days ahead"), dus
een automatisering kan ze aanpassen.

## Goed om te weten

- De kleuren volgen de woorden die kalenders gebruiken: papier en karton blauw, GFT en groen groen, plastic en
  verpakkingen oranje, glas paars, de rest grijs. Nederlandse en Engelse woorden werken allebei.
- De kaart vraagt de komende afspraken via Tessera op bij Home Assistant (`calendar.get_events`); voor de bovenbalk leest
  het scherm de eerstvolgende afspraak van de kalender zelf.
- Werkt op elk bordje, ook op de CYD.
