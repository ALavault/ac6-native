# Mission 01 native HUD capture — P2

Capture native Linux exacte sur 1 800 fixed ticks, 1280x720, avec le replay
`/tmp/ac6-native-evidence/mission01.replay` et le manifeste externe
`/tmp/ac6-mission01-native-p1-camera/manifest.tsv`.

Commande :

```text
SDL_AUDIODRIVER=dummy xvfb-run -a ac6-native --play-headless \
  /tmp/ac6-mission01-native-p1-camera/manifest.tsv 1 \
  /tmp/ac6-native-evidence/mission01.replay \
  /tmp/ac6-native-evidence/headless-p2-hud
```

Le renderer HUD vectoriel est natif et lit `WorldFrame`, le combat, le
scénario, la radio et le débrief. Les compteurs de la capture finale sont :

```text
diagnostic_point_writes = 0
filled_fragment_writes  = 822161
hud_pixel_writes        = 1634
hud_unique_pixels       = 1634
color_coverage          = 361984
depth_coverage          = 361267
speed                   = 1.14458
reticle_visible         = true
telemetry_visible       = true
radar_visible           = true
weapon_visible          = false
objective_count         = 0
radio_message_id        = 0
deterministic_replay    = true
pause_stable            = true
save_resume_stable      = true
```

Le manifeste P1 ne déclare volontairement ni armes, ni objectifs, ni radio,
ni vagues qualifiés. Les panneaux correspondants restent donc absents ; cette
capture ne passe pas `essential_hud`, `units_and_waves` ou
`scenario_radio_or_subtitles`. Elle prouve uniquement que les éléments HUD
disponibles sont produits à partir de données natives et que le chemin ne
réintroduit pas de points diagnostiques.

`object-id.png`, `depth-preview.png` et `wireframe.png` sont des exports de
contrôle ; seul le buffer couleur est présenté par le produit. Aucun payload
retail, Xenia, RexGlue, XEX, Wine ou archive PAC n'est consommé à l'exécution.

Artefacts et SHA-256 :

```text
color.png          67ede8f4f3da6115aef25e28bc2ff3a4aa6f7e9c69feb0db4062e3bbe6d72c53
depth-preview.png  325ff508e5d80edf3b6fba0937ce9d239cf0aeb6385b17a968173cc7cd9b293e
wireframe.png      e467e5c51f86f0d457f0a299c0ee80f01b79c745a206c2fe0752ebb79a7edf85
object-id.png      ab06458c79077d6f58d6cdef04f21586a2610e106d1ccd06dea72f2a1387caac
capture-metrics.json 7f02134a8c95ad391995c86f478eb40f80909aff204243af242a7250f72c4a3f
native-session.json  e5f4f64564b3e31f8fab919c256614d023890f0faba8a2f630a920ef97a9bb43
```
