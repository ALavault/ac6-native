# Cycle 380 — conclusion : le chargement produit une image vide à partir de données valides

## 1. La lecture arrière n'est pas nécessaire

Le cycle 379 concluait qu'il fallait lire le contenu de l'image hôte. Cette
mesure existe déjà, indirectement, depuis le cycle 360.

**Chaîne d'inférence, chaque maillon mesuré :**

1. forcer l'échantillon de `ps=8F1C48BA92C8E43E` à 1.0 rend l'écran de
   sauvegarde **blanc**, y compris là où le texte devrait être (cycle 360,
   rafale de huit captures identiques) ;
   -> les dessins de glyphes **rastérisent et écrivent bien de la couleur** ;
2. avec la texture réelle, ces mêmes dessins ne produisent rien ;
3. le mélange et le test alpha sont **identiques** à ceux des dessins d'art de
   la même passe, qui s'affichent (cycles 349, 373) ;
   -> l'annulation n'est pas dans le mélange ;
4. la vue d'image est réelle et non nulle (364), la constante de fetch, les
   mips, le pavage et les dispositions sont **identiques** aux textures qui
   rendent (362-364, 371-372, 379) ;
   -> l'échantillonnage lit bien l'image prévue ;
5. les données sources en mémoire invitée sont **non nulles** (368 : 160/256 et
   175/256 octets) ;
6. la dispatch de chargement est émise avec le bon nombre de groupes (374).

**Conclusion :** l'image hôte liée à ces dessins contient des zéros. Le
chargement transforme des données sources valides en une image vide.

C'est une déduction, pas une lecture directe — mais chacun de ses maillons est
mesuré, et aucune alternative ne subsiste après vingt-quatre éliminations.

## 2. Le défaut, nommé

**Le *load shader* d'indice 4 (chemin `k_DXT4_5` pavé) produit une image vide
pour `03514000` (256x256) et `028B7000` (320x180)**, alors qu'il fonctionne pour
d'autres textures du même format sur le même chemin — `18D6C000` (1280x720),
visible à l'écran (cycle 378).

La seule variable restante entre ces cas est la **dimension**. Les deux échecs
mesurent 256x256 et 320x180 ; le succès connu, 1280x720.

## 3. Ce qui reste à faire

Examiner le *load shader* 4 sur ces deux géométries précises. Deux pistes
concrètes, issues du cycle 371 :

- `03514000` est la seule texture **sans aucun rembourrage** (`row_pitch` =
  `x_ext_blk x 16`, `array_stride` = `row_pitch x y_ext_blk`) ;
- `028B7000` est la seule dont la **hauteur en blocs est impaire** (45).

Un calcul d'index qui suppose un rembourrage non nul, ou une hauteur paire,
échouerait exactement sur ces deux cas et sur aucun autre.

## 4. Portée honnête

Le défaut est **localisé et nommé**, il n'est pas corrigé. P1.3 reste bloquée,
la première mission ne se joue pas, P2 à P7 ne sont pas entamées.

`recompiler-generated` n'est pas `verified`.
