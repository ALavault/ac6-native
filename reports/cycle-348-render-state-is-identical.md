# Cycle 348 — l'état de rendu est identique entre l'écran qui marche et celui qui ne marche pas

## 1. Mesure

Journal par dessin étendu à l'état susceptible de rendre invisible un dessin
pourtant émis : mode EDRAM, tuile de destination, format, pas, MSAA.

Derniers 300 dessins, groupés :

| écran | couple de shaders | dessins | état de rendu |
|---|---|---:|---|
| **titre** (texte **visible**) | `C049A8C9E556F129 / 0` | 288 | `edram_mode=4 color_base=0 color_fmt=0 pitch=1280 msaa=0` |
| | `BBAADA3605B82C5A / 1899F02DC6758D8F` | 6 | **idem** |
| | `0A6D1DD7767FDF27 / 2E372EA28CC404B7` | 6 | `pitch=640 msaa=2` (présentation) |
| **sauvegarde** (texte **absent**) | `C049A8C9E556F129 / 0` | 260 | **idem** |
| | `472913F460D4B446 / 8F1C48BA92C8E43E` | **35** | **idem** |
| | `0A6D1DD7767FDF27 / 2E372EA28CC404B7` | 5 | `pitch=640 msaa=2` (présentation) |

`edram_mode = 4` est `kColorDepth` (`xenos.h:877`) — le mode **normal**, pas une
anomalie. Vérifié plutôt que supposé : la valeur 4 évoque un « copy » dans
d'autres API, ici elle désigne le rendu couleur+profondeur.

## 2. Ce que cela élimine

**L'état de rendu est identique entre les deux écrans** : même mode, même tuile
EDRAM de destination (`color_base=0`), même format, même pas, même MSAA.

Sont donc écartés, par témoin :

- une couche écrite dans un autre rendu-cible jamais résolu ni présenté ;
- un mode EDRAM sans écriture couleur ;
- une surface de géométrie ou d'échantillonnage différente.

La passe de contenu de l'écran de sauvegarde émet **35 dessins** contre **6**
pour celle du titre, vers **la même cible, dans le même mode**, et n'affiche
presque rien.

## 3. Ce qui reste, et c'est plus étroit

L'invisibilité ne vient d'aucun état mesuré ici. Restent quatre causes
possibles, toutes en aval de ce qui a été instrumenté :

1. **la géométrie** — sommets dégénérés, ou transformés hors du viewport ou du
   ciseau ;
2. **les textures liées** — une texture absente ou vide rend un quad
   transparent ;
3. **le mélange** — un état de blend qui annule la contribution ;
4. **le pixel shader lui-même** — `8F1C48BA92C8E43E` mal traduit, écrivant du
   transparent.

La quatrième est la plus consistante avec l'ensemble : même état, même cible,
même chemin, seul le couple de shaders change entre ce qui s'affiche et ce qui
ne s'affiche pas. Elle relève de la phase P3 (traduction des shaders Xenos),
pas du chemin de commandes.

Ce n'est pas démontré. C'est le candidat que la mesure suivante doit départager.

## 4. Front suivant

1. Vider le shader `8F1C48BA92C8E43E` traduit et l'inspecter : écrit-il une
   couleur non nulle ? Comparer avec `1899F02DC6758D8F`, celui du titre, qui
   aboutit à l'écran — c'est le témoin naturel, déjà identifié.
2. À défaut, journaliser viewport, ciseau et textures liées pour les 35 dessins,
   ce qui départage les causes 1 à 3 en une exécution.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
