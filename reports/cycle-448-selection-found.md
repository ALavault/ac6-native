# Cycle 448 — le champ de sélection est trouvé : deux mots qui **échangent** leurs valeurs

## 1. La région statique, enfin balayée

`kScanOrigin = 0x82000000`, 32 Mo, dense, déclenché sur le front `DPAD_LEFT`.
Le reste du dispositif était déjà validé au cycle 447.

Contrairement au tas — un seul flottant qui dérivait — la région statique
répond franchement : six blocs modifiés au passage de l'appui.

## 2. Le couple décisif

```
0x82A53428 : 0x00000040 -> 0x00000004
0x82A5342C : 0x00000004 -> 0x00000040
```

**Deux mots adjacents qui échangent leurs valeurs.** `0x40` passe du premier au
second, `0x04` fait l'inverse. C'est la signature d'une sélection entre deux
entrées : le bouton actif prend une valeur, l'autre reprend celle qu'il avait.

C'est très exactement le comportement visible à l'écran — YES et NO qui
échangent leur surlignage.

## 3. Le contexte autour

Le bloc `0x82A50000` porte une structure régulière, pas seulement ce couple :

| adresse | avant | après |
|---|---|---|
| `0x82A533EC` | `0x00000861` | `0x00003861` |
| `0x82A533F0` | `0x0E00000D` | `0x0EF10003` |
| `0x82A53414` | `0x0E00000E` | `0x0EF10004` |
| `0x82A53438` | `0x0E00000A` | `0x0EF10005` |
| `0x82A5345C` | `0x0E000010` | `0x0EF10006` |
| `0x82A53480` | `0x0E000009` | `0x0EF10007` |

Les mots en `0x0Exxxxxx` progressent régulièrement (`0003`, `0004`, `0005`,
`0006`, `0007`), espacés de 36 octets — une table d'éléments d'interface, mise à
jour d'un coup.

Un autre bloc, `0x82918978`–`0x829189E4`, montre quatre compteurs espacés de 36
octets passant de ~`0x2EF9` à ~`0x2FAD`.

## 4. Pourquoi cela compte

Pour la première fois de la série, une adresse précise est liée à un effet
visible et vérifiable. `0x82A53428`/`0x2C` peut être lu et écrit à l'exécution :

- le **lire** pendant un appui sur A dirait si la validation modifie quoi que ce
  soit ;
- l'**écrire** permettrait de forcer la sélection et d'observer la suite.

C'est le premier point d'appui manipulable depuis le début du blocage.

## 5. Réserve

L'identification repose sur la forme de l'échange, non sur une preuve. Il faut
la confirmer en rejouant l'appui et en vérifiant que les deux mots
**re-échangent** — un compteur ne revient pas en arrière, une sélection si.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
