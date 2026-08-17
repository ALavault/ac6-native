# AC6 native Linux — état de recherche

Mise à jour : 2026-08-15T06:30:00+02:00

## Contrat actif — démo native Linux

- La cible primaire exclusive est désormais la démo PAL `Default.xex`,
  SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
  qualifiée dans Ghidra 12.1.2 sous `PowerPC:BE:64:Xenon` par le projet
  `ghidra-projects/ace-combat-6-demo`.
- Le PAL retail `acc302c1…bcde`, son produit, son projet Ghidra, son worktree et
  ses preuves sont gelés. Les sections retail ci-dessous restent historiques
  et ne décrivent plus la cible active ; aucune preuve ni trace n'est fusionnée
  entre les identités.
- Le socle démo déjà fermé est conservé : codegen strict reproductible,
  runtime PPC/scheduler déterministe, corridor XAM partiel, replay
  `AC6RTPLY-v4` et processeur PM4 transactionnel. Les frontières ouvertes sont
  le movie XAM appel par appel, les consumers frontend, les pixels Xenos, les
  services/audio atteints et la mission endogène.
- Contrat détaillé : `/fastdata/lavaulta/auto-re-agent/GOAL.md`. Gate :
  `recompilation/ace-combat-6-demo/config/demo-playable-gate-v1.json`.

## Oracle Xenia Linux — cycle 1604

- Le bundle Canary Linux `907d92b` avec profil portable et intervention hôte
  déclarée `strace -f -qq -e trace=none` progresse visuellement deux fois
  jusqu'à l'intro Project Aces sous Xvfb/Vulkan/SDL dummy.
- Le contrôle sans `strace`, à binaire, profil et XEX identiques, reste figé :
  les captures 8/20 s sont byte-identiques et aucun pixel ne change.
- Le contournement est donc causal sur cette machine, mais reste une
  intervention d'oracle affectant signaux et scheduling, jamais une
  dépendance du produit. Menu, input, mission, cadence et pixels exacts restent
  non qualifiés. Preuve :
  `reports/cycle-1604-xenia-linux-strace-verification.md`.

## Debugger Xenia Linux — cycle 1605

- Deux runs bornés confirment que le mode `--debug` est désactivé pendant
  l'initialisation : `Stack walker unimplemented on posix`, puis
  `Disabling --debug due to lack of stack walker`.
- `break_on_start` suspend avant le premier thread guest sans PC exploitable ;
  sur un guest vivant, `Break and Show Guest Debugger` affiche la même absence
  de mode debug. Aucun PC, thread ou registre PPC n'est qualifié.
- Xenia Linux reste donc un oracle visuel sous intervention `strace`, pas un
  oracle de debug guest. Preuve :
  `reports/cycle-1605-xenia-linux-debug-oracle.md`.

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

## Borne briefing/map retail — cycle 1072

- Le census CPU des entrées DATA.TBL 210–224 établit une famille répétée
  `BRDB`/`BMAP`/`SWG`/`NTXR`/`RIFF`; les SWG sont nommés
  `briefing_ms01`…`briefing_ms15`. Les quinze racines et leurs tailles sont
  hashées dans `reports/cycle-1072-retail-briefing-package-boundary.md`.
- Pour l'entrée 210, `BRDB` (`c46c824a...`) et `BMAP` (`c50823a...`) sont des
  feuilles de données carte/briefing; `briefing_ms01` contient 618 chaînes de
  widgets et zéro occurrence de `Aerial`, `Defence`, `Gracemeria`, `objective`,
  `wave` ou `target`.
- Le résultat est une classification négative qualifiée : ces paquets ne sont
  pas promus comme propriétaire des objectifs ou vagues de gameplay. Le scan
  des 926 entrées n'a toujours aucun propriétaire `SubMisTbl`, `ComTbl`,
  `Maneuver`, condition d'objectif ou identité unité/vague qualifiée.

## Prochain test discriminant

Fermer l'owner/consumer des records binaires de scénario par une acquisition
dynamique étroite et hashée, avec transition native vers `MissionExecution`.
Le gate `retail_objectives` reste ouvert; les fixtures P6 et la preuve bridge ne
peuvent pas le fermer. Ne pas rouvrir selector, manager, UpHud, DATA.TBL[119]
ou le renderer sans preuve contradictoire.

## Borne owner/consumer objectifs-vagues — cycle 1073

- La passe statique canonique ne qualifie aucune paire owner/consumer retail.
  La frontière est réduite à `0x820A7F48`, ses constructeurs directs
  `0x822A6560`, `0x822A8570`, `0x820A8E08`, et le consommateur générique
  `0x822707C8`.
- Aucun selector 4, record stable, champ variant ou insertion
  `UnitManager`/`MissionManager` n'est établi. L'hypothèse historique d'un
  créateur derrière `state40=8, selector44=4, type28=6 → type28=8` reste ouverte.
- Artefact : `reports/cycle-1073-static-objective-wave-owner-boundary.md`.

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
- Le contrat `analysis/contracts/mission01-native-gate-v2.json` passe désormais J0/J1
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

## Frontière d’entrée du créateur retail — 2026-08-06

- Les runs bridge instrumentés 1041–1043 ont conservé `SDL_AUDIODRIVER=dummy`
  et un GPU Vulkan réel, mais se sont arrêtés avant le HUD gameplay à
  `state40=8, selector44=4, type28=6`.
- Les hooks read-only de `0x820A7F48`, `0x822A6560`, `0x822A8570` et
  `0x820A8E08` n’ont produit aucun appel. Il n’existe donc aucune identité
  retail nouvelle pour une unité, une vague ou un objectif.
- Cette frontière ne passe aucun gate et ne réfute pas le créateur retail.
  Le prochain run doit fermer l’entrée storage/save avant toute nouvelle
  instrumentation créateur; le census générique du cycle 1035 reste fermé.
- Rapport : `reports/cycle-1043-bridge-unit-factory-entry-boundary.md`.

## Frontière storage bridge depuis la source courante — 2026-08-06

- Une reconstruction externe fraîche en `AC6_EXPERIMENT_LANE=bridge` avec le
  source tree `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11` atteint sur profil neuf
  `type28=6 → selector44=7/type28=8 → selector44=8/type28=10`. Le binaire est
  hashé `c82042b60b78d2e2b69733a70499eb1243a9c5fb6e47b3db0fd70dc1b814a30e`.
- Le contrôle stock précédent reste à `state40=8, selector44=4, type28=6`.
  Cette différence ferme la dépendance de la transition aux interventions
  bridge `save-dialog-synthesis, force-cvars, fallback-allocator`; elle ne
  qualifie aucun comportement natif.
- Le recipe d’observation sans entrée a publié 180 `PRESENT` supplémentaires
  après la cinématique. Le log contient 17 266 PRESENT, 200 enregistrements de
  tâche Mission 01 et 583 entrées d’input canonique, mais aucune identité retail
  d’unité/vague et aucune preuve native HUD. La capture affiche encore le
  briefing retail; elle reste bridge-only.
- La prochaine frontière est le handoff fenêtre/input vers le gameplay et son
  équivalent natif, pas une nouvelle exploration storage ou renderer.
- Rapport : `reports/cycle-1047-current-source-bridge-storage-and-window-boundary.md`.

## Frontière probe texte bridge — 2026-08-06

- Deux builds diagnostiques ont été exécutés sur le GPU Vulkan réel avec le
  recipe déterministe de handoff. Le build avec préambule PPC forcé et le build
  isolé avec logger activé mais préambule désactivé n'atteignent pas
  `type28=37` après la route initiale.
- Aucun log `[ac6-text*]`, aucune identité de texte d'objectif et aucun
  événement d'unité/vague n'est produit. Ces runs ne qualifient donc ni la
  présence ni l'absence d'un corpus texte retail.
- Le macro de diagnostic reste externe et ne doit pas entrer dans le runtime
  natif. La frontière causale reste le handoff gameplay puis l'identité du
  créateur/registre retail, pas une interprétation de chaînes non capturées.
- Rapport : `reports/cycle-1050-bridge-text-probe-window-boundary.md`.

## Frontière factory retail après handoff gameplay — 2026-08-06

- Le run bridge courant atteint réellement le handoff Mission 01 :
  `type28=30 → 37 → 35 → selector44=3 → type28=6 → 8 → 10`, puis la
  transition de campagne `1→2`. Il ne s'agit plus d'une précondition de menu.
- Le hook read-only `0x820A7F48` observe 128 créations : le joueur est
  `selector=1`, un objet `selector=3` a le vtable `0x82009AB0`, et 126
  `selector=4` ont le vtable `0x82009440`.
- Le census `0x822707C8` reste `object_count=230`, avec le joueur
  `0xB2470000/0x820568D4`, un enfant `0xB2470100/0x82007A10`, zéro
  `other_player`, et 228 objets génériques `0x82009440`. Aucun registre
  d'unité, publication de vague, faction, cible ou transition d'objectif n'est
  capturé.
- Le briefing bridge montre `Invasion of Gracemeria` et
  `Aerial Defence (Air-to-Air)`. Cette chaîne est une observation retail
  hashée, pas une table scénario ni une preuve native ; elle ne passe pas
  `retail_objectives`.
- La prochaine frontière causale est l'identité du créateur/registre de vague,
  pas une nouvelle exploration du raster, de la caméra ou des textures.
- Rapport : `reports/cycle-1057-bridge-retail-factory-gameplay-boundary.md`.

## Fermeture statique des familles Mission 01 — cycles 1067–1068

- Les entrées DATA.TBL 9–23 ont été extraites et fermées en parallèle sur le
  CPU. Les quinze racines sont des FHM valides, avec zéro parser note et des
  identités SHA-256 enregistrées dans
  `reports/cycle-1067-retail-mission-family-closure-boundary.md`.
- La comparaison entry 9 contre 10–23 distingue les nœuds exactement partagés
  des formes seulement similaires. Les extensions NFIC/Scen sont limitées aux
  entrées 9, 15, 17, 21 et 23 ; les slots binaires 0014/0015 restent sans
  sémantique qualifiée.
- Aucun objectif, vague, faction ou unité retail n'est promu. Le gate
  `units_and_waves` reste ouvert, de même que `retail_objectives`.
- L'A/B bridge du cycle 1068 avec logger mémoire et contrôle intact reste dans
  `type28=0 → 6 → 9`; aucun record task n'a été produit. C'est une borne
  négative diagnostique, pas une preuve native.
- Conclusion raster conservée : raster fill qualified ; raster fill still
  broken dans la voie retail complète tant que la chaîne de contenu n'est pas
  qualifiée ; topology is next boundary ; camera/clipping is next boundary.

## Recapture raster native P5 — 2026-08-06

- Le binaire natif courant a rejoué exactement 1 800 fixed ticks avec le même
  manifeste/replay 1280x720. La recapture indépendante est versionnée sous
  `reports/mission01-native-captures/p5-raster-recapture/`.
- `diagnostic_point_writes=0`, `filled_fragment_writes=822161`, couverture
  finale couleur `361984`, profondeur `361267`, et profondeur joueur
  `0.00241196877..0.00522737764`.
- L'object-ID attribue `42722` pixels finaux au drawable `f16` dans la bbox
  `[452,240]..[1113,457]`; le terrain couvre `257545` pixels dans
  `[0,366]..[1279,718]`. Cela confirme des surfaces intérieures remplies,
  indépendamment des compteurs de draws.
- La conclusion obligatoire reste : raster fill qualified ; raster fill still
  broken pour le contenu retail complet ; topology is next boundary ;
  camera/clipping is next boundary.

## Native HUD, wave et débrief P6 — 2026-08-06

- `ac6-native-hud-acceptance-tests` exerce le chemin natif complet depuis
  `MissionExecution`, `CombatWorld`, `ObjectiveRegistry` et
  `RadioPlaybackService` vers `NativeHudRenderer`.
- La capture live rend réticule, télémétrie, arme, cible verrouillée, radar,
  objectif et radio : `target=4098`, `weapon=7`, `radio=15`, `2250` écritures
  HUD et `2218` pixels uniques à 640x360. La pause est capturée sans
  progression.
- Une `MissionWaveSpawn` native publie l'entité `5000` au tick 2, fait passer
  les unités actives de 3 à 4 et laisse zéro entrée en attente.
- Les branches natives success/failure rendent respectivement un panneau de
  débrief avec `completed_objectives=1` et `failed_objectives=1`.
- Le bundle versionné est
  `reports/mission01-native-captures/p6-native-hud/`. Son JSON porte
  explicitement `fixture=true` et `retail_semantics_qualified=false` : ces
  preuves ferment les mécanismes natifs `units_and_waves`, `essential_hud` et
  `success_failure_debrief`, mais ne ferment pas `retail_objectives`.

## Borne statique scénario retail — cycle 1070

- Le scan CPU parallèle des 926 entrées DATA.TBL couvre
  `5,424,368,676` octets expansés. Les occurrences `SubMis`, `loadMission`,
  `missionID`, `Destroy`, `Obj` et `Act` sont attribuées aux familles UI,
  briefing/debrief, shaders ou noms d'objets map après fermeture des FHM.
- Aucun corpus imprimable exact `SubMisTbl`, `ComTbl` ou `Maneuver`, aucune
  table d'objectifs et aucune identité qualifiée unité/vague n'est promue.
- Rapport : `reports/cycle-1070-retail-scenario-exhaustive-cpu-boundary.md`.
  La prochaine frontière causale reste l'ownership dynamique d'une table
  binaire/scénario retail ; ne pas inventer les vagues ni les objectifs.

## Handoff aérien et factory retail — cycle 1080

- Le run bridge `/tmp/ac6-cycle-1080-airborne-qualified` atteint le gameplay
  aérien avec le binaire hashé `fb5fea32…8920ae`, sans forçage loadout/launch,
  sur NVIDIA Vulkan et `SDL_AUDIODRIVER=dummy`.
- Les hooks gameplay observent `UpInput`, `UpObj`, `UpCam`, `UpRadio`, le tick
  manager, les mises à jour joueur et la factory `0x820A7F48`. Le census reste
  à 230 objets : joueur `0xB2470000`, enfant `0xB2470100`, zéro autre joueur,
  et 228 objets au vtable `0x82009440`.
- Le dispatcher et l'interpréteur de records sont très actifs, mais le record
  observé est `SWG\0` et aucune insertion de faction, cible, unité, vague ou
  objectif n'est reliée au `UnitManager`/`MissionManager`.
- Entry 119 est lue dans le même run sur toute sa plage stockée : 443
  requêtes `DATA00.PAC`. Le marqueur de bridge registry/consumer reste à
  zéro ; cette corrélation de stockage ne qualifie pas l'entry 119 comme
  propriétaire de scénario.
- Rapport : `reports/cycle-1080-bridge-airborne-owner-boundary.md`.
  Le handoff gameplay est qualifié comme borne bridge ; `retail_units_and_waves`
  et `retail_objectives` restent ouverts. La prochaine frontière est le lien
  exact record/scénario → insertion ou activation gameplay.

## Refactoring runtime et gate v2 — 2026-08-06

- Le runtime natif est réparti en translation units mission, combat/scénario,
  assets/manifests, géométrie/raster, renderer et frontend. `main.cpp` est un
  dispatcher court ; les anciennes façades restent compatibles.
- Les dix chargeurs de manifests ciblés valident une instance temporaire et ne
  publient qu'après succès complet ; les caches dérivés de textures suivent la
  même transaction. Les tests couvrent l'invalidité tardive, les doublons et
  la conservation de l'état antérieur.
- Le ratchet local mesure les lignes physiques et les fonctions, exclut les
  artefacts générés/build, et passe sur 55 fichiers sans exemption.
- Le contrat v2 sépare `native_units_and_waves` et `native_objective_flow` de
  `retail_units_and_waves` et `retail_objectives`. J0 et les mécanismes natifs
  sont passés ; la Mission 01 retail reste ouverte sur l'identité exacte des
  vagues/unités et des objectifs.
- La frontière retail n'a pas changé : cycle 1080 réduit le prochain test au
  lien record/scénario → insertion ou activation dans UnitManager/MissionManager.

## Réconciliation historique du contrat retail — 2026-08-14 (superseded)

- À cette date, la cible annoncée était exclusivement le PAL retail
  `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, avec
  `DATA.TBL` SHA-256
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.
- Le projet actif `ghidra-projects/ace-combat-6` a été remplacé par un corpus
  Ghidra 12.1.2 utilisant exclusivement `PowerPC:BE:64:Xenon`. L'ancien
  `PowerPC:BE:64:A2ALT-32addr` est conservé, sans fusion, sous
  `ghidra-projects/historical-a2alt-20260814`.
- L'export de bornes Xenon contient 10 708 fonctions contre 10 645 dans
  l'ancien corpus A2ALT. Preuve machine-readable :
  `analysis/ghidra/canonical-import.json` et
  `analysis/ghidra/canonical-function-boundaries.json`.
- La mise à jour change donc les résultats statiques (frontières et décodage
  VMX128); toute observation bridge antérieure doit être requalifiée avec ce
  projet avant promotion. Aucun run stock ou Xenia n'est requis pour cette
  conclusion.
- Les documents demandés `NATIVE_RECONSTRUCTION_STATUS.md`,
  `DECOMPILATION_PLAN.md`, `CURRENT_PLAN.md` et `reports/handoff/CURRENT.json`
  n'existent pas dans ce dépôt AC6; cette absence est conservée comme dette
  documentaire explicite, sans importer les plans d'un autre produit.
- Le prochain corridor reste `ObjBin -> WeaponBin/DurableBin -> loadout ->
  destruction -> compteur`; il doit être instrumenté en lane `bridge` après
  revalidation statique Xenon. Le probe démo reste hors de cette cible.

## Recapture native post-merge P7 — 2026-08-06

- Après arrêt des workers persistants, le HEAD stable est
  `ce457c5baa3e6c62e8a87516a897aed47b69e4c6`. Le bundle
  `reports/mission01-native-captures/p7-current-main/` est une recapture native
  1280x720 avec le même manifest/replay et 1 800 fixed ticks.
- Le chemin produit enregistre `diagnostic_point_writes=0` et
  `filled_fragment_writes=822161`; la couverture finale est `361984` pixels
  couleur et `361267` pixels profondeur. Le terrain a `257545` pixels uniques
  dans `[0,366]..[1279,718]`; `f16` a `42722` pixels object-ID dans
  `[452,240]..[1113,457]` et `79140` écritures couleur/depth-pass.
- La profondeur est non dégénérée (`0.00241196877..0.0550876558`), le radio
  natif consomme la clé `15`, et `deterministic_replay`, pause, save/resume et
  restart sont vrais. Les PPM/F32/replay restent externes; seuls les PNG,
  metrics et session sont versionnés.
- Conclusion obligatoire : raster fill qualified ; raster fill still broken
  pour le contenu retail complet tant que l'ownership matériel/texture retail
  n'est pas qualifié ; topology is next boundary ; camera/clipping is next
  boundary. La recapture ne promeut pas les vagues ou objectifs retail ni J1.

## Xenia Linux ORACLE_RECOVERY bornée — 2026-08-14

- Le checkout Xenia est propre à `95a5c3ee250f80c3b9d139658649d9ffb6db3eec`;
  `origin/master` est identique après `fetch --prune`. Aucune mise à jour
  n'était nécessaire.
- La matrice Canary atteint le chargement du module et crée la swapchain/audio
  avec `SDL_AUDIODRIVER=dummy`, mais les captures restent noires et identiques.
  `apu=nop` échoue explicitement dans `AudioSystem::RegisterClient`; la variante
  `gpu=null` n'apporte aucune preuve de progression CPU. Cette matrice indique
  `ProfileManager: Found 0 Profiles`, ce qui est un prérequis manquant, pas une
  cause suffisante : le cycle 1540 a chargé un profil jetable et reste noir
  après création des threads, audio et bind réseau.
- `break_on_start` confirme l'entrée du debugger mais aucun PC guest n'est
  lisible sans l'UI debugger locale. La frontière Linux/Xvfb non fermée reste
  donc après lancement invité et avant le premier `PRESENT` de contenu; ce
  résultat ne constitue ni une preuve négative sur le guest ni une promotion
  native. Le détail est consigné dans le rapport, réconcilié avec le cycle
  1540.
- Rapport borné : `reports/cycle-1603-xenia-linux-oracle-recovery.md`.
  Les logs/captures restent temporaires sous `/tmp`; aucune source AC6 n'a été
  modifiée et aucun processus Xenia/Xvfb ne reste actif.
