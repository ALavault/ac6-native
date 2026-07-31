# Cycle 371 — les dispositions calculées, et ce qu'elles ne séparent pas

## 1. Mesure

Disposition invitée **calculée** (et non déclarée) pour les sept textures de la
passe. Bloc DXT4/5 = 4x4 pixels, 16 octets.

| base | taille | row_pitch | x_ext_blk | x_ext x 16 | y_ext_blk | array_stride | rend |
|---|---|---:|---:|---:|---:|---:|---|
| 028B2000 | 64x64 | 512 | 16 | 256 | 16 | 16384 | oui |
| 028D0000 | 64x720 | 512 | 16 | 256 | 180 | 98304 | oui |
| 028E9000 | 960x264 | 4096 | 240 | 3840 | 66 | 393216 | oui |
| 0294A000 | 208x48 | 1024 | 52 | 832 | 12 | 32768 | oui |
| 02953000 | 224x64 | 1024 | 56 | 896 | 16 | 32768 | oui |
| **028B7000** | 320x180 | 1536 | 80 | 1280 | **45** | 98304 | **non** |
| **03514000** | 256x256 | 1024 | 64 | **1024** | 64 | **65536** | **non** |

## 2. Lecture, honnête

Deux singularités apparaissent, mais **aucune ne couvre les deux échecs** :

- `03514000` est la **seule** texture sans aucun rembourrage : `row_pitch` égale
  exactement `x_ext_blk x 16`, et `array_stride` égale exactement
  `row_pitch x y_ext_blk`. Toutes les autres, y compris `028B7000`, sont
  rembourrées.
- `028B7000` est la **seule** dont l'extension verticale en blocs est **impaire**
  (`y_ext_blk = 45`). Toutes les autres sont paires.

Chaque anomalie n'explique qu'un des deux échecs. Il n'existe donc **pas** de
champ unique, parmi ceux mesurés, qui sépare les deux groupes.

Deux lectures possibles, non départagées :

1. deux défauts distincts — un lié au rembourrage nul, un lié à une hauteur
   impaire en blocs — qui produisent le même symptôme ;
2. un défaut unique dans un calcul plus en aval, dont ces deux dispositions sont
   simplement deux cas limites.

La seconde est plus économique mais n'est pas démontrée.

## 3. Observation à ne pas surinterpréter

`320x180` est exactement le quart de `1280x720`, et `256x256` est la seule
dimension carrée en puissance de deux. Ces formes évoquent des cibles de rendu
plutôt que des textures de disque. Le cycle 367 a toutefois mesuré qu'aucune
résolution ne les vise. Noté, non retenu.

## 4. Front suivant

Comparer, pour ces deux textures, la disposition **hôte** (image Vulkan) à la
disposition invitée mesurée ici : c'est le point où un rembourrage nul ou une
hauteur impaire en blocs se traduirait en copie tronquée ou décalée. Le code est
juste en aval, dans le même appel.

Dix-huit causes éliminées ; celle-ci n'en élimine aucune mais fournit la donnée
brute qui manquait.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
