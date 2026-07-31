# Cycle 378 — d'autres textures DXT4/5 empruntent ce chemin, dont une qui s'affiche

## 1. Mesure, sur les journaux déjà collectés

Textures en `format=20` (`k_DXT4_5`) passant par
`LoadTextureDataFromResidentMemoryImpl`, toutes exécutions confondues :

```
028B7000  320x180    <- fautive
03514000  256x256    <- fautive
18C23000  280x360
18C52000  128x128
18D6C000  1280x720
18E1B000  512x64
```

## 2. Lecture

**Quatre autres textures DXT4/5 empruntent ce chemin.** `18D6C000` fait
1280x720 : c'est très probablement une trame de la cinématique d'introduction —
laquelle **s'affiche correctement** (captures des cycles 345, 359, 371).

L'hypothèse implicite qui restait — « ce chemin est cassé pour `k_DXT4_5`, et
nos deux textures sont les seules de ce format à l'emprunter » — est donc
**écartée** : le format et le chemin fonctionnent ensemble ailleurs.

Corrélation incidente, notée sans la surinterpréter : `18C52000` figure aussi
dans la liste des **vues nulles** du cycle 364. Elle échoue donc à se lier, pour
une raison distincte de celle de nos deux planches, qui obtiennent bien une vue
réelle.

## 3. Bilan de la session sur P1.3

Vingt-trois causes éliminées, toutes par mesure avec témoin de vivacité. Sur les
deux planches de glyphes, toute propriété observable est identique à celle de
textures qui rendent : données sources, chargement, dispatch, disposition
invitée et hôte, format, constante de fetch, vue d'image, chemin de chargement,
et maintenant le couple format+chemin.

Le seul maillon **jamais observé** reste le **contenu de l'image hôte après
écriture**. Toutes les mesures ont porté sur ce qui est déclaré ou émis, jamais
sur le résultat.

## 4. Front suivant, unique et précis

Lire les octets de l'image Vulkan après la dispatch de chargement, pour
`03514000` et `028B7000` et pour une texture témoin qui rend — par exemple
`18D6C000`, DXT4/5 sur le même chemin et visible à l'écran, ce qui en fait le
témoin idéal.

Si l'image hôte est vide alors que la source ne l'est pas (cycle 368), la
transformation est fautive et le *load shader* devient examinable ligne à ligne.
Si elle est pleine, le défaut est en aval de l'écriture, dans l'échantillonnage.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
