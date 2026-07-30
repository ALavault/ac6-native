# Cycle 343 — les primitives sont émises, pas absentes

## 1. Mesure

Compteurs par trame de notre runtime, lus dans sa propre surcouche :

| écran | émis / réussis / dessins hôte | texte visible | im/s |
|---|---|---|---|
| écran-titre | **44 / 43 / 43** | **oui** — « ACE COMBAT / Fires of Liberation » | 58,7 |
| écran de sauvegarde | **56 / 56 / 56** | **non** — fond + OUI/NON seuls | 61,6 |

Oracle au même point : navigateur GAME DATA complet (FILE 01/02/03, lignes
MISSION / DIFFICULTY LEVEL / CAMPAIGN FLIGHT TIME), « Load file 01? »,
OUI/NON, pied « (A) OK / (B) CANCEL ».

## 2. Lecture

**L'écran de sauvegarde émet plus de dessins que l'écran-titre — 56 contre 44 —
en affichant beaucoup moins.** L'écran-titre rend son texte avec 44 dessins ;
l'écran de sauvegarde en dépense 56 pour un fond et deux boutons.

**Inférence, explicitement marquée comme telle :** la couche manquante est
vraisemblablement **émise mais invisible**, non **absente**. Si l'invité ne
soumettait pas la liste de fichiers et les libellés, le compte de trame devrait
*chuter* sous celui du titre, pas le dépasser.

Ce n'est pas une preuve. 56 dessins pourraient en principe ne couvrir que le
fond, le panneau et les deux boutons, la couche de texte n'étant jamais soumise.
Trancher demande d'attribuer les dessins à leur contenu, pas de les compter.

Aucune comparaison numérique directe avec l'oracle n'est faite : Xenia et ce
runtime n'accumulent ni ne découpent leurs dessins de la même façon, et opposer
leurs totaux produirait un chiffre sans signification. L'oracle sert ici de
référence de **contenu**, ce pour quoi il est qualifié.

## 3. Observations connexes, non expliquées

Relevées sur l'écran de sauvegarde, à ne pas confondre avec la cause :

- `frame-end viewport: 0x0` et `signature viewport: 0x0 (0% x 0% du
  frontbuffer)` — un viewport nul, alors que le fond et les boutons se rendent
  bien ; il s'agit donc du suivi de signature, pas du rendu lui-même ;
- `swap source: unknown` et `present classification: unknown`, là où
  l'écran-titre annonce `guest_swap_texture` ;
- `frame guest MATE: 0` — aucun matériau invité pour cette trame.

Le dernier point est le plus prometteur : une couche de texte dessinée **sans
matériau** serait précisément « émise et invisible ».

## 4. Front suivant

1. Attribuer les 56 dessins : combien portent du texte, avec quel matériau et
   quelle texture. C'est la mesure qui transforme l'inférence du §2 en fait.
2. Vérifier pourquoi `frame guest MATE` vaut 0 ici et non sur l'écran-titre —
   les acquis `MATE_STRUCTURE_REPORT.md` et
   `AC6_MATERIAL_TEXTURE_LINK_REPORT.md` sont des entrées, pas à refaire.
3. L'oracle headless reste disponible pour trancher toute question de contenu
   en une exécution.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
