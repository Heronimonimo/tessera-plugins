# Tikgeluid

Een kort, zacht tikje op de luidspreker van het scherm zodra je vinger iets raakt waar je op kunt tikken: een tegel,
een toets, een slider, een rij van de instellingen, de bovenbalk. Sleep je een slider, dan tikt hij zachter bij elke
stap: elke 5 %, of elke stand van een slider met maar een paar, met het volle tikje waar hij zijn einde bereikt. - of +
vasthouden tikt bij elke stap. Het speelt op het volume van het scherm: zet je het
scherm zachter, dan is het tikje ook zachter.

## Wat je nodig hebt

- De firmware van Tessera met plugin-API 0.11.
- Een scherm met een luidspreker: een bord dat er een meebrengt (de reTerminal D1001, het Waveshare ESP32-P4-86-paneel)
  of een plugin die dat doet.

## Instellen

1. Open in Tessera **Plugins**, kies **Tikgeluid** en vink je schermen aan. Elk scherm wordt een keer gebouwd.
2. Meer is het niet: het scherm tikt vanaf dan bij elke aanraking.

## Op het scherm

Houd de bovenbalk vast, dan **Instellingen**, **Plugins**, **Tikgeluid**: **Tikgeluid** zet het tikje uit en aan. Dezelfde
schakelaar is een entiteit van het scherm in Home Assistant ("Tap sound") en staat in de details van de plugin op het
tabblad Plugins van het scherm. Het **Volume** van het scherm staat onder **Extras**.

## Het geluid

Het zijn twee bestanden: `components/tap_sound/sounds/tap.wav` voor een vinger die neerkomt en `step.wav` voor een stap
van een slider, WAV-bestanden met 16-bit samples, mono, op 48 kHz. Een
eigen plugin die je van deze maakt kan ze vervangen; de build zegt het als een bestand niet zo'n WAV is. Op een scherm
waarvan de microfoon en de luidspreker een bus delen (het Waveshare-paneel) wacht een tikje zolang er iets luistert, en
valt het weg als dat langer dan een seconde duurt.
