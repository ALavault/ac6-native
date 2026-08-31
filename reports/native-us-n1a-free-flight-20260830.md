# AC6 natif NTSC-U/J — N1a free-flight qualifiée (2026-08-30)

## Décision

N1a est verte. Deux processus Linux natifs distincts ont exécuté exactement
1 800 ticks de Mission 01 avec le même script de cinq contrôles. Leurs replays,
leurs sept captures Vulkan et leurs manifestes sont identiques octet par octet.
Les deux processus terminent avec le statut 0.

Cette décision ne promeut ni `jv_eligible`, ni combat, ni objectif, ni
progression. `jv_eligible=false` reste explicite jusqu'à N1b.

## Chemin produit qualifié

- Le cache NTSC-U/J scellé `d7071928…1a34df5b` ouvre le monde M01 et une unité
  joueur placée ; aucune origine non placée n'est utilisée en repli.
- `RetailSession` ouvre la caméra mode 2 et publie à chaque tick la pose, la
  base d'orientation et la caméra joueur live.
- Le script fixe applique successivement pitch, roll, yaw, throttle et frein
  aux kernels input/vol existants. La position est produite par l'intégrateur
  de vol contracté via le chemin externe, jamais par l'intégrateur générique de
  `MissionRuntime`.
- Le même processus rend par Vulkan 4 226 instances cité, 65 536 cellules
  terrain, l'eau, le F-16 retail et le HUD GPU.
- Le reçu N1a refuse PAL, caméra diagnostique, capture CPU interactive,
  loadout autre que F-16/arme 1, durée autre que 1 800 ticks, save/resume et
  absence de replay. Le marker cible est désactivé pendant la qualification.

## Monde Vulkan

Le test store-backed NTSC-U/J final rapporte :

- `runtime_draw=4226`, `terrain_draw=65536` ;
- `runtime_meshes=4740`, `runtime_textures=169` ;
- eau : 65 536 cellules lues, 31 191 visibles, un draw batché ;
- F-16 : 4 435 sommets, 6 468 indices source, un draw, texture GIDX
  `0x10002215` ;
- HUD vert : 112 pixels contractuels à chacun des sept jalons ;
- contraste : luminance 0–255 à chaque jalon.

Le cadrage arrière reste serré et N1a ne revendique aucune parité pixel ou
caméra avec l'oracle. Elle qualifie la composition live et sa visibilité.

## Deux runs déterministes

Les deux manifests ont le SHA-256
`743924995c2bf46d318a4e1e3d24918cd4c008e7e9f6b21dd0cc3dea5869012c`.
Le digest d'évidence interne commun est
`5354cb5a3ff60ab88d363476a0af945c3472627ccc0e95959ef45dc66e5d7b2e`.

| Tick | Fenêtre | Pixels changés | Effet sémantique | Effet visuel |
| ---: | --- | ---: | --- | --- |
| 300 | neutre | 0 | vrai | vrai |
| 540 | pitch | 121 996 | vrai | vrai |
| 780 | roll | 193 751 | vrai | vrai |
| 1 020 | yaw | 235 086 | vrai | vrai |
| 1 260 | throttle | 279 803 | vrai | vrai |
| 1 500 | frein | 283 519 | vrai | vrai |
| 1 800 | récupération | 339 199 | vrai | vrai |

`comparison.log` prouve `cmp=0` pour le replay, les sept PPM et
`receipt.json`. `run-a.exit`, `run-b.exit` et `comparison.exit` valent tous 0.

## Validation

- build ciblé final : vert ;
- audit complexité : vert ;
- tests ciblés input/vol/cache Vulkan : 3/3 ;
- CTest complet sous Xvfb et `SDL_AUDIODRIVER=dummy` : 88/88, zéro échec,
  19 skips explicites pour fixtures retail absentes ;
- test scène NTSC-U/J explicite : vert ;
- frontières source et ELF : vertes dans CTest ;
- contrôle fail-closed : caméra diagnostique refusée avec statut 119 avant
  création du replay ou du reçu ;
- smoke Vulkan direct et capture visuelle : verts.

Artefacts principaux :

- `artifacts/native-us-n1a-runs-20260830/run-a/receipt.json` ;
- `artifacts/native-us-n1a-runs-20260830/run-b/receipt.json` ;
- `artifacts/native-us-n1a-runs-20260830/run-a.replay` et `run-b.replay` ;
- `artifacts/native-us-n1a-runs-20260830/comparison.log` ;
- `artifacts/native-us-n1a-runs-20260830/ctest-xvfb-final.log` ;
- `artifacts/native-us-n1a-runs-20260830/us-scene-final.log` ;
- `artifacts/native-us-n1a-runs-20260830/cli-refusal.log` ;
- `artifacts/native-us-n1a-build-20260830/receipt-targets-r4.log` ;
- `artifacts/native-us-n1a-build-20260830/complexity-r3.log`.

## Frontière suivante

N1b doit étendre cette sortie contrôlée à 3 600 ticks et fermer les causes
nommées qui maintiennent `complete_render_scene=false` et
`jv_eligible=false`. Combat/WeaponBin, objectifs, succès, débrief, niveau 2,
save/reload, PAL et optimisation restent hors de N1b.
