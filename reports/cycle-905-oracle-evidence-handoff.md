# Cycle 905 — preuve oracle encore requise

Le produit natif possède maintenant les seams replay, NDXR, caméra, UV et
texture externe, mais aucune capture positive de gameplay oracle n'est présente
dans le workspace. Les captures Xvfb Xenia précédentes sont noires/menu-only et
ne sont pas acceptées comme référence.

Handoff humain minimal, sans archive à fournir :

- route : Xenia Wine/Vulkan épinglé documenté dans `XENIA_WINE_ORACLE_HANDOFF.md` ;
- action : atteindre Mission 01 après le premier contrôle joueur, sans pause ni
  warp, puis maintenir 30 s de contrôle neutre ;
- artefacts : readback couleur 1280×720, readback profondeur, et trace des
  1 800 `InputFrame` à 60 Hz ;
- identité : SHA-256 du XEX/PAL et timestamp de la frame initiale.

Cette preuve est le seul élément externe qui manque pour exécuter
`--compare-mission01` avec un seuil SSIM/couverture significatif. Les archives
retail ne doivent pas être envoyées au modèle.
