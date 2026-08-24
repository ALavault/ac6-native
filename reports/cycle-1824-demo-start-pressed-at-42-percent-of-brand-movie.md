# Cycle 1824 — START est pressé à 42 % du film de marque

> Ce fichier s'appelait `cycle-1824-demo-start-lands-on-a-frozen-splash.md`.
> Renommé parce que sa conclusion corrigée contredit ce titre : le guest n'est
> pas figé. Aucun contrat ni report ne le citait encore.

## Qualification

- Projet Ghidra : `ghidra-projects/ace-combat-6-demo`.
- XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`
  (démo PAL). Aucun corpus retail croisé.
- Binaire : `recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-recomp`.
- Payload : `artifacts/goal-playable/brandlogo-list2-window-runtime-20260823/store`.
- Oracle utilisé : **non**.
- Run : `probe --until frontend --max-ticks 3000 --backend vulkan`,
  **aucune entrée**, aucun override.
  Script : `artifacts/goal-playable/title-start-timing-capture-20260824/run-natural-timeline.sh`.

  **Ce run n'a pas atteint sa borne** : il a été arrêté délibérément après plus
  de trois heures, une fois son livrable obtenu, pour libérer le CPU au profit
  des runs headless. Son `runtime-t3000.status` porte donc un code de signal et
  non le statut qualifié 4. L'image analysée est **la dernière frame titre
  dessinée avant l'arrêt** (environ 2800 ticks), pas « la frame du tick 3000 ».
  Le readback à 1160 ticks du cycle 1811 et celle-ci restent byte-identiques,
  ce qui est le seul point sur lequel ce run est cité.
- Run de contrôle de l'avance de frame :
  `artifacts/goal-playable/swg-frame-advance-current-20260824/`, borne 600,
  statut **4** (borne qualifiée), aucune entrée.

## Hypothèse testée (formulée par l'utilisateur)

> « Le runner envoyait START sur les splash screens ; comme il ne faisait pas de
> captures d'écran, je ne pouvais pas valider. »

## `CONFIRMÉ`

**L'écran affiché au moment où START est envoyé est le splash éditeur
« Bandai Namco Games », pas un écran titre interactif.**

La forme est lisible sans ambiguïté : le sigle à trois lobes, le mot
« Games » et le `™`. Image inspectée humainement :
`artifacts/goal-playable/title-start-timing-capture-20260824/png/frame-live-shape-inv.png`.

La couleur du produit est cassée (tout dans le canal rouge, `G=B=0`), donc
l'image brute n'est pas lisible telle quelle. L'inspection s'est faite sur une
transformation **de lecture seule** : le canal R étiré sur sa plage réelle
(231..255) en niveaux de gris, puis inversé. Aucune modification du renderer,
du readback ou de la sémantique couleur. La forme est intacte dans le produit ;
seule sa restitution chromatique ne l'est pas.

## `PROUVÉ` — la sortie rendue est figée, pas seulement statique

Le readback de ce run est **byte-identique** à celui du cycle 1811, produit le
même jour à 12:41 à la borne 1160 ticks :

```
diff readback(1160 ticks, cycle 1811)  vs  readback(~2800 ticks, ce cycle)
  bbox: None   changed_pixels: 0   verdict: identical
```

L'instrument a été mesuré avant d'être cru, comme l'exige `CLAUDE.md` : sur un
contrôle synthétique où **un seul pixel** est modifié **d'un seul niveau**, il
répond `bbox (640,360,641,361) changed_pixels=1`. « Identique » est donc un
résultat, pas un défaut de sensibilité.

Corollaires mesurés sur le même run :

- 429 draws titre pour seulement **8 frames uniques** — l'écran ne joue pas
  d'animation ;
- deux textures seulement atteignent le chemin de draw titre dans ce run :
  `0x0DF22000` en 64×64 (handle `0x57`) et `0x0DF27000` en 512×512
  (handle `0x58`). Le 1280×720 (`0x59`, `003_NTXR`) n'y apparaît pas.

Les deux readbacks comparés viennent bien du **même binaire** : `ac6-demo-recomp`
est daté du 24 août 12:26 et n'a pas été reconstruit depuis ; les readbacks sont
de 12:41 (cycle 1811) et 14:30 (ce cycle). La comparaison est donc valide.

Outil : `tools/analyze_title_frame_timeline.py` (`timeline`, `describe`,
`diff`).

## `NON JOIGNABLE EN L'ÉTAT` — la preuve guest porte sur un autre binaire

**Correction que ce report s'applique à lui-même.** Sa première version
présentait la preuve guest ci-dessous comme une confirmation indépendante du
même comportement. Elle ne l'est pas : les deux mesures portent sur des
binaires **différents**.

```
binaire de ce cycle (pixels)          1f8c67c44bdb9ea7…   (24 août 12:26)
binaire du run 8000 ticks (mode)      32bd700c0c9f3f02…
```

Deux observables indépendants sur deux binaires différents ne se joignent pas —
et surtout pas dans cette campagne, qui a précisément vécu une régression de
binaire entre les cycles 1804 et 1810. La mesure reste vraie de son binaire ;
elle ne dit rien du courant. Elle est conservée ici comme contexte, pas comme
preuve du run présent :

```
tick  222  StartUp 0x2E7E0080  inner_state 0
tick  266  StartUp             inner_state 1
tick 2426  StartUp             inner_state 2
tick 2429  Title   0x2E3C0100  inner_state 0
tick 2452  Title               inner_state 1
      ...  plus aucun changement jusqu'au tick 8000
```

Sur **ce** binaire, le Title est immobile à l'état interne 1 pendant 5548 ticks.
Établir la même chose sur le binaire courant demande une mesure guest du binaire
courant : c'est le premier travail du cycle suivant, et il est bon marché
(`AC6_DEMO_WATCH_BRANDLOGO_DRAW_SELECTOR` prend un tick de départ).

## Contradiction ouverte à trancher, sur ma propre affirmation

Le run `artifacts/goal-playable/brandlogo-list2-window-runtime-20260823/RESULT.md`
observe au **tick 1125** l'owner `0x2E3CDD10` à `frame_index=0x12C` (300), qui
sélectionne `list_index=2`, `draw_index=2`, et atteint le handle `0x0E000059`.
Ce run s'est arrêté en fail-closed au tick 1131 sur un profil de fetch BC3
1280×720 inconnu du renderer — donc **pas** sur un blocage guest.

Autrement dit : une timeline qui avance jusqu'à la frame 300 et atteint `0x59`
le 23 août, contre un run du 24 août où `0x59` n'atteint jamais le draw titre.
Là encore les binaires diffèrent, et la régression 1804–1810 est dans
l'intervalle. **Cette contradiction n'est pas tranchée**, et elle est plus
importante que le reste de ce report : si la timeline avançait avant et
n'avance plus, le blocage vers le gameplay est une **régression native
identifiable**, pas une frontière guest à découvrir.

## Le mécanisme d'avance de frame, entièrement cartographié

Établi statiquement dans ce cycle (`ppc_recomp.43.cpp`, `0x82323BB8` l. 16291,
`0x82322438` l. 12557) :

| champ | rôle |
|---|---|
| `+0xD5` | enable — testé pour l'incrément (ou callback `[+0x19C]+0xF4`) |
| `+0xD6` | inhibition — doit valoir 0 |
| `+0xD8` | curseur de frame interne |
| `+0xDC` | frame_index publié (`= +0xD8` si `+0xD4 != 0`) |

L'incrément est une instruction unique : `0x82323C08 addi r11,r11,1`, stockée
par `0x82323C0C stw r11,216(r31)`. Le clamp `0x82323D74/0x82323D98` reboucle à 0
au-delà de `(+0x2C − +0x28)/8`.

L'horloge est portée non par le contrôleur mais par l'agrégat SWG :
`sub_82322438(A, 1.0f/60.0f)` accumule `A+0x108 += rate*delta` avec
`rate = *(*(A+0x0C)+0x4C)`, et déclenche une invocation de `0x82323BB8` par
franchissement du seuil `A+0x104`. Deux octets, `A+0x00` et `A+0x01`,
court-circuitent tout, accumulation comprise.

**Aucun hook n'existe sur cette horloge** : `grep 82322438\|0x104U\|0x108U` sur
les sources du bridge ne renvoie rien. Personne n'a jamais observé si
l'accumulateur reste nul. C'est le trou d'instrumentation le plus direct pour
la question posée.

Deux owners dont l'immobilité est **normale et non un blocage** : `0x2E3CE490`
et `0x2E3F1350` ont des tables mono-frame ; le clamp les remet à 0 par
conception. Ne pas les confondre avec l'owner racine `0x2E3CDD10` (2220 frames),
ni avec `0x2E3CED10` — les rapports antérieurs mélangent ces trois adresses.

## `RÉFUTÉ` — la timeline SWG n'est pas gelée, et c'est ma propre hypothèse qui tombe

Ce report avait d'abord conclu « timeline SWG figée sur frame 0 ». **C'est
faux**, et la mesure qui le montre a été faite sur le binaire courant, dans ce
cycle : `artifacts/goal-playable/swg-frame-advance-current-20260824/`, watcher
`AC6_DEMO_WATCH_BRANDLOGO_DRAW_SELECTOR` depuis le tick 190, aucune entrée.

```
tick=222  owner=0x2E3CDD10  enabled=1  frame_index=0xFFFFFFFF
tick=225  owner=0x2E3CDD10  enabled=1  frame_index=0
tick=228  owner=0x2E3CDD10  enabled=1  frame_index=1
tick=231  owner=0x2E3CDD10  enabled=1  frame_index=2
...                                    +1 toutes les trois ticks
```

L'owner racine avance exactement à la cadence documentée. Les deux autres
owners visibles ne bougent pas parce que leurs tables sont mono-frame, ce qui
est **normal et non un blocage** — c'est la confusion que je faisais :

```
owner 0x2E3CDD10  begin=0x2DF17570 end=0x2DF1BAD0  frames=2220
owner 0x2E3CE490  begin=0x2DF01768 end=0x2DF01770  frames=1
owner 0x2E3CED10  begin=0x2DF017A4 end=0x2DF017B4  frames=2
```

J'avais lu l'immobilité d'un owner mono-frame comme une panne, et lu une image
figée comme un guest arrêté. Les deux lectures étaient fausses, et de la même
manière : une observation prise pour une preuve sans le contrôle qui la
qualifie.

## Ce que cela réoriente, correctement cette fois

Le guest joue son film normalement. L'image ne change pas parce que le renderer
ne sait pas dessiner ce que le film demande ensuite : le run
`brandlogo-list2-window-runtime-20260823` a atteint le handle `0x0E000059` puis
s'est arrêté **fail-closed sur un profil de fetch BC3 1280×720 inconnu du
renderer**. Le dernier draw réussi — le splash `0x58` en 512×512 — reste donc à
l'écran indéfiniment. C'est un manque d'affichage, pas un arrêt du jeu, et cela
réconcilie la contradiction ouverte plus haut sans invoquer de régression.

L'arithmétique donne alors le vrai cadrage du problème :

```
frame 0                       tick 225
1 frame / 3 ticks             (frame 59 au tick 402, cadence documentée)
2220 frames                   fin du film ≈ 225 + 3*2219 = tick 6882
START pulsé au tick 3000  ->  frame (3000-225)/3 = 925 sur 2220, soit 42 %
```

**START est pressé à 42 % du film de marque.** C'est exactement l'intuition de
l'utilisateur, désormais chiffrée : ce n'est pas « il manque un producteur pour
armer `manager+0x18` », c'est « on appuie pendant les logos ».

L'oracle Xenia concorde sur la forme, sur le même XEX démo
(`ac6_demo_work/instrumentation-xenia/20260815-134231`, session humaine,
448 événements manette, sortie propre) :

```
15.0 s   DATA.TBL, DATA00.PAC, DATA01.PAC ouverts, rafale de lectures
20-58 s  quasi aucune lecture -- le film joue sur des données déjà chargées
58.55 s  START pressé par l'humain
60 s     514 lectures d'un coup, puis demopack_eng.bin et voicepack_eng.bin
```

La progression splash → titre n'est donc pilotée ni par le disque ni par un
événement externe : elle est pilotée par l'horloge du film. Et l'écran cible est
connu par capture : `ac6_demo_work/.../screenshots/xenia-title-check.png`,
titre « ACE COMBAT 6 / Fires of Liberation » avec `[PRESS START]`.

## Ce qui reste à établir

Presser START **après** la fin du film, vers le tick 7200, et voir si la
transition se produit. C'est le seul test qui décide, et il est en cours
(`artifacts/goal-playable/start-after-brand-movie-20260824/`, backend headless
car la question est causale et le rendu est ~50× plus lent).

Si la transition se produit, la frontière gameplay bascule de « chercher un
producteur manquant » à « piloter l'entrée au bon moment », et tout le travail
`manager+0x18` redevient de la description, pas une frontière.

## Non établi

- **Pourquoi** la timeline SWG imbriquée reste sur sa frame 0. C'est le gate
  suivant, et il est guest-side.
- Que retarder START suffirait. Rien ne le suggère : la séquence ne progresse
  pas, donc aucun instant ultérieur n'est meilleur. À ne pas tenter comme
  correctif avant d'avoir la cause.
- Le comportement au-delà du tick 3000 en backend rendu. Un run
  échantillonné à 8000 ticks est prévu ; il doit dire si l'image change **une
  seule fois** après 3000.
- Aucune correction renderer n'est faite ni justifiée par ce cycle. Décision
  utilisateur : le renderer est « functional-enough », on ne le touche pas.

## Note d'instrument

Le hook `AC6_DEMO_AUDIT_SCREENCAP_DIR` est **inutilisable** pour une
chronologie : il est fail-closed sur l'unique writeback qualifié et fait
trapper le run au premier present non conforme — observé au tick 182,
`« audit screencap source is not the qualified writeback »`. La chronologie
passe donc par `AC6_DEMO_DUMP_READBACK_PPM`, réécrit à chaque draw titre.

Un échantillonneur qui lit ce fichier doit **copier d'abord, valider la taille
(2 764 816 octets), hacher la copie, puis seulement la nommer** : le runtime
écrit non atomiquement, donc hacher la source puis la copier fabrique de
fausses frames « distinctes » à partir d'écritures déchirées. Le script
`run-timeline-sampled.sh` applique cet ordre et journalise les rejets.

## État

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
