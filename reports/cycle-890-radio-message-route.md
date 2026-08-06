# Cycle 890 — radios externes

Ajout de `RadioMessageDatabase` avec manifeste TSV
`mission_id\tid\tstable_id\tspeaker\taudio_asset\tsubtitle_asset`. Le
scénario vérifie l’appartenance mission et l’état Gameplay/Paused avant
d’ajouter le message à son historique ; la lecture audio reste déléguée au
service SDL3.

Validation : CTest `2/2`, smoke SDL3/Vulkan double-frame sous Xvfb : OK.
