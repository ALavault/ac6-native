# Cycle 345 — la capture ne noircit rien : elle n'enregistre rien

## 1. Correction du cycle 344

Le cycle 344 affirmait qu'activer `ac6_render_capture` « noircit le rendu dès
le départ ». **C'est faux.** Mesuré :

```
capture active: yes
fenêtre applicative présente, compteurs qui avancent
eop=5545  guest_swap_requests=1850  host_swap_presents=1848
capture d'écran : 416 780 octets, cinématique d'introduction bien rendue
```

Le jeu rend normalement, capture activée. Les images unies de 9 865 octets du
cycle 344 venaient de captures d'écran prises **avant que la fenêtre ait du
contenu** — le journal de cette même exécution portait pourtant 1 715 lignes
`PRESENT`, ce qui aurait dû m'alerter : un runtime qui présente 1 715 trames
n'a pas un rendu noirci.

J'aurais dû vérifier la présence et la géométrie de la fenêtre avant de
conclure. Troisième erreur de cette famille en trois cycles — `MATE = 0`
(cycle 343), « capture noircit » (cycle 344) — toutes du même type : **conclure
d'une observation nulle sans vérifier que le canal d'observation était vivant.**

## 2. Le vrai défaut, plus net

Avec la capture **active** :

```
capture active: yes
capture frame: 3655
capture draws / clears / resolves:      0 / 0 / 0
capture indexed / shared / primitive:   0 / 0 / 0
capture rt0 switches / unique rt0:      0 / 0
backend total issue / success / host draw: 97021 / 97021 / 95169
```

**La capture est active et n'enregistre rien**, pendant que 97 021 dessins hôte
sont émis. L'instrument n'est pas destructeur : il est **inerte**.

Cela explique aussi pourquoi `frame guest MATE` reste à 0 même capture activée —
le compteur d'identité de matériau est alimenté par ce même chemin, qui ne voit
aucun dessin.

## 3. Conséquence

Le seul instrument d'attribution des dessins existant est débranché du chemin
de rendu réellement utilisé. Le runtime est en `mode: hybrid_backend_fixes`,
renderer autoritaire **Vulkan**, alors que la capture vit dans `src/d3d_hooks.cpp`
— un chemin **D3D**. Hypothèse à vérifier, pas un fait : la capture instrumente
un backend qui n'est plus celui qui dessine.

C'est cohérent avec tout ce qui est mesuré : `capture draws 0/0/0`,
`frame guest MATE 0`, et `backend total` qui compte bien, lui, des dizaines de
milliers de dessins par un autre chemin.

## 4. Front suivant

1. Vérifier l'hypothèse §3 : la capture est-elle branchée sur D3D alors que le
   rendu passe par Vulkan ? Une lecture des points d'appel de
   `ac6::d3d::OnFrameBoundary` et de leurs équivalents Vulkan suffit.
2. Si oui, l'attribution des 56 dessins doit se faire côté Vulkan — journaliser
   texture et programme liés par dessin dans le processeur de commandes Vulkan,
   et non via `ac6_render_capture`.

## 5. État

L'inférence du cycle 343 — couche « émise mais invisible » — reste non tranchée.
P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
