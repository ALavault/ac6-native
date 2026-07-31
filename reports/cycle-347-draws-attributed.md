# Cycle 347 — les dessins sont attribués : la couche est bien émise, sous un couple de shaders identifié

## 1. Instrument

Journal par dessin posé **dans le processeur de commandes Vulkan**
(`VulkanCommandProcessor::IssueDraw`), là où passe réellement le trafic — et
non via `ac6_render_capture`, branché sur le chemin D3D invité (cycle 346).
Derrière `ac6_log_vulkan_draws`, faux par défaut.

```
[ac6-draw] prim=N verts=N idx_count=N vs=<hash> ps=<hash>
```

## 2. Mesure, et le témoin qui compte

Répartition des 200 derniers dessins, par couple de shaders :

| écran | couple | dessins |
|---|---|---:|
| **titre** (texte **visible**) | `vs=C049A8C9E556F129 ps=0000000000000000` | **192** |
| | `vs=BBAADA3605B82C5A ps=1899F02DC6758D8F` | 4 |
| | `vs=0A6D1DD7767FDF27 ps=2E372EA28CC404B7` | 4 |
| **sauvegarde** (texte **absent**) | `vs=C049A8C9E556F129 ps=0000000000000000` | **175** |
| | `vs=472913F460D4B446 ps=8F1C48BA92C8E43E` | **21** |
| | `vs=0A6D1DD7767FDF27 ps=2E372EA28CC404B7` | 4 |

Part de dessins **sans pixel shader** : titre **13 667 / 14 541**,
sauvegarde **1 608 / 1 873**.

## 3. Une quatrième conclusion nulle, évitée par le témoin

Le premier réflexe était : « 175 dessins sur 200 sans pixel shader — un dessin
sans PS n'écrit aucune couleur, voilà la couche invisible ». **Le témoin le
réfute** : l'écran-titre, où le texte se rend parfaitement, en compte
**192 sur 200**. `ps=0` domine les deux écrans ; ce n'est pas le défaut.

C'est la quatrième fois en cinq cycles qu'une observation nulle allait devenir
une conclusion — `MATE = 0` (343), « la capture noircit » (344), « capture
inerte donc rendu cassé » (345). La seule chose qui a changé ici, c'est d'avoir
mesuré le témoin **avant** d'écrire la conclusion, pas après.

## 4. Ce que la mesure établit réellement

L'écran de sauvegarde émet **21 dessins entièrement ombrés par trame**
(`vs=472913F460D4B446 / ps=8F1C48BA92C8E43E`), contre **4** pour l'écran-titre
avec son propre couple. Or il n'y affiche qu'un fond, un panneau et deux
boutons — de l'ordre de quatre à six quads.

**Vingt et un dessins ombrés pour quatre à six éléments visibles.** La couche
manquante est donc bien **émise**, avec un vrai pixel shader, et n'apparaît pas.
L'inférence du cycle 343 cesse d'être une inférence sur ce point précis, et le
défaut se localise sur **un couple de shaders nommé**.

Réserve : « 21 dessins pour 4 à 6 éléments » suppose qu'un élément visible coûte
environ un dessin. C'est plausible pour de l'interface en quads, ce n'est pas
prouvé ici.

## 5. Front suivant

1. Isoler ce que font les 21 dessins de `vs=472913F460D4B446 /
   ps=8F1C48BA92C8E43E` : cible de rendu, viewport, ciseaux, mélange, test de
   profondeur. Une couche écrite hors viewport, entièrement transparente ou
   rejetée au ciseau produit exactement ce tableau.
2. Comparer ce couple à celui du titre (`BBAADA3605B82C5A / 1899F02DC6758D8F`),
   qui, lui, aboutit à l'écran — c'est le témoin naturel.
3. L'oracle headless reste disponible pour toute question de contenu.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
