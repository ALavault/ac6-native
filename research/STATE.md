# AC6 native Linux — état de recherche

Mise à jour : 2026-08-06T02:27:03+02:00

## Gate courant

- G0 qualifié : corpus retail PAL, représentation loaded-image, fingerprints,
  build, SDK et machine identifiés.
- Gate selector fermé en runtime `bridge` : C5/C6 sont `full_3d`, avec bit
  maître `0x10` présent et `manager+0x29C` non nul.
- La transition de vue n'est pas absente au niveau comportemental : le champ
  `manager+0x260` traverse `1 -> 0 -> 1 -> 2 -> 1 -> 8 -> 1 -> 2 -> 1 -> 0 -> 3`.
  Le guest PC exact des stores reste ouvert, car le corpus généré ne porte pas
  de PC littéral dans le watcher (`pc=0`); `ctx.lr` reste contextuel.
- Gate entry 119 : le buffer runtime du binaire courant est byte-identique à
  l'extraction hors ligne. L'enregistrement/consommation dans le run courant
  reste à joindre.
- Gate UpHud : la frontière inline `0x8226DF00/0x8226DF1C` est atteinte 1 066
  fois; le bit update `0x80` et la cible virtuelle sont observés. La chaîne
  texte par élément reste ouverte.
- Le renderer natif possède un raster fill qualifié : edge barycentrique unique,
  depth `[0,1]`, clipping NDC conservatif et mode points diagnostic explicite.
  Le modèle joueur applique désormais la pose `WorldFrame` au drawable local;
  aucun correctif shader, texture, resolve, MRT, input ou HSM n'a été appliqué.
  Les runs bridge restent `bridge`, jamais promus `stock`.

## Faits qualifiés

- `default.xex` : SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- `DATA.TBL` : 14 824 octets, 926 entrées, 2 packs, SHA-256
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.
- Corpus généré gelé : 54 fichiers C/C++, tree SHA-256
  `f42fa2c4c1ec3bfb061003ef7074f73881e968ef2719f7f78e59190d1c5af73d`.
- Mission 01 atteint une frame avec HUD, terrain/sky runtime et entrées de vol
  dans la lane `bridge`; cette observation bridge reste distincte de la preuve
  native et ne qualifie pas son monde noir.
- Le runtime join exact des quatre `mapobj_m01` de l'entry 9 et le batch
  environnement générique ont augmenté `flight_world_pixels` hors HUD de
  12 à 141 sur deux exécutions reproductibles.
- Cycles 778–779 : `CModeTaskGame` (`0x82064384`) progresse jusqu'au timeout ;
  `UpInput`, `UpObj`, `UpCam` et `UpRadio` s'exécutent et le UnitManager garde
  230 objets.
- Le registre contient un `CAce6UnitPlayer` exact (`0x820568D4`) et les entrées
  brutes atteignent XAM. La factory canonique donne un wrapper de 256 octets :
  l'ancien raccord `player+10672` est rejeté.
- Cycle 779 ferme l'ownership suivant : `player+216/+220` contient un enfant
  unique `0xB2470100`, table `0x82007A10`; le slot `+0x3C` (`0x822A6710`)
  s'exécute et le transform copié vers `player+144..+207` évolue. Le joueur
  et son transform ne sont pas figés derrière l'écran noir.
- Cycle 780, variable gameplay unique, joint `XAM ly=32767` à une réponse
  immédiate du transform copié `child+128 -> player+160`. La dérive nulle est
  -0,0020 en 2,35 s contre +0,0699 au premier sample pendant le pitch. G8 est
  soutenu mais non qualifié : le champ de commande canonique reste à observer.
- Cycle 781 reproduit cette réponse physique (+0,093475 contre une dérive nulle
  de -0,000593), mais rejette `child+380/+382/+536/+538` comme commande
  analogique : ils restent nuls sur le front pitch. Un front tardif sur `+382`
  constitue le contrôle positif du probe, sans relation causale au stick.
- Cycle 782 ferme directement `XAM -> état canonique LY` à `0x8234D378` :
  `ly=32767`, `raw_ly=0x7FFF` et `canonical_ly=0x7FFF` partagent le même
  timestamp, puis reviennent ensemble à zéro. La réponse physique est encore
  reproduite (+0,270045 contre -0,000637 de dérive nulle).
- Les rôles input attribués historiquement à
  `0x821CE088/0x82215418/0x82215210` sont rejetés pour le projet Ghidra
  canonique. Ils provenaient de `ace-combat-6-corrected`, désormais
  needs-revalidation selon `AGENTS.md`.
- `SDL_AUDIODRIVER=dummy` est un invariant qualifié des runs AC6/Xvfb ; son
  absence peut figer le startup après un seul `PRESENT`.

## Prochain test discriminant

Relier les définitions retail des vagues/objectifs/radio et un HUD natif aux
états de `MissionExecution`. La calibration de scène native reste hashée comme
support de visibilité, mais sa parité retail est ouverte; ne pas rouvrir les
fronts selector, manager, UpHud, DATA.TBL[119] ou bridge sans preuve
contradictoire.

## Slice raster P0 — 2026-08-05

- Le test end-to-end `make_ndxr_be_strip_fixture` couvre triangle strip,
  restart `0xFFFF`, winding CW/CCW, restart entre strips, triangle dégénéré,
  occlusion depth et triangle partiellement hors viewport. Il mesure des
  fragments intérieurs et rejette les écritures ponctuelles implicites.
- La capture native exacte `/tmp/ac6-native-evidence/headless-p0-NAIyvS` couvre
  1 800 ticks à 1280x720 avec `diagnostic_point_writes=0` et
  `filled_fragment_writes=1365250`. Le readback a 780389 pixels couleur et
  profondeur non-clear; le hash couleur est
  `17683416805664810819`, le hash depth `2344087371551432379` et le hash
  sémantique `1cd250de1c0a3e23`.
- Le terrain produit 6 765 275 fragments intérieurs, 1 344 349 écritures
  couleur/depth et une bbox `[0,0]..[1279,718]`; l'image est donc remplie mais
  trop grande/collée aux bords. L'object-ID final ne contient que le terrain;
  le joueur `f16` a des fragments depth-pass pendant le draw mais aucun pixel
  final attribué dans le readback.
- Conclusion : `raster fill qualified`; `world_visible` et
  `player_aircraft_visible` restent `open` dans le contrat. La caméra/clipping
  est la prochaine frontière; la topologie n'est pas rouverte.

## Slice caméra/visibilité P1 — 2026-08-05

- Le défaut de joueur hors-cadre provenait de la géométrie F-16 restée à
  l'origine alors que la caméra suivait `WorldFrame.position_*`. Le chemin
  produit ancre uniquement les drawables `kind=aircraft` sur cette pose; les
  transforms de scène restent ceux du manifeste.
- La capture native exacte `/tmp/ac6-native-evidence/headless-p1-camera` couvre
  1 800 ticks à 1280x720 avec `diagnostic_point_writes=0`,
  `filled_fragment_writes=822161`, 361267 pixels couleur/depth non-clear et
  hash sémantique `84a39be4daf4e71f`.
- Le terrain a 257545 pixels finaux et bbox `[0,366]..[1279,718]`; le drawable
  `f16` a 42722 pixels finaux object-ID et bbox `[452,240]..[1113,457]`.
  L'object-ID est donc une preuve native d'attribution joueur, pas une simple
  couverture non nulle.
- Le manifeste externe est hashé
  `2e552b538df4e36ec0a800f7c21d8f2fd17e46b5dc7b6d0449da11d7f61dc8fd` et son
  `transforms.tsv` `fc4ed417367b2711e7bb8d08f2b813ba55f1089c2ef7ead5e7216828c74de0fb`;
  le terrain y est explicitement `(0,-40,0)`. Cette calibration ferme la
  visibilité native J0, pas la parité des transforms retail.
- Le contrat passe désormais `world_visible` et
  `player_aircraft_visible`; J1 reste ouvert. La topologie n'est pas rouverte.

## Slice natif J0 — 2026-08-05

- Les fermetures bornées entries 9 et 119 puis 119–133 passent avec les
  identités DATA.TBL/PAC qualifiées, zéro note parser et zéro FHM invalide.
  Les associations non fermées restent `open`; aucune sémantique n'est déduite
  d'un magic, d'une taille ou de l'ordre FHM.
- Le runtime expose `--play MANIFEST_DIR 1` et
  `--play-headless MANIFEST_DIR 1 REPLAY_PATH OUTPUT_DIR`. Le device Vulkan,
  la swapchain, le renderer et l'exécution de mission sont persistants pendant
  la boucle.
- La preuve native headless `/tmp/ac6-native-evidence/headless-v2` couvre 1 800
  ticks : `mission_ready=true`, joueur 4097/asset 9, 3 unités, 10 appels de
  géométrie, 2 206 pixels couleur et profondeur non constante. Trois
  exécutions du même replay ont le hash sémantique
  `459390c93868090f`; pause et save/resume sont stables.
- Le contrat `analysis/contracts/mission01-native-gate.json` passe désormais J0
  avec une capture native object-ID après calibration de scène; J1 reste
  ouvert. La capture P0 précédente reste conservée comme baseline du défaut
  point-cloud.
- Le binaire natif expose désormais `--combat-headless`. Le probe manifesté
  verrouille la cible hostile 4098 depuis le joueur 4097, tire l'arme 7 deux
  fois, produit deux événements de dégâts, passe la santé de 100 à 0 et réduit
  les unités actives de 3 à 2. L'artefact externe `combat-v1.json` est hashé
  `cca1e77db38cc8096bfd1a89d3f47a75c0831507107cd81d9f93072ea3c013a3`.
  Cette preuve ferme uniquement les mécaniques natives ciblage/armes/
  dégâts-destruction; elle ne qualifie aucune identité retail de vague ou
  d'objectif.

## Prochain gate

Relier les vagues/objectifs/radio retail et le HUD essentiel aux services natifs;
ne pas utiliser la capture bridge comme preuve J1 et ne pas promouvoir la
calibration de scène comme parité retail sans association exacte.

## Slice HUD natif P2 — 2026-08-05

- `NativeHudRenderer` dessine maintenant un overlay vectoriel dans le chemin
  produit SDL/Vulkan sans texture de secours : réticule, télémétrie, radar et
  panneaux conditionnels lisent `WorldFrame`, `CombatWorld`,
  `MissionScenario`, `RadioPlaybackService` et `MissionExecution`.
- `WorldFrame.speed` est dérivé des mêmes termes de vitesse que l'intégrateur
  de vol. Les identifiants d'arme et le nombre de stores viennent du lancement
  de mission; aucun panneau d'arme n'est dessiné si le lancement n'en déclare
  pas.
- La capture P2 `/tmp/ac6-native-evidence/headless-p2-hud` couvre 1 800 ticks
  à 1280x720 : `hud_pixel_writes=1634`, `hud_unique_pixels=1634`,
  `diagnostic_point_writes=0`, `filled_fragment_writes=822161`, couverture
  couleur 361984 et profondeur 361267. Le replay, la pause et la reprise de
  sauvegarde restent déterministes.
- Le manifeste externe de visibilité ne porte toujours ni définition retail
  d'objectifs, ni radio, ni vagues, ni arme. La preuve P2 qualifie le chemin
  HUD de base mais ne passe pas `essential_hud`, `units_and_waves` ou
  `scenario_radio_or_subtitles`.
- Le chemin interactif conserve le dernier monde lors d'une pause ou d'un
  état terminal et présente l'overlay natif; le monde n'est pas recréé pour
  une frame de pause.

## Slice runtime state P3 — 2026-08-05

- Le run natif `--play-headless` a été rejoué sur 1 800 fixed ticks avec le
  manifeste P1 et le replay qualifié, à 1280x720. Le nouveau rapport de session
  v3 est `reports/mission01-native-captures/p3-runtime-state/native-session.json`,
  SHA-256 `0b509194669eccd022c6e27637b44f3d1df98d383a49ad037b45a63cb47ffeae`.
- La preuve native enregistre `deterministic_replay=true`, `pause_stable=true`,
  `save_resume_stable=true` et `restart_stable=true`, avec le hash sémantique
  `db6cfa8c0aff25f3`. Le contrat ferme donc `pause_save_restart`.
- Cette exécution ne promeut pas de sémantique retail absente du manifeste :
  objectifs, vagues, radio/scénario, HUD complet et débrief succès/échec restent
  ouverts. Les readbacks volumineux restent sous `/tmp/ac6-native-evidence/`;
  aucun payload retail n'est ajouté au dépôt.

## Slice radio retail M01 P4 — 2026-08-06

- La fermeture statique bornée de `DATA.TBL[34]` est qualifiée sans parser note.
  Sa racine a le SHA-256
  `ce5316ffe7f2e52a17bcd7c218a74303fb911a7240fef16b33b5ea416301b0f0` et son
  leaf `root/0015` (table de clés radio) a la taille 5 944 et le SHA-256
  `2c5d9fe0ca271e2869157cfc14fdaffa1988d5152275dbdf1647a0b3578b0fd0`.
  Le même root contient les identités `mapobj_m01_l_brg1` et
  `mapobj_m01_l_brg2`; cette co-localisation ne suffit pas à déduire les
  vagues ou les objectifs.
- Les identifiants big-endian lus dans la table sont `15` pour
  `JIKKYOU_PLAYER_AWACS_MISSION_START`, `86` pour
  `JIKKYOU_PLAYER_AWACS_SHTDWN_SHIP_DESTROYER` et `98` pour
  `JIKKYOU_PLAYER_AWACS_MISSION_END`. Seul l'événement de démarrage est
  intégré au manifeste natif; les timings, la cible et l'audio XMA exact ne
  sont pas promus.
- Le run P4 natif `--play-headless` a utilisé le manifeste hashé
  `ddec953d1ae9b21d930f86fddedb71a73ff617de7e812a72e959b476b24e54bc`,
  1 800 ticks et le replay qualifié. Il donne
  `hud_radio_message_id=15`, `hud_pixel_writes=4920`,
  `diagnostic_point_writes=0`, `filled_fragment_writes=822161`, avec
  `deterministic_replay=true`, `pause_stable=true`,
  `save_resume_stable=true` et `restart_stable=true`. La preuve native
  `native-session.json` a le SHA-256
  `c80bebca9624bceb407f4d5162684fa04392b197e09cd3bb82a5cdc7a0465f71`.
- Le scan textuel complet des entrées PAC n'a pas fourni de
  `SubMisTbl`/`SubMis`/`ComTbl`/`Maneuver` qualifiable. Les marqueurs bruts
  `<Obj`/`<Act` des entrées 553/564 sont des octets de flottants NDXR, pas un
  script scénario. La frontière suivante est donc l'appartenance retail des
  objectifs/vagues, pas le raster, la caméra ou les textures.
- Le gate `scenario_radio_or_subtitles` est passé avec cette preuve native;
  `units_and_waves`, `retail_objectives`, le HUD complet et
  `success_failure_debrief` restent ouverts.

## Slice conditions d’objectif natives — 2026-08-06

- Le runtime accepte maintenant les conditions explicites `manual`,
  `destroy_unit` et `protect_unit` avec une cible d’entité obligatoire pour
  les deux dernières. La résolution est fermée par `UnitRegistry` puis
  `CombatWorld`; un identifiant périmé ne déclenche aucun terminal implicite.
- La sauvegarde version 9 persiste condition et cible; les versions 1 à 8
  restent rétrocompatibles en condition manuelle. Les tests natifs couvrent
  le succès, l’échec et le round-trip checkpoint.
- Cette primitive ne constitue pas une donnée retail. Le contrat ne passe pas
  `retail_objectives`, `units_and_waves` ni `success_failure_debrief` tant
  qu’une fermeture retail exacte ne fournit pas leurs identités et transitions.
- Rapport : `reports/cycle-1034-native-objective-condition-binding.md`.

## Frontière bridge objets gameplay — 2026-08-06

- Une route bridge fraîche a rejoint le HUD Mission 01 avec l’exécutable
  instrumenté `e1a3be5398119c1fa5fabbecc14b1c1f3952ae024e989d3bea4bbff871969bc6`.
- Le census `0x822707C8` reste à 230 objets : joueur identifié, un enfant,
  zéro `other_player`, et 228 objets partageant le vtable `0x82009440`.
  Les mots bornés ne contiennent pas de champ d’identité exploitable; aucune
  vague, faction ou cible n’est déduite.
- Cette preuve bridge supporte uniquement la frontière d’acquisition et
  rejette l’association par ordre/type générique. Le prochain point
  discriminant est le créateur ou registre d’unités lors d’une publication de
  vague; les gates `units_and_waves` et `retail_objectives` restent ouverts.
- Rapport : `reports/cycle-1035-bridge-object-vtable-boundary.md`.

## Slice frontière corpus scénario retail — 2026-08-06

- Un scan borné parallèle des 926 entrées décodées, avec les encodages ASCII,
  UTF-8, UTF-16LE et UTF-16BE, confirme que toutes les racines sont `FHM `;
  aucun `SubMisTbl`, `ComTbl` ou `Maneuver` exact n'est qualifié.
- Les candidats `SubMis` des entries 187 et 191 sont des fermetures UI/SWG,
  pas des propriétaires de scénario. L'entrée 163 est une base `NSXR` de
  shaders et l'entrée 230 une fermeture debrief UI. Les noms génériques
  `Mission`/`Wave` et les marqueurs isolés `Obj`/`Act` restent rejetés sans
  fermeture et décodage lossless.
- Le contrôle Ghidra canonique de la chaîne `SubMisTblBin` à `0x8200F5A8`
  ne trouve aucune référence ni matérialisation split sur 64 octets. Le
  contrôle de la grammaire AC5 sur les entries AC6 0/9/119 produit zéro
  candidat; aucune sémantique AC5 n'est transférée.
- Cette frontière négative ne passe aucun gate J1. La prochaine arête utile
  est le créateur ou registre d'unités au moment d'une publication de vague;
  le census générique du cycle 1035 reste invalidé pour les unités.
- Rapport : `reports/cycle-1036-retail-scenario-corpus-boundary.md`.
