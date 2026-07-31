# Cycle 379 — les paramètres de mip sont identiques, et nuls

## 1. Mesure

Sélection de mip, jamais instrumentée jusqu'ici, pour les sept textures :

```
028B2000  mip_min=0  mip_max=0  mip_address=00000000   rend
028D0000  mip_min=0  mip_max=0  mip_address=00000000   rend
028E9000  mip_min=0  mip_max=0  mip_address=00000000   rend
0294A000  mip_min=0  mip_max=0  mip_address=00000000   rend
02953000  mip_min=0  mip_max=0  mip_address=00000000   rend
028B7000  mip_min=0  mip_max=0  mip_address=00000000   NE REND PAS
03514000  mip_min=0  mip_max=0  mip_address=00000000   NE REND PAS
```

## 2. Résultat

Identiques pour les sept, et nuls : aucune de ces textures n'a de mips, et
l'échantillonnage est contraint au niveau 0 partout.

L'hypothèse « `mip_max_level` sélectionne un niveau jamais peuplé » est
**écartée**. Vingt-quatre causes éliminées.

## 3. Épuisement des propriétés statiques

Ce cycle clôt l'inventaire : **toute propriété statique atteignable depuis le
processeur de commandes et le cache de textures a été mesurée**, et toutes sont
identiques entre les deux planches fautives et les cinq qui rendent :

format, constante de fetch complète (swizzle, num_format, endianness, filtres,
pavage, pas, exp_adjust), mips, vue d'image, chemin de chargement, load shader,
dispatch et nombre de groupes, disposition invitée calculée, disposition hôte,
présence des données sources, invalidation, ordre de dessin, viewport, ciseau,
test alpha, cible de rendu, mode EDRAM.

Il n'existe plus de paramètre à comparer. La cause ne peut donc pas être une
différence de configuration : elle est dans un **résultat** que rien n'a encore
lu.

## 4. Ce qui reste, sans équivoque

Une seule mesure n'a jamais été faite : **lire le contenu de l'image hôte après
la dispatch de chargement**.

- textures visées : `03514000`, `028B7000` ;
- témoin : `18D6C000` — DXT4/5, même chemin de chargement, visible à l'écran
  (cycle 378) ;
- verdict attendu : image vide alors que la source ne l'est pas (cycle 368) =>
  la transformation est fautive, et le *load shader* s'examine ligne à ligne ;
  image pleine => le défaut est en aval, dans l'échantillonnage.

Cette mesure demande une lecture arrière GPU, seule opération de cette enquête
qui ne se réduise pas à une ligne de journal.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
