# Cycle 350 — les textures sont présentes ; la différence est leur format

## 1. Mesure

Pour chaque dessin, les liaisons de texture du pixel shader résolues via la
constante de fetch qu'elles nomment : adresse invitée, format, dimensions.

**Passe défaillante** (`ps=8F1C48BA92C8E43E`, écran de sauvegarde) :

```
fc=0 type=2 base=03514000 fmt=20 w=256  h=256
fc=0 type=2 base=02953000 fmt=20 w=224  h=64
fc=0 type=2 base=0294A000 fmt=20 w=208  h=48
fc=0 type=2 base=028E9000 fmt=20 w=960  h=264
fc=0 type=2 base=028D0000 fmt=20 w=64   h=720
fc=0 type=2 base=028B7000 fmt=20 w=320  h=180
```

**Passe qui fonctionne** (`ps=1899F02DC6758D8F`, écran-titre) :

```
fc=0 type=2 base=03CE1000 fmt=2 w=1280 h=720
fc=1 type=2 base=03DD1000 fmt=2 w=640  h=360
fc=2 type=2 base=03E21000 fmt=2 w=640  h=360
```

## 2. Ce que cela écarte

**Les textures de la passe défaillante sont présentes et plausibles** :
adresses invitées réelles, dimensions cohérentes avec des éléments d'interface
(256x256, 960x264, 320x180, 64x720, 224x64, 208x48). Aucune n'est nulle ni de
taille zéro.

L'hypothèse « texture liée absente ou vide » est donc **écartée au niveau des
descripteurs**. Réserve explicite : seuls les descripteurs sont vérifiés, pas le
contenu de la mémoire. Un descripteur sain peut décrire une zone non peuplée.

## 3. La différence, et son statut

| | passe défaillante | passe qui fonctionne |
|---|---|---|
| format | **20 = `k_DXT4_5`** (BC3, compressé, alpha interpolée) | **2 = `k_8`** (8 bits, un canal) |
| constantes de fetch | toutes `fc=0` | `fc=0`, `fc=1`, `fc=2` |

Deux différences réelles, aucune démontrée comme cause :

1. **Le format.** La passe qui s'affiche échantillonne du `k_8` — typique d'une
   planche de glyphes en alpha. Celle qui ne s'affiche pas échantillonne du
   **DXT4/5**. Un décodage BC3 fautif rendrait tout transparent en laissant la
   géométrie s'exécuter : exactement le tableau observé.
   **Mais `k_DXT4_5` est bien pris en charge** —
   `kHostFormatDXT4_5Unaligned` et un chemin dédié en
   `vulkan/texture_cache.cpp:2463` — et **aucun avertissement de format non
   supporté n'apparaît** dans le journal de l'exécution. L'hypothèse est donc
   affaiblie : prise en charge ne vaut pas correction, mais rien ne l'appuie.
2. **Toutes les liaisons rapportent `fc=0`** alors que la passe saine utilise
   trois constantes distinctes. Curieux pour un shader déclarant deux liaisons ;
   noté, non expliqué, et **pas retenu comme cause** faute de mesure.

## 4. Bilan des causes, après quatre cycles d'élimination

| cause | statut |
|---|---|
| cible de rendu / mode EDRAM | écarté (348) |
| géométrie hors viewport | écarté (349) |
| test alpha | écarté (349) |
| texture absente ou de taille nulle | **écarté (ici, au niveau descripteur)** |
| contenu mémoire de la texture non peuplé | ouvert |
| décodage DXT4/5 incorrect | ouvert, affaibli |
| pixel shader mal traduit | **ouvert, candidat principal** |

## 5. Front suivant

1. Vider une des textures `k_DXT4_5` liées — par exemple `base=03514000`,
   256x256 — après décodage, et vérifier qu'elle contient autre chose que du
   transparent. Cela tranche « contenu non peuplé » et « décodage fautif » d'un
   coup.
2. Sinon, vider le shader traduit `8F1C48BA92C8E43E` contre
   `1899F02DC6758D8F`.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
