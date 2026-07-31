# Cycle 362 — les deux textures fautives sont nommées

## 1. Association taille de lot -> texture liée

Obtenue en appariant, dans le journal du cycle 361, chaque ligne de dessin de la
passe `472913F460D4B446 / 8F1C48BA92C8E43E` avec la ligne de texture qui la
suit. Aucune exécution supplémentaire n'a été nécessaire.

| lot | sommets | quads | texture | visible |
|---|---:|---:|---|---|
| quad isolé | 4 | 1 | `028B2000` 64x64 | **oui** |
| | | | `028D0000` 64x720 | **oui** |
| | | | `028E9000` 960x264 | **oui** |
| | | | `02953000` 224x64 | **oui** |
| double | 8 | 2 | `0294A000` 208x48 | **oui** |
| chaîne courte | 20 | 5 | **`03514000` 256x256** | **non** |
| chaîne longue | 64 | 16 | **`028B7000` 320x180** | **non** |

Chaque association est observée 144 fois — stable, pas un artefact
d'échantillonnage.

## 2. Lecture

Les deux textures qui ne rendent rien sont **`0x03514000` (256x256)** et
**`0x028B7000` (320x180)**, et ce sont exactement celles des lots multi-quads,
c'est-à-dire des chaînes de glyphes. 256x256 est la dimension classique d'une
planche de caractères.

Les cinq textures qui rendent correctement servent des quads isolés : panneaux,
boutons, barres.

**Les sept sont en `fmt=20` (`k_DXT4_5`).** Le format n'est donc définitivement
pas le discriminant : même format, même passe, même shader — certaines
s'échantillonnent, deux non. L'hypothèse « décodage DXT4/5 cassé », déjà
affaiblie aux cycles 360 et 361, est **écartée** : un décodeur fautif ne
trierait pas ainsi.

## 3. Ce qui reste à distinguer

Entre les deux groupes, tout ce qui a été mesuré est identique sauf l'adresse et
les dimensions. Restent, non mesurés :

1. **le swizzle et le canal échantillonné** — une planche de glyphes porte
   souvent l'information en alpha seul ; un swizzle erroné rendrait zéro sur les
   canaux lus, et seulement pour ce type de texture ;
2. **les mips** — un niveau de base absent donnerait un échantillon nul ;
3. **le mode d'adressage et les bornes** — un clamp ou une bordure mal réglés
   sur ces dimensions précises.

Le point 1 est le plus économique et correspond au motif : ce sont les textures
de texte, pas les textures d'art d'interface, qui disparaissent.

## 4. Front suivant

Journaliser, pour ces deux bases précises, les champs restants de la constante
de fetch : swizzle, `num_format`, `sign_x/y/z/w`, nombre de mips, modes de
clamp — et les comparer à ceux de `028E9000` ou `02953000`, qui fonctionnent.
Une seule exécution, l'instrumentation par dessin existe déjà.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
