# Cycle 361 — les lots manquants sont des chaînes de glyphes

## 1. Mesure

Répartition des dessins de la passe défaillante
(`vs=472913F460D4B446 / ps=8F1C48BA92C8E43E`), 40 derniers, tous `prim=13`
(liste de rectangles — la primitive d'interface) :

| dessins | sommets | quads | lecture |
|---:|---:|---:|---|
| 23 | 4 | 1 | quad isolé — panneau, bouton |
| 6 | 8 | 2 | |
| 6 | 20 | 5 | |
| 5 | 64 | **16** | **chaîne de glyphes** |

## 2. Lecture

Une liste de rectangles à 64 sommets est **seize quads en un lot** : c'est la
forme d'une chaîne de texte rendue glyphe par glyphe. Les lots à 20 sommets sont
des chaînes plus courtes.

Les quads isolés (4 sommets) sont les panneaux et les boutons OUI/NON — **ceux
qui s'affichent**. Les lots multi-quads sont les libellés, les en-têtes et les
lignes du navigateur GAME DATA — **ceux qui manquent**.

La partition demandée au cycle 360 est donc faite, et elle sépare exactement
visible et invisible.

## 3. Ce que cela ajoute au cycle 360

Le cycle 360 a établi que la passe peint bien l'écran et que le défaut est
**par texture**. Ce cycle nomme la texture concernée : **celle qu'échantillonnent
les lots de glyphes**, c'est-à-dire la planche de caractères.

Les panneaux et boutons échantillonnent une autre texture de la même passe, et
elle fonctionne. Le défaut est donc circonscrit à **la texture de glyphes**, non
au format `k_DXT4_5` en général — puisque les deux groupes sont en `fmt=20`
(cycle 350).

Cela **affaiblit encore** l'hypothèse « décodage DXT4/5 cassé » : un décodeur
fautif ne trierait pas entre deux textures du même format dans la même passe.

## 4. Front suivant

1. Isoler, parmi les six adresses de base du cycle 350, celle liée aux dessins à
   20 et 64 sommets — un journal par dessin associant `verts` et `base` suffit.
2. Comparer son descripteur à celui d'une texture qui fonctionne : dimensions,
   mips, signedness, mode d'adressage. Le cycle 350 a déjà les six ; il manque
   l'association avec la taille du lot.
3. La planche de glyphes est vraisemblablement une texture d'alpha ; vérifier le
   swizzle et le canal échantillonné est le premier soupçon raisonnable.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
