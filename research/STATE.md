# AC6 native Linux — état de recherche

Mise à jour : 2026-08-05T14:35:00+02:00

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
- Aucun correctif Vulkan, shader, texture, resolve, MRT, input ou HSM n'a été
  appliqué dans ce slice. Les runs restent `bridge`, jamais promus `stock`.

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

Joindre dans un run gameplay courant le registre et le consommateur de
`DATA.TBL[119]`, puis reprendre le graphe borné
`draw -> render target -> resolve -> composite -> swap`. La première expérience
renderer devra relier les draws monde aux pixels; elle ne pourra pas se baser
sur un compteur de draws ou un bind non nul.
