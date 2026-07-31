# Cycle 385 — mise en garde sur l'attribution « lots multi-quads = texte manquant »

## 1. L'inférence en question

Depuis le cycle 361, l'enquête repose sur cette lecture :

- lots de 4 sommets (un quad) = panneaux et boutons -> **visibles** ;
- lots de 20 et 64 sommets (5 et 16 quads) = chaînes de glyphes -> **manquants**.

Elle a été **déduite** des compteurs de sommets, jamais **vérifiée** contre ce
que l'écran affiche réellement.

## 2. La tension qu'elle laisse

`YES` et `NO` sont **du texte, et ils s'affichent**. Trois et deux caractères,
soit cinq quads si le jeu les dessine glyphe par glyphe — exactement la taille du
lot de 20 sommets, celui que j'attribue à `03514000`, la texture réputée
défaillante.

Deux lectures, non départagées :

1. `YES`/`NO` sont des **libellés pré-rendus** posés sur un quad chacun ; alors
   l'inférence tient, et les lots multi-quads sont bien le texte manquant ;
2. `YES`/`NO` sont **composés de glyphes** ; alors le lot de 20 sommets est
   visible, `03514000` s'échantillonne correctement, et une partie du
   raisonnement des cycles 361 à 384 porte sur la mauvaise cible.

Rien dans les mesures accumulées ne tranche : aucune n'associe un lot de dessin
à une **zone de l'écran**.

## 3. Pourquoi le signaler maintenant

Vingt-sept causes ont été éliminées en supposant cette attribution. Si elle est
fausse, les éliminations restent valides *en tant que faits sur ces deux
textures*, mais leur **pertinence** pour le texte manquant tombe.

C'est le même type de faille que celles corrigées aux cycles 344, 351, 358 et
367 : une prémisse plausible, jamais mesurée, qui oriente le travail.

## 4. Mesure qui tranche, et elle est simple

Forcer l'échantillon à blanc **pour la seule texture `03514000`** (l'outil et la
restriction par hachage existent depuis le cycle 359, il faut y ajouter une
restriction par constante de fetch) et regarder **quelle zone** blanchit :

- si `YES`/`NO` blanchissent -> ils sont composés de glyphes, `03514000` rend
  bien, et l'attribution est fausse ;
- si une zone vide blanchit -> l'attribution est juste et la zone du texte
  manquant est localisée à l'écran.

C'est la première mesure de cette enquête qui relierait un dessin à des pixels.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
