# Cycle 1080 — bridge airborne Mission 01 owner boundary

Date : 2026-08-06.

## Classification et provenance

Cette expérience est `bridge` uniquement. Elle ne peut pas valider un gate
native et aucun log, buffer, payload PAC ou PNG bridge n'est ajouté au dépôt.

- Projet Ghidra : `ghidra-projects/ace-combat-6`.
- PAL `default.xex` SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- `DATA.TBL` SHA-256 :
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.
- Binaire bridge SHA-256 :
  `fb5fea32ac3fb34aa2fe1be758151861efdf5ef6cc11d81fea4943d4178920ae`.
- Source bridge dirty externe, fichier
  `src/ac6_backend_fixes/ac6_ui_input_dispatch_probe.cpp`, SHA-256 :
  `960a20f0b2cd6d28e58947c290ab323a71261e9ad2ba9c0a5074f978c509d950`.
- GPU : NVIDIA RTX PRO 4000 Blackwell, Vulkan.
- Audio : `SDL_AUDIODRIVER=dummy`.
- Recette : `scripts/ac6-first-mission-bridge-airborne-probe.steps`.
- Run : `/tmp/ac6-cycle-1080-airborne-qualified`.
- Follow log SHA-256 :
  `2ca857a8f4f759902697a6a704f9afc9ce54fafa45c0a0e2dbc87fd1d8d48c85`.

Le run utilise `ac6_force_loadout_ready=false` et
`ac6_force_loadout_launch=false`. La seule modification de probe est la
condition étroite qui accepte `selector=0` pour armer la transition de la
ressource niveau 1 ; elle n'écrit pas l'état gameplay.

## Handoff et observation gameplay

La route atteint le briefing, la cinématique, puis le HUD aérien. Le premier
handoff gameplay observé est au frame 17962, avec les marqueurs suivants :

```text
gameplay-phase : 44 = UpCam 14, UpInput 12, UpObj 9, UpRadio 9
gameplay-tick  : 12
mode-task      : 17
player-update  : 56
child-dispatch : 40
object-pipeline: 9
unit-census    : 9
```

Le manager gameplay passe de `state=0` à `state=1`, avec
`manager=0xB0EB0000`, `vtable=0x8206457C`, `campaign_phase=0` et
`campaign_step=3`. Le pipeline objet reste à `object_count=230`.

La factory `0x820A7F48` produit 128 paires enter/leave :

| selector | résultat observé | occurrences leave |
| ---: | --- | ---: |
| 1 | `0x820568D4` (joueur) | 1 |
| 3 | `0x82009AB0` | 1 |
| 4 | `0x82009440` | 126 |

Le census répété donne `player=0xB2470000`, enfant
`0xB2470100`/`0x82007A10`, `other_player_count=0`, et un histogramme
`0x820568D4:1`, `0x82009AB0:1`, `0x82009440:228`. Les 228 objets ou le
vtable `0x82009440` ne reçoivent donc aucun nom sémantique par ordre, adresse
ou plausibilité.

Les transform bits du joueur changent pendant le tick, mais les probes ne
montrent aucune publication de faction, cible, vague, unité ennemie ou
transition d'objectif. Aucun hook `flight-input` spécialisé n'est observé
dans cette variante ; cela ne retire pas la preuve séparée du handoff et du
player-update.

## Limite des records et de l'entry 119

La ressource niveau 1 est appelée une fois :

```text
caller=0x8218F3A0 context=0x829E6790 root=0x829E6720
mode=1 selector=0 result=1
```

Le probe arme ensuite `qualified-level1-resource`. Le dispatcher/timeline
produit des records génériques (`record_type=1398228736 = SWG\\0`) et les
opérations de record sont très fréquentes, mais aucun champ de ces records
ne lie un objectif ou une vague au `UnitManager`.

Dans ce même run, les lectures `DATA00.PAC` couvrent la plage stockée de
`DATA.TBL[119]` :

- offset : `0x0C928000` (`210927616`) ;
- longueur stockée : `0x06EE1766` (`116266854`) ;
- requêtes `NtReadFile` chevauchantes : `443`, en blocs `0x40000` ;
- payload expansé entry 119 connu hors ligne :
  `e57cbeeb8f97a7a607ee1315b11a822b6af2d32581dcb7cbd557f1a6280e6dbd`.

Le run ne contient toutefois aucun marqueur
`ac6-campaign-resource-bridge` : la lecture PAC et la présence simultanée
d'un record ne suffisent donc pas à qualifier l'enregistrement puis la
consommation d'entry 119. Le bridge a établi une corrélation de fenêtre,
pas une identité de propriétaire.

## Décision

Cette expérience ferme la borne « le handoff gameplay et la factory retail
sont-ils atteignables ? » : oui. Elle ne ferme pas `retail_units_and_waves`
ni `retail_objectives`. Les hypothèses
`H-RETAIL-OBJECTIVE-WAVE-OWNER-STATIC-BOUNDARY` et
`H-ENTRY119-REGISTER-CONSUMER` restent ouvertes.

La prochaine frontière est un lien exact entre le record/scénario retail et
l'insertion ou l'activation dans `UnitManager`/`MissionManager`, avec
identité stable et condition de transition. Aucune nouvelle correction du
rasterizer, de la caméra ou des textures n'est justifiée par ce run.
