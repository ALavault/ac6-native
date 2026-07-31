# Cycle 370 — les sept textures de la passe ont des paramètres de chargement identiques

## 1. Mesure, sans échantillonnage

Les sept textures de la passe défaillante, journalisées sans biais
d'échantillonnage (le biais avait déjà masqué deux mesures, cycles 350 et 365) :

```
028B2000  64x64    load_shader=4 signed=false tiled=1 packed_mips=0 dim=1   rend
028D0000  64x720   load_shader=4 signed=false tiled=1 packed_mips=0 dim=1   rend
028E9000  960x264  load_shader=4 signed=false tiled=1 packed_mips=0 dim=1   rend
0294A000  208x48   load_shader=4 signed=false tiled=1 packed_mips=0 dim=1   rend
02953000  224x64   load_shader=4 signed=false tiled=1 packed_mips=0 dim=1   rend
028B7000  320x180  load_shader=4 signed=false tiled=1 packed_mips=0 dim=1   NE REND PAS
03514000  256x256  load_shader=4 signed=false tiled=1 packed_mips=0 dim=1   NE REND PAS
```

*(L'étiquette imprimée est générique pour les sept ; seule la donnée compte.)*

## 2. Résultat

**Tous les paramètres de chargement sont identiques.** Même *load shader*, même
pavage, mêmes mips, même dimensionnalité, même signedness.

`load_shader=4` est donc **disculpé** : il produit des textures qui s'affichent
correctement dans cette même passe.

## 3. L'état de la contradiction

Pour ces deux textures, tout ce qui a été mesuré est identique aux cinq qui
fonctionnent :

| propriété | identique ? |
|---|---|
| format (`k_DXT4_5`) | oui (362) |
| constante de fetch complète — swizzle, num_format, endianness, filtres | oui (363) |
| vue d'image liée, non nulle | oui (364) |
| chargement réussi | oui (365) |
| invalidation | jamais, pour les deux groupes (366) |
| données sources présentes | oui (368) |
| load shader, pavage, mips, dimension | **oui (ici)** |
| **échantillon** | **non — zéro contre correct** |

**Seules les dimensions diffèrent** : `320x180` et `256x256` d'un côté,
`64x64`, `64x720`, `960x264`, `208x48`, `224x64` de l'autre.

Dix-huit causes éliminées, toutes par mesure avec témoin.

## 4. Front suivant

La dimension est désormais la **seule** variable non écartée. Piste à tester :
le calcul d'adressage/pavage dépend de la largeur, et `320` comme `256` sont des
multiples de 32 alors que `960`, `208`, `224`, `64` ne le sont pas tous —
vérifier le pas de pavage calculé pour chaque texture, et non plus seulement les
paramètres déclarés.

## 5. Note de protocole, corrigée par l'opérateur

**A (`Space`) permet de sauter la cinématique d'introduction.** Cela rend les
exécutions à cache froid bien moins coûteuses : plus besoin d'attendre la fin de
l'introduction. Utilisé dans ce cycle, entrée à 22 s au lieu de 33 s.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
