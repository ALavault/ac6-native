# Cycle 456 — contrat d'affichage AC6 remis en 1280×720

## Périmètre qualifié

- Cible : Ace Combat 6 PAL, Xenon PPC big-endian, image base `0x82000000`.
- XEX retail SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Runtime Linux final SHA-256 :
  `4fa071fcaf1879feb58ab8ec205142dbf8c3fa8fb5f961296b85a59361cc02b7`.
- Aucun fichier de `generated/` n'a été modifié.

## Cause confirmée

`ApplyAc6DefaultSettings()` imposait `1920×1080` à la fois au mode vidéo
rapporté à l'invité et à la fenêtre native. Le frontbuffer AC6, la chaîne de
présentation observée et l'écran d'acceptation restent tous `1280×720`.
L'image 1080p était donc tronquée par la racine Xvfb 720p : les éléments se
centraient vers `x=960` et la moitié droite sortait de la capture.

Le correctif définit un contrat AC6 unique dans `src/ac6_display_defaults.h` :
`1280×720`, preset `720p`. `src/main.cpp` l'applique aux cinq valeurs par
défaut (`video_mode_*`, `resolution`, `window_*`) sans supprimer la possibilité
d'une surcharge utilisateur explicite.

## Validation

- Reconfiguration CMake : PASS, action
  `ce1bb38805076553cf39cc3f029f7967a8d1dc7c37e852d8c287d124b86d34e0`.
- Build final : PASS, action
  `93ac966b6f6ac11a40e573407c5218eb422af76f78b37f2230cb227b96bb23e5`.
- Tests natifs AC6 : **5/5 PASS**, action
  `4d257b9bc43576ba77de9dcec021feb0eae5bc4f5c0f0b8484662437953fcf04`.
- Replay vierge complet : PASS, action
  `927a946b0e981da12fba2378c27e03d314d690393f003710bc4e3aac77f295a7`.
- Trace : `reports/logs/cycle-456-720p-dialog/ac6recomp.log`, SHA-256
  `74ebb1599ced8af50a4b88103ea7e2ec62fc4b69c40c63a214b5de872faccfa2`.
- La présentation journalisée est constamment
  `packet/src/guest_output=1280×720`.
- Capture du dialogue à 47 s : `1280×720`, SHA-256
  `f3cc0588b1de50d1f1eb5077d888605dc72fe5379c3fa8d868d5fa4d800d01ae`.
- L'oracle est capturé dans une fenêtre `1280×800`; son contenu invité commence
  à `(4,50)`. Une fois cette bordure retirée, le panneau oracle et le panneau
  natif occupent les mêmes coordonnées `x=174..1105`, `y=220..455`.
- Le passage de sauvegarde reste acquis : réponses 1 aux types 30, 37 et 35,
  état externe 8, puis écran stable `GAME DATA` aux captures 66 s et 72 s.
  `save.dat` (129112 octets) et `not_00000000.dat` (524296 octets) sont créés
  dans le stockage isolé de la preuve.
- `git diff --check`, `bash -n tools/ac6-run.sh` et absence de `bin/bin` : PASS.

Le corpus PPC complet n'a pas été relancé : son assembleur SDK non exécutable
(code 126) est un blocage déjà qualifié.

## Risques résiduels

- La résolution et la géométrie du panneau sont corrigées. Le texte de secours
  reste une surimpression native et non le rendu de police retail ; son
  placement vertical constitue une frontière visuelle distincte.
- Un essai de recalage de cette surimpression a produit deux SIGBUS précoces au
  premier chargement de texture, avant la création de fenêtre. Il a été retiré.
  Le binaire final est bit-à-bit celui du replay complet réussi.
- La première notice `OK` de l'oracle reste absente, frontière déjà consignée
  au cycle 455 ; elle n'empêche pas le passage fonctionnel.

État : mismatch de résolution corrigé et validé dynamiquement ; parité de la
police/notice initiale non revendiquée.
