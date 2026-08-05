# AC6 native Linux — état de recherche

Mise à jour : 2026-08-05T18:20:00+02:00

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
- Le seul correctif renderer de ce slice est le raster fill natif : edge
  barycentrique unique, depth `[0,1]`, clipping NDC conservatif et mode points
  diagnostic explicite. Aucun correctif shader, texture, resolve, MRT, input ou
  HSM n'a été appliqué. Les runs bridge restent `bridge`, jamais promus `stock`.

## Faits qualifiés

- `default.xex` : SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- `DATA.TBL` : 14 824 octets, 926 entrées, 2 packs, SHA-256
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.
- Corpus généré gelé : 54 fichiers C/C++, tree SHA-256
  `f42fa2c4c1ec3bfb061003ef7074f73881e968ef2719f7f78e59190d1c5af73d`.
- Mission 01 atteint une frame avec HUD, terrain/sky runtime et entrées de vol
  dans la lane `bridge`; le monde visible reste noir.
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

 Qualifier la frontière caméra/clipping à partir de la capture native remplie :
 conserver les triangles partiellement visibles, mesurer les surfaces collées
 aux bords et expliquer la bbox terrain `[0,0]..[1279,718]`. Ne pas rouvrir les
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
- Le contrat `analysis/contracts/mission01-native-gate.json` est fail-closed :
  `world_visible` et `player_aircraft_visible` sont revenus `open` après la
  preuve raster, car la capture précédente ne distinguait pas un nuage de
  points d'une surface remplie. J1 reste ouvert.
- Le binaire natif expose désormais `--combat-headless`. Le probe manifesté
  verrouille la cible hostile 4098 depuis le joueur 4097, tire l'arme 7 deux
  fois, produit deux événements de dégâts, passe la santé de 100 à 0 et réduit
  les unités actives de 3 à 2. L'artefact externe `combat-v1.json` est hashé
  `cca1e77db38cc8096bfd1a89d3f47a75c0831507107cd81d9f93072ea3c013a3`.
  Cette preuve ferme uniquement les mécaniques natives ciblage/armes/
  dégâts-destruction; elle ne qualifie aucune identité retail de vague ou
  d'objectif.

## Prochain gate

Qualifier caméra/clipping et visibilité finale du joueur à partir de l'object-ID
native; ne pas utiliser la capture bridge comme preuve J0/J1 et ne pas passer
aux textures avant cette frontière.
