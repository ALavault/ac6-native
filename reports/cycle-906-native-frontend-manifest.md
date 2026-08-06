# Cycle 906 — chemin frontend natif du manifeste Mission 01

Le générateur de manifeste produit désormais `input.tsv` avec les événements
Start/Pause/Resume/Abort. Le chemin `--frontend-smoke` traverse naturellement
Title → New Game → Briefing → Hangar → Loading → Mission, sélectionne la
Mission 01 du catalogue et résout sa définition sans saut d'état.

Validation sur un manifeste NDXR F-16 + terrain : `ac6-native --frontend-smoke …`
retourne 0 sous SDL dummy/Xvfb. Cette preuve frontend est distincte de la
preuve gameplay/oracle et ne prétend pas encore présenter les écrans retail.
