# Gate courant autoritaire — pourquoi le thread 1 ne revient plus attendre (cycle 1828)

## RÉFUTÉ : aucun événement ne manque au chemin ring

Le journal de publications était un `std::array<…, 32U>` qui cesse
d'enregistrer au 33ᵉ événement. Les deux côtés de l'A/B du cycle 1826 l'avaient
saturé : ce cycle mesurait donc la composition des 32 premières publications,
pas le run. Porté à 1024, le relevé complet donne :

```
site invité      HSIO=1   HSIO=0        clés distinctes : 61 contre 60
0x821A61F0          52       52
0x821A688C           2        2
0x821A6AC4         970      970
```

Les seize handles `0xE0000088…0xE00000C8`, donnés pour absents sous HSIO=1,
sont publiés **une fois chacun des deux côtés**. Seule `0xE0000050` est
réellement exclusive (27 contre 0) : c'est la clé d'attente du thread 13, que le
régime d'interruption réveille — signature attendue, pas anomalie.

Ne pas rouvrir « quel événement manque » : la question n'a pas d'objet.
Ne pas rouvrir « le streaming s'arrête » : 55 lectures contre 61.

Preuve : `artifacts/goal-playable/publication-ab-cap1024-20260824/RESULT.md`.

## Question

Le seul contraste qui survit vient de compteurs non plafonnés :

```
thread 1, NtSignalAndWaitForSingleObjectEx @0x821A69CC
   HSIO=1      5 appels, dernier au tick 177
   HSIO=0   5611 appels, dernier au tick 2999
```

Le thread 1 ne dort pas faute d'être réveillé : **il ne va plus se coucher**.
Pourquoi cesse-t-il d'appeler son handshake après le tick 177 ?

C'est une question côté invité. Point de départ : le corps qui contient le
callsite dont le retour est `0x821A69CC`, et le prédicat qui y mène ; puis ce
que le régime d'interruption (`0x821B9768 -> 0x821C5190`, actif dès le tick 0)
change dans l'état que ce prédicat lit.

## `done_when`

Le prédicat qui fait sauter le handshake est nommé, avec l'état qu'il lit et
qui diffère entre les deux chemins — ou une preuve négative bornée nomme la
frontière suivante.

## Leçon d'instrument à retenir

Trois affirmations commitées sont tombées faute d'avoir mesuré l'instrument
avant de le croire : deux au cycle 1826 (publications, streaming) et une au
cycle 1807. Toutes trois venaient d'une liste tronquée. Avant de conclure d'une
absence dans un journal, vérifier sa capacité et s'il l'a atteinte.

---

# Gate courant autoritaire — le régime d'interruption graphique (cycle 1827)

## Fermé : la divergence est au tick 0, pas au tick 177

Sur 1782 arêtes contre 1930, **dix seulement existent exclusivement sous
HSIO=1**, toutes sur les threads 2 et 13, actives dès le tick 0 :

```
t2   0x821B9768 -> 0x821C5190   x1021   ticks 0..2998
t2   0x821B9768 -> 0x822E4240   x936
t2   0x821B9768 -> 0x822E4268   x935
t2   KfAcquireSpinLock / KfReleaseSpinLock / KeQueryPerformanceFrequency  x1021
t2   KeAcquireSpinLockAtRaisedIrql / KeReleaseSpinLockFromRaisedIrql      x2892
t13  RtlEnterCriticalSection / RtlLeaveCriticalSection @0x822E4324/48     x935
```

Deux arêtes seulement sont exclusives à HSIO=0, et ce sont des `DbgPrint` au
tick 0.

`0x821C5190` est l'interface de callback de `VdGlobalDevice+0x4084` qu'un gate
antérieur avait laissée « sans cible qualifiée ». Elle en a une : elle dispatche
vers `0x822E4240`/`0x822E4268` et réveille le thread 13.

Rien ne détourne le thread 1 au tick 177 : il cesse son handshake parce que
l'invité attend désormais l'interruption pour piloter la complétion. C'est une
conséquence du régime, pas sa cause. Le thread 1 n'est pas bloqué — il tourne
encore au tick 2997 avec 1 858 344 `RtlEnterCriticalSection`.

Preuve : `artifacts/goal-playable/ring-interrupt-regime-20260824/RESULT.md`.

## Question

L'interruption arrive 2892 fois et réveille le thread 13, et pourtant le thread
9 cesse ses `NtReadFile` au tick 205. Que fait `0x821C5190` côté hôte, et que
devrait-il publier pour que le streaming reprenne ?

## `done_when`

Le maillon entre l'interruption graphique et la reprise des lectures est nommé,
ou une preuve négative bornée nomme la frontière suivante.

## Tension à ne pas masquer

Les arêtes `0x82323A4C -> 0x82323468` et `0x82326660 -> 0x82325DF8` tombent de
1002 à 1 sous HSIO=1, ce qui suggère une timeline SWG arrêtée — mais une mesure
directe de ce cycle
(`artifacts/goal-playable/swg-frame-advance-current-20260824/`, binaire HSIO=1)
montre l'owner racine avançant normalement jusqu'à la frame 85. Les deux ne sont
pas réconciliées. Ne pas conclure sur la seule foi des arêtes.

---

# Gate courant autoritaire — ce qui détourne le thread 1 au tick 177 (cycle 1826)

## Fermé : l'événement est identifié

Le thread invité 1 exécute un handshake `NtSignalAndWaitForSingleObjectEx` au
LR **`0x821A69CC`** (signal `0xE0000048`, attente `0xE000004C`). Sous HSIO=0 il
l'exécute **5611 fois** jusqu'au tick 2999 ; sous HSIO=1 **5 fois**, la
dernière au tick **177**, puis plus jamais.

`0xE000004C` est publié dans les deux runs : l'événement ne manque pas côté
hôte, le thread cesse de venir l'attendre. Il part en boucle indirecte
`0x822E559C -> 0x822F8848` (1 039 227 appels) et ne se bloque plus.

Cascade mesurée : plus de blocage → 2998 épuisements de tranche sur 3000 → le
thread 9 cesse ses `NtReadFile` au tick 205 (contre le tick 2369 sous HSIO=0)
→ les dix-sept publications depuis le LR `0x821A61F0` disparaissent → le
contenu du titre ne charge pas → StartUp ne franchit jamais son état 2.

Corrige le cycle 1807, qui lisait les seize handles `0xE0000088…0xE00000C8`
comme une publication **en trop** du binaire courant : ce sont au contraire les
publications du chemin qui fonctionne.

Preuve : `artifacts/goal-playable/ring-path-missing-wait-20260824/RESULT.md`.

## Question

Qu'est-ce qui, autour du tick 177, détourne le thread 1 de sa boucle de
handshake ? Son dernier appel y signale `0xE0000054` et attend `0xE0000058`.

Fenêtre très bornée : quelques ticks autour de 177, un seul thread, LR de
sortie connu, boucle d'accueil `0x822E559C -> 0x822F8848`.

## `done_when`

Le site invité qui fait quitter la boucle est nommé, avec le prédicat qui
dépend de la valeur HSIO — ou une preuve négative bornée nomme la frontière
suivante.

## Contexte établi au cycle 1825

Un seul mot arbitre ring contre progression, et il est mesuré à une variable
près (`artifacts/goal-playable/hsio-mode-transition-tradeoff-20260824/`) :

```
HSIO=1 (HEAD)  ring 502 soumissions   mode bloqué à StartUp, Title jamais publié
HSIO=0         ring 0 soumission      Title publié au tick 2369, état 1 au 2385
```

Signature ordonnanceur : HSIO=0 donne 23 threads bloqués, 0 runnable, 215
épuisements de tranche sur 3000 ; HSIO=1 donne 22 bloqués, 1 runnable, **2998**
épuisements. Sur le chemin ring, l'invité attend des événements que le port ne
rend pas.

`kVdHsioTrainingSucceededResult` reste à **1**. Ne pas le passer à 0 : cela
échangerait un blocage contre un autre et masquerait le défaut.

## Question

Quel événement l'invité attend-il sur le chemin ring, et pourquoi n'est-il
jamais publié ?

Première passe sans nouvelle instrumentation : les rapports de probe portent
déjà, par thread, `wait_key`, `wait_lr`, `wait_kind`, `wake_tick`, plus la
liste `event_publications`. Comparer les deux côtés sur le **premier thread qui
se bloque sans réveil** et remonter son `wait_lr`.

## `done_when`

Le premier `wait_key` bloqué sans publication correspondante est nommé, avec le
site invité qui l'attend et le producteur natif qui devrait le publier — ou une
preuve négative bornée nomme la frontière suivante.

## Ne pas re-parcourir

Réfutés par mesure dans le cycle 1825 : régression de binaire entre le 22 et le
24 août (témoin et courant rigoureusement identiques) ; différence de store
(byte-identique au store neutre) ; différence de code invité (même manifeste
codegen) ; et l'objet même de « quel producteur arme `manager+0x18` depuis
START » — sous HSIO=0 la chaîne StartUp→Title se produit seule, sans START.

---

# Gate précédent — piloter START au bon moment (cycle 1825)

## Ce qui est établi, et ce qui est RÉFUTÉ

`CONFIRMÉ` (hypothèse utilisateur) : START tombe sur le splash éditeur Bandai
Namco Games. Image inspectée :
`artifacts/goal-playable/title-start-timing-capture-20260824/png/frame-live-shape-inv.png`.

`RÉFUTÉ`, et c'était l'hypothèse du cycle 1824 lui-même : « la timeline SWG est
gelée ». Elle ne l'est pas. Mesure sur le binaire courant
(`artifacts/goal-playable/swg-frame-advance-current-20260824/`) : l'owner racine
`0x2E3CDD10` va de la frame 0 au tick 225 à la frame 85 au tick 480, soit
exactement `(480-225)/3`. Les owners `0x2E3CE490` (1 frame) et `0x2E3CED10`
(2 frames) sont **mono/bi-frame par conception** — leur immobilité n'est pas une
panne, et c'est la confusion qui a produit le faux diagnostic.

Longueurs mesurées (`(end-begin)/8`) : racine **2220 frames**, d'où fin du film
à `225 + 3*2219 = tick 6882`. START au tick 3000 est donc la frame
`(3000-225)/3 = 925` sur 2220, soit **42 % du film**.

L'écran figé s'explique sans blocage guest : le film demande ensuite le handle
`0x0E000059` (attendu au tick ~1125), et le renderer échoue fail-closed sur son
profil de fetch BC3 1280×720 ; le dernier draw réussi reste affiché. C'est un
manque d'affichage, pas un arrêt du jeu, et cela réconcilie la contradiction
avec `brandlogo-list2-window-runtime-20260823` sans invoquer de régression.

## Question

START pressé **après** la fin du film (tick ≥ 6882) produit-il la transition ?

Run en cours : `artifacts/goal-playable/start-after-brand-movie-20260824/`,
START au tick 7200 tenu 10 ticks, borne 9000, backend headless (la question est
causale ; le rendu est ~50× plus lent — le run vulkan à 3000 ticks a pris plus
de trois heures).

## `done_when`

Soit une transition de mode est observée après l'appui, et la frontière gameplay
devient « piloter l'entrée au bon moment » — auquel cas un run rendu produit
ensuite la capture validée automatiquement et inspectée humainement. Soit rien
ne bouge, et il faut alors chercher ce que le film produit à sa dernière frame
et pourquoi le titre n'est pas armé.

## Attention aux pièges déjà payés

- Les boutons de `--input-at` sont en **décimal** (`std::from_chars` base 10) :
  `0x0010` est rejeté, START vaut `16`.
- Ne pas attendre sur un fichier `runtime.status` qui peut être résiduel d'un
  essai précédent — attendre le processus, ou effacer l'état avant de relancer.
- Le hook `AC6_DEMO_AUDIT_SCREENCAP_DIR` est fail-closed et fait trapper un run
  quelconque au tick 182 ; pour une chronologie visuelle, passer par
  `AC6_DEMO_DUMP_READBACK_PPM`.
- Le watcher `AC6_DEMO_WATCH_BRANDLOGO_DRAW_SELECTOR` plafonne à 512 événements,
  soit ~tick 480 avec trois owners actifs. Au-delà, il faut relever le cap ou
  filtrer sur un seul owner.

## Rappel de méthode

Le renderer est « functional-enough » par décision utilisateur : ne pas le
corriger, ne pas corriger la couleur, ne pas substituer de ressource, ne pas
forcer de draw. Le fil couleur reste parké.

---

# Gate fermé — timing réel du pulse START (cycles 1823–1824)

## Redirect de priorité utilisateur — 2026-08-24

L'utilisateur a redirigé la campagne : **focus sur l'atteinte du début du
gameplay**, et **toute run produit des captures pour validation automatique et
humaine**.

Son intuition, à tester en premier : *le runner envoyait START pendant les
splash screens*; faute de captures au moment du pulse, cela n'a jamais pu être
validé. Elle est cohérente avec une phrase de `STATE.md` jamais exploitée — « le
logo Namco appartient à une époque nettement antérieure au prompt visible
"PRESS START" » — et avec le fait qu'aucune capture n'existe au tick 3000.

## Expérience en cours

Run naturel **sans aucune entrée**, borné à 8000 ticks, une capture d'audit par
présentation qualifiée. Script :
`artifacts/goal-playable/title-start-timing-capture-20260824/run-natural-timeline.sh`.

`done_when` : la chronologie des frames distinctes est établie sur [0,8000) et
l'on sait si un titre acceptant l'entrée apparaît, et à quel tick. Si oui, le
gate suivant renvoie START à ce tick-là et non à 3000. Si le splash ne se
termine jamais, la frontière devient le producteur qui doit le faire avancer.

## Gates suivants prévus

1. Writers de `Title+0x48`/`+0x49` (statique) — `manager+0x5F` est bloqué par
   `Title+0x48=0` et `manager+0x5D` annulé par `Title+0x49=1`; un writer
   naturel de ces champs serait l'arête d'armement manquante.
2. Les **deux gardes restantes** de `sub_8217E258` (route forcée, instrument
   seulement) : `[0x2E3C0200+12] == 1` et `[0x2E3C0200+136] == 0`, plus la
   valeur de sortie du handler. Watch live jusqu'à 8000 ticks.

   **Ne pas** repartir sur le flag de readiness `[*0x827435F8+0x222BFE]` : il
   est **réfuté comme blocage**. Il est rempli à `0xFEFEFEFE` par le
   pool-poison de l'allocateur au tick 4 et jamais réécrit ; or la garde est
   `flag != 0`, et `0xFE` la satisfait déjà. Le forcer à 1 n'a rien changé —
   tous les signaux sont restés octet pour octet identiques à la baseline.
   Preuves : `reports/AC6_DEMO_THE_LOADING_TASKS_READINESS_FLAG_IS_ALLOCATED_ONCE_AND_NEVER_WRITTEN.md`
   et `reports/AC6_DEMO_CORRECTING_THE_GATE_3_FRAMING_FORCING_THE_FLAG_CHANGES_NOTHING_BECAUSE_IT_WAS_ALREADY_TRUE.md`.

   Trou de fenêtre à couvrir : la mesure « poll indéfiniment » (741 appels
   M150) couvrait les ticks **4251–4999**, alors que l'état de mode ne se fixe
   à 1 qu'au tick **5414**. Si `[primary+12]` est ce champ d'état, la garde 1
   bascule à 5414 et personne n'a re-mesuré le poll après. La watch doit donc
   aller jusqu'à 8000 et journaliser la **réponse** du handler, pas seulement
   ses champs.
3. Handler de l'item de stream post-START `+0x1788` (« opcode 2 puis arrêt ») —
   seul contenu post-START dont le handler est inexpliqué. Vérification, pas
   pari.

## Ne pas re-parcourir

Pistes déjà réfutées : EndMode comme commande de complétion manquante ; payload
littéral `{6,0xE04}` ; SendMsgI/M102/M150 comme verrou ; durée de maintien de
START ; attract-advance ; listener global `0x826DF804` ; boucle
`E000004C/0x821A8C88` ; flag 9 de `CSwgCallback` ; transition directe dans
`[2990,3041)`.

---

# Gate PARKÉ par redirect utilisateur (2026-08-24) — builder du transform parent du draw titre

**Statut : parké, ni réfuté ni supersédé.** Ce gate appartient au fil couleur
`BF` du logo titre (cycles 1810–1822). Les trois contrats mission01 portent
`"visual_parity_out_of_scope": true` : aucun contrat n'exige le fond blanc, et
le `done_when` correspondant était auto-imposé au cycle 1811. Les résultats
1812→1822 restent valides et la frontière ci-dessous est conservée intacte pour
reprise après l'atteinte du gameplay. Voir
`reports/cycle-1823-demo-consolidation-and-gameplay-pivot.md`.

## Hypothèse

Un des deux appelants externes de `0x82323BB8`, `0x82322438` ou `0x82324118`,
construit ou reçoit le transform racine utilisé par le `MovieController` du
titre. Son dataflow détermine ensemble les composantes
`+0x40/+0x44/+0x48/+0x4c/+0x5c` qui deviennent RGBA dans le record P1.

## Expérience statique unique

Extraire les définitions de `r3/r4` aux callsites externes
`0x82322438` et `0x82324118`. Identifier par flux de données lequel reçoit le
contrôleur titre, puis remonter uniquement le builder de son argument `r4`
jusqu'aux cinq stores `+0x40/+0x44/+0x48/+0x4c/+0x5c`.

## `done_when`

L'appelant titre, le type/adresse du transform et les writers des cinq champs
sont prouvés, avec leurs constantes ou sources; sinon un seul appelant ou
champ live reste avec son writer exact. Aucun runtime ni changement renderer.

---

# Gate précédent — stores `MovieController+0x14c/+0x15c`

## Hypothèse

Dans `0x82323BB8`, un prédicat unique choisit entre le calcul affine et la
copie de matrice pour `MovieController+0x14c/+0x15c`; le chemin du draw logo
permet de ramener `0xBF` à un champ source précis du transform parent ou de la
frame active.

## Expérience statique unique

Extraire `0x82323CF4..0x82323D60`, qualifier tous les stores vers
`r31+0x14c/+0x15c`, leurs branches dominantes et les définitions de `r3/r30`.
Suivre seulement les deux opérandes qui survivent dans la somme
`C[0x14c]+C[0x15c]`.

## `done_when`

Les deux stores, leur prédicat et la somme amont du draw logo sont prouvés, ou
un unique champ live reste avec son writer exact. Aucun runtime global ni
changement renderer.

---

# Gate précédent — cible type 4 de `0x823237B8`

## Hypothèse

`0x8264CDF8[4]` sélectionne un helper unique qui retourne directement ou
construit le pointeur `T` consommé par `0x82326420`; ses champs `T+0x0c` et
`T+0x1c` ferment la provenance de l'octet `0xBF` du logo.

## Expérience statique unique

Lire l'entrée big-endian `0x8264CDF8+0x10` dans le basefile démo qualifié,
ouvrir uniquement cette cible dans le HIR/Ghidra canonique, puis remonter son
retour et les loads/stores vers `T+0x0c/+0x1c`. Conserver séparément les
coefficients `MovieController+0x14c/+0x15c`.

## `done_when`

La cible, l'objet retourné et les deux floats sont joints au draw logo, ou un
seul champ live exact reste nécessaire avec son instruction d'écriture. Aucun
runtime avant épuisement de cette entrée statique.

---

# Gate précédent — producteur des composantes couleur `r5`

## Hypothèse

Le pointeur `r5` reçu par `0x820EB200` désigne un enregistrement SWG dont les
champs `+0x40/+0x44/+0x48/+0x4c/+0x5c` sont produits par un chemin statique
unique; ce chemin permet de décider si l'octet `0xBF` du logo est naturel ou
provient d'une divergence antérieure à la construction du record.

## `done_when`

Le type ou producteur de `r5`, les stores vers ces cinq offsets et la valeur ou
formule qui atteint le draw logo sont prouvés; sinon les appelants compatibles
et l'unique valeur live manquante sont conservés avant toute instrumentation.

## Limites

Rester sur le projet Ghidra démo PAL canonique et les artefacts qualifiés.
Aucun changement renderer. Aucun runtime tant que les stores et appelants
statiques ne sont pas épuisés.

---

# Gate précédent — slice `0x821DEED8: record+0x18`

## Hypothèse

`0x821DEED8` copie ou dérive `record+0x18` depuis un champ unique du bloc de
paramètres `r30`, lui-même construit à `r1+0x50` par `0x820EB200`; ce champ
porte naturellement les bits `BFFF0000` du draw titre.

## Expérience statique unique

Dans le corps qualifié de `0x821DEED8`, extraire uniquement les stores dont la
base est `r31` et l'offset `0x18`, avec leurs prédicats et définitions SSA.
Remonter la source à `r30+offset` ou à une constante. Puis, dans
`0x820EB200`, retrouver le store qui construit ce champ dans le bloc
`r1+0x50` au callsite `0x820EB3C0..0x820EB3D4`.

## `done_when`

L'instruction qui définit `record+0x18`, son prédicat, le champ source du bloc
local et la valeur amont sont prouvés. Si plusieurs branches restent possibles
pour le draw titre, conserver leurs conditions exactes avant d'envisager un
watchpoint unique sur le nœud alloué. Aucun runtime global ni nouveau rendu.

# Gate précédent — joindre `0x821185A8` au nœud `record`

## Hypothèse

`0x821185A8` initialise le même type de nœud que la liste
`owner+0x20`/`record+0x10`; son store `0x8211860C` est le producteur de
`record+0x18` observé à `BFFF0000`.

## Expérience statique unique

Extraire les appelants de `0x821185A8`, la définition de son `r31`, son retour
et les stores qui publient un nœud dans `owner+0x20` ou `record+0x10`. Si ce
joint tient, remonter `fr12` dans la même fonction jusqu'à sa source et
calculer les bits effectivement stockés à `0x8211860C`. Sinon, conserver la
fonction d'insertion réfutante comme nouveau pivot exact.

## `done_when`

Le résultat/record de `0x821185A8` est joint par flux de données à la liste et
les bits de `fr12` sont prouvés égaux à `BFFF0000`, ou cette piste est réfutée
et le producteur exact de la tête/lien de liste est identifié. Aucun runtime
avant épuisement de ce joint statique.

# Gate précédent — consommateur de la table `0x82009E8C`

Chercher les constructions et chargements de la base ou du voisinage de
`0x82009E8C`, puis qualifier le premier calcul `base + index*0x14` qui charge
`entry+0x0C` avant `mtctr/bctrl`. Établir quels champs bruts (`+0x00`, `+0x04`)
sélectionnent les deux entrées P de `0x82009E8C` et `0x82009EB4`. Depuis ce
callsite seulement, suivre la définition de `r4` puis les stores vers
`r4+0x18`.

`done_when` : callsite indirect et index discriminant prouvés, type/adresse du
record `r4`, instruction qui écrit `+0x18` et valeur amont de `BFFF0000`
identifiés. Aucun runtime tant que la xref de base reste statiquement
exploitable.

Frontière :
`artifacts/goal-playable/title-p1-dispatch-xref-static-20260824/BLOCKER.md`.

# Prochain gate autoritaire — producteur du champ couleur P1

Partir du writer quatre-vertices `0x82118D18`. Il charge `r4+0x18` à
`0x82118D8C`, puis écrit cette valeur à `P1 + {0x30,0x64,0x98,0xCC}`. Trouver
le dispatch ou l'appelant qui fournit `r4`, identifier le type du record SWG et
l'instruction naturelle qui peuple son champ `+0x18`. Réutiliser les HIR et
artefacts existants avant tout watchpoint.

`done_when` : adresse/type du record source, instruction qui écrit son champ
`+0x18`, et valeur en amont de `BFFF0000` identifiés. Si une table de dispatch
indirecte reste l'unique frontière après les lectures statiques, documenter
son slot exact avant de concevoir un watchpoint borné.

# Gate fermé — réconciliation du fetch couleur P/Q

Au même draw titre et au même état de registres que le VS qualifié, extraire
les deux dwords du fetch constant 95 (`0x4800 + 2*95`) et les comparer au slot
94 déjà décodé. Rejouer littéralement le format, l'endianness et le swizzle du
descripteur réellement consommé ; ne pas modifier le renderer et ne pas
relancer de capture avant cette jointure statique.

`done_when` : le slot effectivement consommé par `vf0` est identifié et sa
couleur RGBA résultante est calculée. S'il confirme le rouge, reprendre au
producteur de `vertex+0x30`; sinon, localiser l'erreur de publication P/Q dans
le PM4 ou le bridge.

Fermé au cycle 1814 : slot 95/P1, `BFFF0000` -> `(191,0,0,255)`.

# Gate suivant — producteur de la couleur rouge du quad

Partir du dword couleur à `vertex+0x30` dans les quatre records du premier
quad de Q1 `[0x104A4890,0x104A4960)`. Réutiliser les writers déjà bornés
`0x82118D18` (P) et `0x82119048` (Q), leurs curseurs et la publication
`0x82118A28 -> 0x821B86F8`; ne pas rescanner le ring ni réexaminer le shader.

Chercher d'abord statiquement la source copiée ou packée par le writer Q et le
champ du record SWG qui alimente l'offset 12. Si l'indirection empêche encore
une jointure causale, préparer un unique watchpoint borné aux quatre adresses
`Q1 + {0,52,104,156} + 0x30`, arrêté avant le premier draw.

`done_when` : instruction guest naturelle qui écrit les quatre octets couleur,
valeur source avant packing et record SWG producteur identifiés. Ce verdict
doit décider entre couleur rouge voulue par le guest et corruption native en
amont; aucun nouveau rendu avant cette décision.

# Gate fermé — couleur du premier quad du logo

Partir du premier draw plein écran observé, pas du readback. Qualifier
statiquement son fetch vertex `FMT_8_8_8_8`, tous ses bits d'endianness et son
swizzle, puis comparer la conversion native à la sémantique Xenos/ReXGlue sur
le mot brut `0xFFFF0000`. Suivre ensuite la valeur jusqu'à l'interpolateur du
pixel shader qui module la texture 64x64.

`done_when` : prouver si ce mot doit produire blanc ou rouge dans le shader.
S'il doit produire blanc, corriger une seule conversion partagée et faire un
run codegen-on borné avec readback automatique et inspection humaine. S'il
doit produire rouge, identifier le draw, clear, blend ou load antérieur qui
doit établir le fond blanc avant tout nouveau runtime.

Fermé au cycle 1812 : il produit rouge opaque sous `k16in32` puis `zyxw`.
Ne pas modifier le décodeur ni le clear en blanc par hypothèse.

# Gate précédent — première entrée NtReadFile divergente

Reprendre sans relancer l'ancien témoin : reconstruire
`recompilation/ace-combat-6-demo/build-codegen-on` avec le tracepoint déjà
présent, puis exécuter uniquement ce binaire à 1160 ticks. Le côté codegen-off
du premier essai est invalide et doit être ignoré.

Capturer dans une fenêtre bornée les appels `NtReadFile` des deux builds. Pour
chaque appel conserver handle fichier, position/offset, longueur, événement,
IOSB avant/après et waiter auto-reset présent. L'ancien binaire sera observé à
son symbole de débogage `publish_guest_event`; le courant recevra uniquement
le tracepoint d'import minimal équivalent.

`done_when` : premier tuple d'entrée différent identifié et relié à son
producteur amont. Un seul run par build, même store, entrée et borne; aucune
trace globale et aucun forçage guest.

Blocker détaillé :
`artifacts/goal-playable/ntread-first-divergence-20260824/BLOCKER.md`.

# Gate fermé — appel file-I/O à 0x821A61EC

Exporter depuis le projet Ghidra démo canonique uniquement
`Function_821A6168` (`[0x821A6168,0x821A62EC)`) avec assembleur et pseudocode.
Identifier la cible et les arguments de l'appel à `0x821A61EC`, dont le LR
`0x821A61F0` accompagne les seize publications supplémentaires du binaire
courant.

Joindre ensuite ce service à son dispatch natif et à la création/publication
des handles `0xE0000088…0xE00000C8`. Aucun runtime avant cette jointure.

Fermé au cycle 1808 : slot `+0x10` de la table file-I/O `0x823C2D2C`, joint à
`NtReadFile` et à sa publication immédiate d'événement dans le bridge courant.

# Gate bloqué — première divergence guest avant CP_RB_WPTR

Comparer statiquement les objets et chemins d'exécution de l'exécutable
codegen-on du 22 août et du binaire courant. Partir de la divergence observée
à 1160 ticks : ancien `409 VdSwap / 502 soumissions`, courant `1052 VdSwap /
0 soumission`. Le test et l'appel MMIO existent dans les deux binaires; ne pas
les réexaminer comme hook manquant.

Priorité au changement commun qui altère l'état des threads, les callbacks ou
les lectures MMIO avant la première publication. N'utiliser un A/B hybride
qu'après avoir nommé un objet ou une condition unique et son `done_when`.

Bloqué au cycle 1807 après cinq batches sur l'unique export manquant de
`Function_821A6168`. Voir
`artifacts/goal-playable/first-guest-divergence-20260824/BLOCKER.md`.

# Gate fermé — témoin ancien de la régression CP_RB_WPTR

Chercher d'abord une copie exécutable ou un objet compilé antérieur encore
conservé, puis comparer statiquement le chemin commun ring/MMIO du binaire
courant aux artefacts autour du run positif du 23 août, en priorité
`graphics_mmio_cpu.hpp`, `graphics_ring.hpp`, `guest_bridge.cpp` et leurs
appelants. Retrouver la première condition qui empêche `CP_RB_WPTR` de quitter
zéro avant le tick 1160.

Le contrôle courant à 1160 ticks a fermé l'hypothèse d'un reset tardif : zéro
soumission avant comme après cette borne.

Fermé au cycle 1806 : le binaire conservé du 22 août restaure 502 soumissions
et 407 présentations. Son readback obligatoire reste noir et ne ferme pas la
validation visuelle.

# Gate réfuté — oracle Edge post-callback vers kickoff GPU

Utiliser `scripts/run_xenia_edge_native.sh` avec exclusivement
`demo-game-file/extracted/stfs-root/Default.xex` (`de9178…`). Vérifier le
compte local avant lancement. Capturer une fenêtre bornée autour de
`0x821187A8/0x821B4D80`, le premier kickoff GPU et une screencap Edge.

`done_when` : premier événement présent dans Edge et absent du natif, ou
preuve que les deux publient le même PM4 et que la divergence se situe dans le
runtime Xenos natif. Ne jamais substituer `game-files/default.xex` (`acc302…`,
retail).

Réfuté comme prochain pivot au cycle 1804 : le contrôle neutre du binaire
courant perd lui aussi tout PM4, avant START. Edge reste l'oracle visuel mais
ne discrimine pas cette régression native.

# Gate fermé — transfert Q vers callback renderer

Décompiler statiquement `0x82118B88`, les opérations de queue autour de
`0x822DA568` et le callback `0x822E35E8`. Retrouver le champ ou token qui part
du slot Q naturel `0x8270F598` et devient le record P remis à `0x821187A8`.

`done_when` : formule d'adresse ou champ commun joignant
`0x8270F598 -> 0x821187A8 -> 0x821B4D80`, ou branche exacte qui retire ce slot
avant callback. Ensuite seulement examiner la finalisation
`0x821BA780 -> 0x821B9D58` et `CP_RB_WPTR`.

Statique d'abord, aucun A/B. Un runtime au tick 3002 n'est autorisé que si la
statique fournit l'observable exact. Toute run conserve screencaps et métriques.

# Gate partiel — record 0x70 vers émission PM4/Xenos

Partir statiquement du record créé par `0x821DEED8` pour l'owner
`0x8281EAF0`. Fermer la chaîne candidate
`0x8219DB50 -> 0x82119488 -> ... -> 0x82118FA0 -> 0x821185A8` jusqu'au premier
writer de commande, doorbell/ring ou appel Xenos, ou jusqu'à la condition
exacte qui écarte ce record.

Cycle 1802 : joint jusqu'à Q `0x8270F598` et `0x821186B0`. La jointure
Q→callback/P reste unique et explicitement bornée dans
`artifacts/goal-playable/title-record-consumer-runtime-20260824/BLOCKER.md`.

Statique d'abord. Aucun A/B. Si les pointeurs de liste relocalisés empêchent
la jointure, un seul snapshot read-only au tick 3001 est autorisé. Toute run
de vérification doit conserver une screencap Xvfb et ses métriques visuelles.

# Gate fermé — draw index 0x12 et sortie présentable

Partir statiquement de `0x820EB200` avec le record naturel
`0x2DCB2BBC`, dont `record+0x0C=0x12`. Résoudre l'entrée exacte de la table ABI,
le handle/record de draw et l'écriture dans la queue renderer. Corréler cette
route avec les 24 draws typés et expliquer la frontière `present_count=0`.

Fermé au cycle 1801 : `draw_index=0x12 -> handle 0x0E000071 -> 0x820EA9A0 ->
0x82095DF0 -> 0x821DEED8 -> record 0x70` est exactement joint. La sortie
visuelle courante est vérifiée noire par 21 PNG.

`done_when` visuel : le run de vérification publie une screencap de sa sortie
courante, avec classification automatique et inspection visuelle humaine. Si
la ressource présentable manque encore, identifier exactement son producteur
avant le run ; zéro fichier image n'est pas un résultat acceptable.

Entrée inchangée : START naturel au tick 3000. Aucun A/B, aucune écriture
guest. Statique d'abord.

# Gate précédent — snapshot borné de la liste 13

Fermé au cycle 1800 : `table[13]={0,0x1998}`, 16 records type 0,
`draw_index=0x12`, cible renderer `0x820EB200`. Le run n'a produit aucune
screencap car `present_count=0`.

# Gate précédent — liste 13 vers renderer

Résoudre statiquement l'entrée 13 de la table incorporée dans
`B+0x38=0x2DD7963C`, qualifier la liste et son record, puis suivre
`0x82326608 -> 0x82326420` jusqu'au premier appel renderer/draw persistant.

`done_when` : `list_index=13 -> list -> record -> draw/renderer` exactement
joint, ou preuve négative bornée. Ne revenir au runtime que si les octets
relocalisés ne sont pas reconstructibles ; alors un seul snapshot read-only au
tick 3001 suffit.

# Gate précédent — seconde factory au tick 3001

Étendre au minimum le traceur read-only existant pour corréler la seconde
invocation naturelle de `0x820D18C8/0x82323808`, actuellement observée comme
sous-enfant `0x2E3F1350` du parent `0x2E3F8C50`. Enregistrer ses arguments
`B/D`, les cinq mots de `D`, frame 0, ses éléments et tout `execute_raw` lié.

Fenêtre : tick 3001 uniquement. `done_when` : première commande VM ou callback
natif sortant de la chaîne identifié, ou nouvelle construction hiérarchique
prouvée sans commande sortante. Une route, aucun A/B, aucune trace globale.

# Prochain gate historique

## Frontière statique shader mapparts

La classe `0x30000010` est fermée sur `vsCstCT + psCT`. Pour fermer la passe
HDR et l'unique exception sans runtime :

1. décompiler `Function_822E8488` et ses writers de `material+0x14` ;
2. décompiler `Function_822EDF60` pour les éléments du format `06/13` ;
3. déterminer le traitement du bit `0x80` de `0x30000090`.

`done_when` : clé alternative écrite, déclaration `06/13` décrite et sort de
`0x30000090` prouvé, ou preuve négative bornée. Aucun runtime.

## Gate courant autoritaire — premier effet persistant du tick enfant

La promotion pending→active et le premier tick ne sont plus ouverts :
`0x82323BB8` promeut `A+0xE4` vers `A+0xF4` au début du tick, puis appelle
récursivement le nouvel enfant présent dans `A+0xE4` à la fin de la même
invocation.

Après rotation de session, reprendre statiquement depuis ce premier appel
enfant seulement :

1. qualifier sa frame initiale et les handlers effectivement sélectionnables ;
2. suivre la première commande `MovieMemory` remise à `swg::ASContext`, ou le
   premier callback natif direct ;
3. identifier le premier effet persistant hors du contrôleur qui peut joindre
   `EndMode`, le listener Title ou `manager+0x18`.

`done_when` : un producteur enfant, son opcode/callback et son consumer
persistant sont joints, ou une preuve négative bornée nomme la prochaine
frontière. Pas de runtime sans ambiguïté causale irréductible, entrée,
observables, fenêtre et `done_when` explicites.

Preuves de départ :
`reports/cycle-1795-demo-moviecontroller-promotion-first-tick.md`,
`artifacts/goal-playable/title-child-promotion-static-20260824/RESULT.md` et
`reports/cycle-1794-demo-child-controller-consumer.md`.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

## Gate courant autoritaire — promotion du MovieController pending vers actif

Le corps `0x82323468..0x823235CF` ferme le consumer immédiat de l'enfant : le
résultat construit ou réutilisé est enregistré puis inséré dans la liste
`A+0xE4/A+0xE8`. Il n'est pas publié dans `A+0xF4`; la seule écriture à ce
champ retire un ancien candidat actif.

Reprendre uniquement en statique dans le projet Ghidra canonique
`ace-combat-6-demo` :

1. inventorier les writers de `A+0xE4`, `A+0xE8` et `A+0xF4` autour du tick
   `CSwgManager` ;
2. qualifier le prédicat exact qui choisit un enfant pending et le publie dans
   `A+0xF4` ;
3. joindre cet enfant au premier appel `0x82323BB8(A+0xF4)`, boucle de mise à
   jour de la timeline UI scriptée et de consommation de `MovieMemory`.

`done_when` : writer, prédicat de promotion et premier tick
`0x82323BB8` qualifiés, ou preuve négative bornée nommant le propriétaire
alternatif. Ne pas lancer de runtime avant cette fermeture et ne pas assimiler
`MovieController` au lecteur de `moviepack.bin` ou à `CModeTaskTitleMovie`.

Preuves de départ : `reports/cycle-1794-demo-child-controller-consumer.md`,
`artifacts/goal-playable/title-child-consumer-static-20260824/RESULT.md` et
`analysis/demo/ac6-demo-structural-types-v1.json`.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

## Gate courant autoritaire — qualifier le consumer du MovieController enfant

Le pulse START naturel est désormais fermé jusqu'au burst VM/SWG : au tick
3000, current/pressed atteignent `0x10`; au tick 3001, deux receivers passent
par `0x820D32D0 -> vtable 0x82006A9C slot +0x70 -> 0x820D3AC8`, puis les cinq
callbacks GetCurrentLevel, GetCurrentMission, GetCurrentMode, SendMsgI(M102)
et OnVoice2D s'exécutent. L'input n'est plus une frontière.

La fenêtre `[2990,3041)` réfute une transition directe par ce burst : aucun
`0x820EA4A8`, writer `manager+0x18`, remplacement de tâche ou changement des
attributs Title observés. La qualification statique ferme désormais le caller
naturel `0x82322300`, son agrégat `A`, le storage incorporé `B=A+0x08` et la
sélection de `D` depuis les tables de B jusqu'à la factory virtuelle
`0x820D18C8`.

Une trace unique, bornée à START tick 3000/release 3001 et max tick 3407,
observe ensuite un événement naturel distinct au tick 3001 : depuis le
callsite enfant `0x8232356C`, `0x820D18C8` appelle l'initialiseur
`0x82323808` et retourne un `MovieController` de vtable `0x820304D8`. Les
valeurs observées `A`, `B=A+8`, `D`, `s=-1` et `MovieMemory` concordent avec
l'ABI statique. L'observateur conclut toutefois
`factory_result_without_root_publish` : aucune écriture racine `A+0xF4`,
transition de mode ou frame frontend n'est prouvée.

Le prochain gate redevient donc strictement statique :

1. qualifier le corps contenant le callsite de retour `0x8232356C` sans le
   reclasser en fonction autonome ;
2. identifier le prédicat exact de cet appel enfant et la source de son
   argument `D` ;
3. suivre le résultat retourné jusqu'à son consumer ou attribut de
   publication, en distinguant objet enfant, parent et racine `A+0xF4` ;
4. rechercher seulement depuis cette chaîne qualifiée un réarmement D5,
   `EndMode`/`menu_endMode`, le listener `0x8217C890` ou une requête
   `manager+0x18`.

Ne pas réouvrir le faux payload littéral `{6,0xE04}`, déduire une transition
du seul thunk tick3036, rattacher les appels `0x82323808` ultérieurs à la
factory déjà close, écrire un attribut guest, appeler une factory, forcer
EndMode, modifier le scheduler ou synthétiser des pixels. Une nouvelle trace
n'est autorisée qu'après réduction à un nouveau prédicat causal nommé, avec
une fenêtre et un `done_when` plus précis.

Qualifications de départ :
`artifacts/goal-playable/title-start-edge-runtime-final-20260823/RESULT.md`,
`artifacts/goal-playable/post-titleus-distinct-producer-static-20260823/RESULT.md`,
`artifacts/goal-playable/title-loader-descriptor-join-static-20260823/RESULT.md`,
`artifacts/goal-playable/swg-acc-materializer-static-20260823/RESULT.md`,
`artifacts/goal-playable/swg-acc-caller-join-static-20260823/RESULT.md`,
`artifacts/goal-playable/title-post-start-moviecontroller-join-runtime-20260823/RESULT.md`,
`artifacts/goal-playable/tick3036-thunk-static-20260823/RESULT.md`,
`analysis/demo/ac6-demo-structural-types-v1.json` et
`reports/cycle-1793-demo-post-start-child-moviecontroller.md`.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

## Historique cycle 1788 — producteurs supposés de la transition réelle

Dans la fenêtre START, le chemin d'initialisation republie la plage active du
`swg::MovieController` avec une seule entrée type 4. Le canal type 6 qui
pourrait enfiler une commande
`EndMode {6,0x00000E04}` disparaît ; `M102=0` et le lookup local `0x0B` ne
sont pas des prédicats de sélection et cette ancienne frontière est réfutée.

Le chemin encore vivant est désormais classé, et ne constitue pas la
transition :

```text
frame type 4
  -> 0x82326608
  -> 0x82326420
  -> sous-élément type 0 / 0x82325E70
  -> CSwgRenderer, slot 7 / 0x820EB200
  -> record CPU layout 0x70 -> sérialisation/file
```

`0x820EB200` est une méthode virtuelle d'instance `CSwgRenderer` ;
`0x82325E70` est un callback de table globale. Aucun effet EndMode ou mode
manager n'existe dans ces CFG. Reprendre statiquement, en trois slices
séparés :

1. joindre les payloads type6 bruts au buffer heap par `0x82278F78` et ses
   relocations ;
2. remonter le producteur des arguments storage `B` / descriptor `D` du
   chemin republié à START, sans supposer nouvelle instance ou adresse
   réutilisée ;
3. identifier l'état persistant de `CModeTaskTitleDemoOffline` qui peut
   réellement choisir Loading/GameDemoOffline ou armer `manager+0x18`.

Épuiser Ghidra canonique, XenonAnalyse et les artefacts existants avant toute
observation. Si une arête causale précise reste dynamique, fixer seulement
son entrée, ses champs/objets, une fenêtre minimale post-START et un
`done_when`; aucun A/B ni trace globale.

La chaîne suivante reste qualifiée uniquement **si** `menu_endMode` est
invoquée ; elle n'est pas une jonction naturelle depuis START :

```text
menu_endMode -> 0x8217C890 -> 0x8218AB98
  -> Title état 2 / compteur 3 -> 0x8218A7A8 / 0x8218AA30
  -> manager+0x18=1 -> 0x821929A8 -> 0x82190B18
```

`CModeTaskMissionTitle` (`0x8200E5C4/0x82185198`) est distinct du vrai
`CModeTaskTitleDemoOffline` (`0x820113E4/0x8218A7A8`). Toute la famille
`CModeTask{StartUp,Title,Loading,Game}DemoOffline` est démo-only : ne jamais
en inférer un layout, une méthode, un attribut ou une adresse depuis les
prototypes retail. Le listener global `0x826DF804` et la boucle
`E000004C/0x821A8C88` sont des pistes réfutées pour cette transition.

Le gate frontend exige toujours la paire indissociable : état guest persistant
post-transition et frame visible post-transition. Ne pas écrire
`manager+0x18`, appeler une factory, insérer une tâche, forcer `EndMode`,
modifier le scheduler ou promouvoir `frontend` sur la seule preuve statique.
Le prompt « PRESS START » arrive bien après le logo Namco désormais visible.

Qualifications de départ :
`artifacts/goal-playable/title-start-route-static-20260823/RESULT.md`,
`artifacts/goal-playable/start-title-object-rtti-static-20260823/RESULT.md`,
`artifacts/goal-playable/title-swg-post-start-static-correction-20260823/RESULT.md`,
`artifacts/goal-playable/title-frame-descriptor-static-20260823/RESULT.md` et
`artifacts/goal-playable/title-parallel-matrix-consumer-static-20260823/RESULT.md`.

Les priorités ci-dessous sont historiques et supersédées lorsqu'elles parlent
encore du handle `0x59`, du Q1 ou du premier logo comme frontière active.

## Priorité immédiate — producteur de la liste SWG imbriquée

Remonter statiquement le script/callback `brandLogo` qui maintient l'owner
imbriqué sur `frame_index=0`, `list_index=0`, puis identifier l'arête naturelle
vers la liste portant `draw_index=2/handle59`. Le parent avançant normalement,
ne pas modifier le scheduler. Ne pas relancer la même fenêtre, remapper une
ressource, forcer un draw ou modifier le renderer avant un contrat précis.

## Priorité immédiate — sélecteur naturel `draw2 -> 0x59`

Épuiser statiquement le producteur de l'index de draw qui passe de `0x57` à
`0x58`, puis doit publier `0x59`. Identifier le compteur/timer, son writer et
la condition exacte jusqu'au record worker/PM4. N'autoriser une trace bornée
qu'en présence d'une ambiguïté causale nommée. Ne pas remapper `0x57`,
substituer `003_NTXR`, forcer un draw ou peindre des pixels côté hôte.

## Priorité immédiate — flush après `0x821B4D80`

Le BrandLogo atteint naturellement le worker et `0x821B4D80`; remonter
statiquement ses écritures de buffer, cursor et dirty-state jusqu'au premier
flush partagé, puis qualifier son raccord au writer du ring/PM4. Aucun nouveau
runtime avant épuisement de cette chaîne. Ne pas forcer `CP_RB_WPTR`, appeler
le renderer, injecter une commande ou peindre le logo. Voir
`artifacts/goal-playable/brandlogo-consumer-runtime-20260822/RESULT.md`.

## Priorité immédiate — consommateur de la liste BrandLogo

Les portes ACC/geometry sont fermées : le guest publie naturellement les
records `0x70` via `0x82095DF0 -> 0x821DEED8` sous `0x8281EAF0`. Identifier
statiquement le consommateur/flush de cet owner, puis son premier dispatch ou
garde avant la chaîne Xenos qualifiée. Ne pas remapper les handles, changer le
type de la queue motion, appeler le renderer ou peindre le logo. Voir
`artifacts/goal-playable/acc-resource-runtime-20260822/RESULT.md`.

## Priorité immédiate — état SWG/script et garde globale

Le callback `CSwgCallback` est maintenant atteint naturellement, mais
`0x820CDCD8` lit zéro dans les bitsets d'état logique/action pointés par
`0x823C27E0` aux offsets `0x23980..0x23990`; `this+9` ne s'arme donc jamais.
Les écrivains `0x821DE990` et `0x821DE6E0` sont qualifiés. Remonter
statiquement leur événement SWG/script et le consommateur de la garde globale
`0x826DFC48`, sans traiter ces mots comme une table de ressources.
Une trace supplémentaire n'est autorisée que si un site producteur précis
reste causalement ambigu. Ne pas écrire les bits, forcer le callback ou
injecter un record. Voir `artifacts/goal-playable/swg-callback-arm-20260822/`.

Le gate `task-loading-publication` est fermé négativement :
`0x8218CE20`/`0x8218CCD0` terminent ou pollent seulement `CTaskLoading`,
`0x82259D10` ne traite que les tâches déjà en liste, `0x82259E18` ne fait
qu'insérer un objet construit, et `0x821929A8` ne publie aucun mode. Reprendre
statiquement la factory amont qui doit atteindre `0x8217C678`, puis la mutation
de liste; ne pas forcer le thunk START ni ajouter de shim. Preuve :
`artifacts/goal-playable/task-loading-publication/RESULT.md`.

Le dernier slice d'entrée de jeu ne qualifie aucun pont sûr :
`CModeTaskGameDemoOffline` n'est jamais publié, et `0x8217C4D8(-3)` reste en
aval. Le thunk START `0x820D32D0` est un dispatch virtuel data-dependent;
ne pas le remplacer ni forcer la tâche. Reprendre statiquement la terminaison
de `CTaskLoading` (`0x8218CE20`, `0x82259E18/0x82259FF8`) et sa publication
vers le champ de requête du mode manager (`0x821929A8`). Aucun changement
renderer ou runtime avant cette preuve. Voir
`artifacts/goal-playable/game-entry-static/RESULT.md`.

Le slice final `0x8218BFB0`/`0x8219BFB0` est négatif : ne pas transformer les
clés de provider ou le `texture_id 2` en mapping ACC. Le prochain point utile
reste le constructeur/provider qui remplit le descripteur transmis à
`0x8219EE40` puis `0x8219E428`; sans cette arête, garder le renderer
fail-closed et le runtime sans logo.

Le follow-up owner/r8 ferme aussi la provenance AVI : le slot
`0x8200B700 → 0x82165CC0` est qualifié, mais `base+0xC` n'est pas publié et
`r8` n'a pas de producteur qualifié. Ne pas compléter le renderer avant cette
arête. Preuve : `artifacts/goal-playable/owner-r8-followup/RESULT.md`.

## Priorité immédiate — payload du logo, statique d'abord

1. Relier le payload maintenant identifié (`DATA.TBL[170/171]`, SWG
   `brandLogo`, `texture_id 2 → 003_NTXR.ntxr`, huit NTXR) au producteur ACC réellement consommé par
   `0x8219E580`. La piste des clés `0xCB..0xCF` est fermée négativement : elles
   deviennent `DPL::[cb..cf,0]`, rejoignent seulement `0x8219E768` et ne
   mènent pas à `0x8219E428` ni au chargeur de ressources.
2. Qualifier ensuite le NTXR exact ou le buffer SWG qui remplit
   `source+0x20`, puis mapper les handles `0x0E000057..0x0E00005E`.
3. Seulement après un record guest et un draw Xenos non-bootstrap réels,
   vérifier RT0/resolve/writeback et capturer le logo. Aucun bitmap host,
   appel forcé, record synthétique ou nouvelle interface CLI.

Voir `artifacts/goal-playable/namco-logo-route/RESULT.md`,
`artifacts/goal-playable/swg-payload-static/RESULT.md` et
`artifacts/goal-playable/brandlogo-swg-refs/RESULT.md`.

La piste `0x820E8F90 → 0x820EA4A8 → 0x821728C0` est maintenant fermée comme
simple callback SWG/tâche : elle ne construit pas l'ACC. Ne pas ajouter de
shim à ce point. Le prochain slice doit identifier le constructeur/registre
qui alimente `CResourceLoaderSwg` (`0x8219E580`) ou
`CSelectAircraftSetupManager` (`0x82127D40`) et écrit le descripteur
`+0x18/+0x1c/+0x20` correspondant au `brandLogo`; à défaut, borner cette
absence statiquement. Preuve :
`artifacts/goal-playable/acc-guest-join-next/RESULT-followup.md`.

## Priorité immédiate après le gate owner/r8

1. Épuiser statiquement les écritures/constructeurs/xrefs de
   `0x82386C58`, qualifier son vtable et le slot `+0x08`, puis classer la
   cible (PM4, non graphique ou import manquant).
2. Qualifier statiquement la population des entrées de `DAT_826F6188` par
   `0x82165040`/la voie jumelle et relier (ou réfuter) les noms du provider aux
   cinq clés du payload; `0x82165AF8` et `0x821080D0` ont désormais des rôles
   distincts et ne doivent pas être fusionnés.
3. Reprendre la valeur-flow du receiver `0x8200B6FC+0xC` vers
   `0x82165CC0:r8`; toute trace doit rester bornée à cette ambiguïté nommée.
   Les writers titre `0x820B3E80`/`0x82321E18` sont désormais connus, mais
   leur join vers l'owner `0x820D29E0` reste à qualifier.
   La passe AVI classe `0x82374AD0` comme initialiseur de la base
   `0x82731A30`; aucune publication `base+0xC -> 0x82165CC0` n'est qualifiée.
4. Seulement après un record guest type 1–4 réel, compléter les sémantiques
   Xenos effectivement atteintes et vérifier RT0/readback. Ne pas ajouter de
   shim, d'appel forcé, de record synthétique ni d'optimisation.
5. Quand le premier draw guest est qualifié, rejouer le jalon titre puis menu,
   ensuite seulement la transition mission et les lanes `demo-playable-gate-v1`.

## Reprise prioritaire — provenance receiver NFH (statique)

`done_when` : un receiver construit par `0x822CC118`/`0x82237EB0` est relié
à un appel `+0xB0` de la table `0x82027A64`, ou l'intersection est démontrée
vide avec les bornes exactes du traceur.

Corriger l'invocation de `TraceVirtualRecordConsumers.java` en fournissant
`START END`; limiter la fenêtre aux fonctions candidates déjà extraites.
Ne pas déduire le layout `0x20` depuis le déplacement seul et ne lancer aucun
runtime, Wine, Xenia ou shim env avant cette fermeture.

Voir `artifacts/nfh-receiver-provenance-gate/BLOCKER.md`.

## Après réfutation — autre arête NFH ou producteur visible

Le gate des 24 dispatchs directs `+0xB0` est fermé par réfutation. Ne pas
attribuer de champ au record `0x20`. Le prochain slice statique doit soit
suivre un retour conservé au-delà de la fenêtre bornée, soit qualifier un
autre slot/appel indirect dans la chaîne NFH; si cette branche ne fournit pas
de producteur visuel, reprendre ensuite le producteur IB bootstrap indiqué
ci-dessous. Aucun runtime ni shim env n'est justifié par ce gate.

## Gate suivant — watch native du producteur bootstrap

`done_when` : une exécution native limitée au tick zéro attribue les stores de
`0x16AE0980..0x16AE0A40` à une fonction guest, ou démontre que l'IB est soumis
sans store dans cette fenêtre.

Utiliser `AC6_DEMO_WATCH_ADDR_LO` et `AC6_DEMO_WATCH_ADDR_HI`; ne relever que
adresse, taille, valeur, fonction appelante et ordre avant soumission. Les
critères confirmer/réfuter/indécidable sont fixés dans le blocker statique.

## Gate suivant — producteur de l'IB bootstrap

`done_when` : le producteur qui publie la chaîne `0x1685A000 → 0x16AE0980`
est relié à son état persistant de rendu, ou les données statiques bornent
l'absence de cette provenance.

Reprendre dans les producteurs de l'IB et des états PM4, sans interpréter le
microcode et sans runtime. Une capture ciblée ne sera définie qu'après ce
slice si l'état RT0 demeure indécidable.

## Gate suivant — premier writer RT0 qualifié non noir

`done_when` : le premier draw RT0 qualifié dont les entrées peuvent produire
une couleur non nulle est identifié statiquement, avec son contrat minimal de
sortie, ou les producteurs disponibles sont bornés à une sortie nulle.

Partir des commandes PM4 et des états de draw déjà atteints. Réutiliser les
qualifications et le harness RT0; ne modifier ni shader ni backend et ne faire
un runtime ciblé qu'après ce slice.

## Gate suivant — contrat de lecture RT0 après draw qualifié

`done_when` : un test synthétique établit qu'un draw RT0 qualifié publié au
consommateur fournit le buffer attendu, ou borne précisément l'interface qui
manque entre ce draw et la lecture.

Réutiliser les commandes déjà retenues et le test de copie EDRAM existant.
Ne modifier ni shader, ni résolution, ni backend Vulkan; ne lancer aucun
runtime complet.

## Gate suivant — site d'appel du slot `+0x0C` de la vtable `0x820064D8`

`done_when` : un appel indirect qui sélectionne le slot `+0x0C` est relié à
son objet et à son second argument, ou les représentations statiques de cette
vtable bornent l'absence de site résoluble.

Partir de la vtable `0x820064D8`, de son slot `0x820D29E0` et des tables qui
installent cette vtable. Remonter les deux arguments au site d'appel; aucun
runtime avant l'épuisement de ce slice statique.

## Gate suivant — dispatcher titre vers producteur de sélection

`done_when` : les xrefs et CFG du dispatcher qui atteint
`CModeTaskTitleDemoOffline` relient un appel à un producteur du sélecteur
global, ou bornent statiquement l'absence d'une telle arête.

La callback `0x8218AB98` écrit dans son propre objet `this + 0x70`, tandis que
le setter mission accède à `PTR_DAT_823c27e0 + 0x70`. Partir de la chaîne de
vtable du titre et de ses appelants dans le projet démo canonique. Ne lancer
aucun runtime avant cet épuisement; si une capture devient nécessaire,
observer seulement l'objet, la cible virtuelle et l'éventuel appel du
producteur autour de START.

## Gate suivant — valeurs SWG qui suivent START

`done_when` : les retours de `GetCurrentMode`, `GetCurrentMission` et
`GetCurrentLevel`, le contexte de film et son prochain appel sont relevés dans
une fenêtre START bornée, ou leur indisponibilité est démontrée.

Après les slices statiques clos, exécuter uniquement autour de tick 3001 et de
la fenêtre neutre `menu_endMode`. Capturer ces trois retours, l'identité du
contexte/script et le prochain appel. Confirme une branche de film : valeurs
ou chemin divergent avant la suppression de callback. Réfute : mêmes valeurs
et contexte, callback tout de même supprimé. Indécidable : observables absents.

## Gate suivant — sélecteur SWG post-START du titre

`done_when` : le branchement qui sélectionne le callback neutre
`0x8218AB98` ou les handlers START est attribué au contexte SWG, ou une
capture bornée est définie après épuisement du slice statique.

Partir des références et du CFG du contexte global atteint par `0x820EA238`.
Ne capturer, si nécessaire, que l'identité du contexte/script, le branchement
et la cible callback autour de START et du tick d'attract neutre. Confirme :
ces observables divergent. Réfute : elles sont identiques mais la cible est
neutralisée. Indécidable : le contexte ne livre pas ces valeurs dans la fenêtre.

## Gate suivant — premier contenu RT0 non nul après progression guest

`done_when` : un draw postérieur au chemin guest qualifié produit un RT0 non
nul, ou les producteurs PM4 alors atteints sont bornés à une sortie nulle.

Partir de la publication PM4 et des draws nouveaux, sans toucher au renderer.
N'utiliser un runtime ciblé qu'après le slice statique : capturer commande,
état RT0 et résultat de resolve autour d'un seul draw. Confirme : un échantillon
RT0 non nul suivi de sa copie. Réfute : draw entièrement nul. Indécidable : le
draw n'est pas atteint dans la fenêtre bornée.

## Gate suivant — progression guest vers un draw à contenu non nul

`done_when` : une progression guest post-titre est reliée à un draw RT0 dont
la sortie est non nulle, ou les consommateurs du burst de script bornent
statiquement l'absence de cette progression.

Commencer par les consommateurs du compteur et des résultats de VM connus,
avant tout runtime. Si une capture ciblée devient nécessaire, elle doit
observer seulement la valeur du compteur avant/après l'évaluation et le
changement de tâche associé. Confirme : changement persistant suivi d'un
nouveau draw non nul. Réfute : compteur consommé sans changement de tâche.
Indécidable : consommateur non atteignable dans la fenêtre bornée.

## Gate suivant — récepteur virtuel du START pendant le titre

`done_when` : la cible du slot virtuel `+0x70` atteint par `0x820D32D0` est
qualifiée, ou une capture bornée établit que sa mémoire ne permet pas de la
déterminer.

La preuve statique établit le thunk; les sources et xrefs ne révèlent pas
l'objet dynamique. Si cette lacune persiste après le slice statique, capturer
au seul tick de START `r3`, sa vtable, la case `+0x70` et le retour. Confirme :
la case mène au récepteur observé. Réfute : objet ou cible incompatible.
Indécidable : objet/vtable illisible. Ne pas enregistrer de trace globale ni
jouer au-delà de cette fenêtre.

## Gate suivant — progression guest vers un draw à contenu non nul

Le premier draw RT0 atteint est une commande PM4 et non un writer CPU EDRAM;
son contenu reste nul. Reprendre la frontière d'exécution qui empêche le guest
d'atteindre un draw ultérieur, sans synthétiser de pixels ni confondre WPTR et
échantillons EDRAM.

## Gate suivant — premier écrivain non nul de RT0/EDRAM

Le fetch et le varying du rectangle normal sont nuls et ne causent pas sa
noirceur. Qualifier statiquement le premier écrivain non nul de RT0/EDRAM ou
son absence, avant toute résolution ou exécution runtime.

## Gate suivant — interprétation statique du fetch vertex normal

La plage guest du fetch normal est déjà couverte par le descripteur
shared-memory lié au backend. Qualifier maintenant, par sources et analyse
statique, son format et ses lanes vus par le shader traduit. Ne pas synthétiser
de sommets, pixels ou readback et ne pas déclencher de runtime.

## Gate fermé — disposition d'installation native

Le préfixe d'installation temporaire contient le binaire et sa configuration
aux emplacements attendus, sans `bin/bin`. Ce gate ne porte pas sur le
comportement guest.

## Gate fermé — intégration locale du bridge natif

Le build actuel passe ses 27 CTest avec l'audio dummy. Il s'agit d'une
validation locale seulement ; ne pas en déduire une progression frontend,
mission ou renderer visible.

## Gate en pause — producteur de `VdGlobalDevice+0x4084`

Le constructeur titre `0x821C64E8` publie l'objet device et
`0x821C6400` enregistre le callback graphique, mais l'écrivain du champ
interne `+0x4084` reste une opération de l'ABI Vd externe. Après cinq slices
statiques, ne pas poursuivre ni lancer de runtime. Reprendre seulement avec
une documentation Vd autoritative ou une procédure statique qui couvre cette
ABI externe.

## Gate fermé — interface du callback indirect graphique

Le `bctrl` de `0x821C5190` est une interface de callback de
`VdGlobalDevice+0x4084`, sérialisée par le spinlock `+0x4130`. Ne pas la
promouvoir en PM4 ou en consommation pixel : sa cible reste à qualifier.

## Gate fermé — contrat CPU du callback XAudio

Le contrat C++ local compile et son CTest ciblé passe. Il ne règle pas la
chaîne `submit → événement → worker → frame suivante` ; ne pas l'étendre dans
cette branche sans une nouvelle question de preuve.

## Gate en pause — premier écrivain statique non nul du record render

Question : quel CFG écrit un contenu non nul dans le record de 96 octets que
le worker `0x820FFCA0` copie depuis la file, et ce contenu atteint-il une
tâche ou une soumission renderer ?

`done_when` : un écrivain est relié à un champ précis du record et à son
consommateur, ou les producteurs connus sont bornés à l'initialisation nulle.

Confirme : un store non nul atteint un champ que le worker transmet à un
callee consommateur. Réfute : tous les producteurs qualifiés initialisent ou
remettent le record à zéro. Indécidable : les bases composées du record ne
peuvent pas être suivies statiquement.

`0x820FF710` initialise le champ de contrôle du slot, mais la recherche de ses
appelants directs ne trouve aucun `bl`. Après cinq observations statiques, ne
pas poursuivre ce gate ni lancer de runtime. Il ne peut reprendre qu'avec une
procédure statique existante qui borne un dispatch indirect ou un saut entrant
vers le producteur.

## Gate en pause — consommateur de l'état de fournisseur `+0x20`

Question : quel CFG démo lit l'état écrit à `fournisseur+0x20` par `0x822FFCC8` ou `0x822FFDB8`, et cette lecture rejoint-elle une liaison shader ou une soumission GPU ?

Le scan D-form de `+0x20` a produit 1 066 candidats à base `r3`/`r4`, sans
identité d'objet suffisante pour les rattacher aux fournisseurs. La piste
`0x822E9DEC → 0x822F5608` est exclue : le callee est un test nul suivi d'une
branche en queue vers `0x822F54E0`, avec un flux séparé.

Après cinq observations statiques, ne pas poursuivre ce gate ni lancer de
runtime. À la reprise, employer d'abord une procédure statique existante qui
préserve la provenance du pointeur fournisseur ; `done_when` reste un lecteur
qualifié par son flux et ses effets, ou une interface de lecture précisément
bornée.

Confirme : le consommateur charge, sélectionne ou lie un shader, ou atteint un
état GPU soumis. Réfute : il ne fait que préparer, comparer ou recopier la
donnée. Indécidable : la provenance du fournisseur ou l'interface de lecture
reste statiquement non résolue.

## Gate suivant — effet de fournisseur `0x822E0B90`

Question : quel effet exact `0x822E0B90` produit-il pour chacun des fournisseurs extraits du profil global, et ce chemin atteint-il une sélection de shader ou un batch GPU ?

`done_when` : son CFG, ses appels enfants et ses sorties locales sont qualifiés, ou une interface indirecte précise borne son effet.

Confirme : le callee sélectionne/lie un shader ou construit un état GPU. Réfute : il ne fait que transformer ou résoudre une ressource sans soumission. Indécidable : la dispatch reste non attribuable après CFG et vtable statiques.

Partir uniquement de `0x822E0B90` et de son contrat `(fournisseur, buffer local)`. Aucun runtime.

## Gate suivant — lecteurs du profil global non-bootstrap `0x829D0800`

Question : quels CFG démo lisent les champs `+8`, `+0xc` ou `+0x10` du profil global `0x829D0800`, et quels sont leurs effets directs ?

`done_when` : le premier lecteur pertinent et ses effets sont qualifiés, ou les références statiques à ces champs sont bornées à une interface exacte.

Confirme : une lecture mène à la sélection de fournisseur, shader ou commande GPU. Réfute : les lecteurs ne font qu’initialiser, tester ou transmettre le profil sans effet shader/PM4. Indécidable : une adresse calculée échappe au slice statique disponible.

Commencer par les matérialisations statiques de `0x829D0800`, puis réduire aux loads de `+8`, `+0xc` et `+0x10`. Aucun runtime.

## Gate suivant — provenance statique de l’enregistrement de profil non-bootstrap

Question : quel mécanisme, distinct du stub partagé `0x8232710C`, attribue une identité ou un stockage statique à l’enregistrement que `0x822E9A18` remplit ?

`done_when` : un producteur ou une adresse de stockage exacte est qualifié, ou l’opacité de cette provenance est bornée par un CFG/ABI concret.

Confirme : un chemin conserve l’adresse retournée et la relie à des champs ou à un consommateur qualifié. Réfute : le chemin montre une indirection sans stockage identifiable. Indécidable : l’identité n’existe qu’à l’exécution sans observable statique supplémentaire.

Commencer par le contrat ABI et les producteurs immédiats de l’adresse, sans rechercher globalement des consommateurs ni lancer de runtime.

## Gate suivant — consommateur du profil de fournisseurs non-bootstrap

Question : quel CFG démo consomme les trois pointeurs de fournisseur écrits par `0x822E9A18` dans le profil et les transforme, s’il y a lieu, en shader ou en commande GPU ?

`done_when` : le premier consommateur du profil et ses effets directs sont qualifiés, ou la provenance du profil est bornée à une interface exacte. Aucun runtime n’est requis.

Observations prévues : un lecteur des trois champs confirmerait le consommateur; l’absence de lecture vers un chargement shader/PM4 réfuterait son rôle de producteur; un pointeur global encore non attribuable laisserait la frontière statique indécidable.

Partir uniquement de l’enregistrement produit par `0x822E9A18` et de ses champs de fournisseur. Ne pas réexaminer la vtable ni les listes de fournisseurs déjà closes.

## Frontière suivante

Question : Quel callee indirect de drain (`0x821A66E0`, puis ses descendants) effectue la publication du batch, s'il y en a une ?

done_when : Un CFG DEMO qualifié montre une publication du ring/MMIO, ou borne la publication derrière une interface encore indirecte. Aucun runtime.

Actions : partir de `0x821A66E0` seulement ; conserver `device+0x5494+0x258` comme localisation exacte du bit `0x08`.
## Frontière suivante — identité du handle de drain

Question : Quel CFG DEMO produit le handle transmis à `0x821A66E0` depuis `0x821BF7D8`, et quel objet noyau ouvre-t-il ?

done_when : Le producteur et l'ouverture du handle sont qualifiés statiquement, ou une interface indirecte est explicitement bornée. Aucun runtime.

Actions : suivre uniquement le champ de handle du sous-objet `device+0x5494`; ne pas inférer une soumission Xenos du seul appel `NtWriteFile`.
## Frontière suivante — route d'alias du handle

Question : Quelle propagation de pointeur ou table de champs atteint le `+0x14` du canal sans store D-form direct ?

done_when : Un slice statique réutilisant les scripts existants rattache le champ à un producteur, ou démontre que le projet Ghidra ne peut pas porter cette information. Aucun runtime.

Actions : partir du sous-objet créé dans `0x821BEFF0`; ne pas réutiliser l'hypothèse d'un handle graphique sans provenance.
## Frontière suivante — publication par l'interface I/O

Question : Parmi les sites `NtWriteFile` qualifiés et leurs descripteurs, lequel relie statiquement un buffer PM4 à l'interface graphique ?

done_when : Une chaîne statique buffer PM4 → descripteur → `NtWriteFile` est qualifiée, ou les sites disponibles sont tous bornés hors de ce rôle. Aucun runtime.

Actions : repartir des appelants de `0x821A66E0` et de leurs buffers; ne pas recommencer la recherche d'alias `+0x14` sans nouvel outillage de slice.
## Frontière suivante — contrat natif de soumission

Question : Le backend natif reproduit-il le contrat statique de `0x821BFBA8` : buffer PM4 assemblé, longueur arrondie, publication atomique au point partagé ?

done_when : Les sources natives du chemin de soumission sont rapprochées de ce CFG et un écart concret, s'il existe, est isolé. Aucun changement de code sans écart qualifié.

Actions : lire le point de soumission commun de la reconstruction et ses tests; ne pas étendre vers un run complet.
## Frontière suivante — enveloppe de soumission pilote

Question : Quelle structure de buffer `0x821BFBA8` remet-elle à `NtWriteFile`, et contient-elle une publication WPTR ou un PM4 indirect ?

done_when : Le layout statique du buffer et ses pointeurs/longueurs sont qualifiés, ou le CFG borne un format opaque au pilote. Aucun runtime.

Actions : suivre la construction locale immédiatement avant l'appel `0x821A66E0`; ne pas modifier `graphics_ring.hpp` sans correspondance de format démontrée.
## Gate suivant — premier contenu RT0 non nul

Le producteur du premier draw atteint est Xenos et l'ABI XMA publique ne qualifie que son pointeur de sortie. Reprendre la progression guest à partir du premier appel XMA suivant ou de son consommateur PAL, en distinguant le statut/slot établis de tout état interne non qualifié; ne pas synthétiser de contexte, paquet ou pixel.
## Gate suivant — consommateur du quatrième store XMA

Le release-loop ne produit aucun contenu graphique qualifié. Qualifier statiquement le store XMA suivant et son premier consommateur PAL; une observation runtime, seulement si le CFG ne suffit pas, doit capturer ce store, son consommateur et l'état minimal qui déciderait la progression, sans trace globale.
## Gate suivant — premier contenu RT0 non nul

Le quatrième kick XMA est une soumission périphérique sans effet graphique qualifié. Reprendre les submissions PM4 après cette frontière et qualifier le premier draw dont les entrées ou les sorties diffèrent du draw noir déjà joint; ne pas déduire ce contenu depuis XMA.
## Gate suivant — divergence guest après l'entrée START

Le draw noir et le scheduler sont exclus comme causes immédiates : le signal `E000004C` est délivré. Qualifier par CFG et traces déjà bornées le premier état guest qui diffère réellement entre neutral et START avant d'étendre l'exécution; ne pas injecter de wake ou de pixels.
## Gate suivant — état guest qui atteint le menu après START

START arrive jusqu'au mapper logique et le scheduler livre ses événements. Qualifier par CFG le premier producteur de l'état lu par les consommateurs menu `0x82170FCC` et `0x82185210`, puis seulement si le flux statique reste ambigu, capturer cette cellule et ce callsite dans une fenêtre bornée.
# Gate suivant — frontière mission après consommateurs

`done_when` : une sonde bornée avec entrée qualifiée atteint `0x820EA550` ou
`0x820EA598`, ou la chaîne statique suivante (`0x820E9300`/`0x820E9290` et
leurs appelants script) est décompilée dans le projet démo.

Le probe à 120 ticks n'est pas discriminant pour la mission : il confirme
seulement `session_state=guest`, sans milestone. Ne pas attribuer cette
absence à une erreur des wrappers.

# Gate suivant — vocabulaire compact du loader

`done_when` : le CFG de `0x82278F78` relie les mots hors `0..7` à une règle de
décompactage statique, ou établit qu'ils sont volontairement ignorés par le
`switch` de `0x823246C0`.

Commencer par le bit-reader et ses producteurs/consommateurs; aucun runtime
tant que cette question est répondable statiquement. Confirme : une règle
reproduit `0x1A`, `0x2E`, `0x10`, etc. Réfute : le loader produit un flux déjà
final et l'interpréteur ignore ces mots. Indécidable : source compacte non
qualifiée.

# Gate suivant — appelants de la validity gate mission

`done_when` : le projet Ghidra démo qualifie les appelants directs de
`0x820E9300` et le prochain consommateur de dispatch, ou identifie une unique
fonction native manquante. Commencer par le CFG et les xrefs; ne lancer une
sonde bornée que si cette chaîne statique reste ambiguë.

# Gate suivant — helpers de publication de validity

\`done_when\` : \`0x8218E088\` et \`0x8218EA88\` sont décompilés et remplacés par
des équivalents natifs vérifiés contre leurs corps générés. Commencer par les
bornes de table et le store \`+0x10\`; ne pas lancer de runtime tant que cette
question statique est fermée.

# Gate suivant — appelant validity court

\`done_when\` : le corps Ghidra et le corps généré de \`0x8216DB10\` sont
concordants et la fonction est native, ou une ambiguïté statique précise est
documentée. Reprendre les extractions déjà présentes dans
\`artifacts/mission-validity-callers-static-gate/\`; aucune sonde runtime tant
que ce contrat reste répondable statiquement.

# Gate suivant — appelant validity \`sub_8216D760\`

\`done_when\` : CFG, appels enfants et écritures d'état de \`0x8216D760\` sont
qualifiés entre Ghidra et le corps généré, puis couverts nativement si le
contrat est complet. Continuer avant toute nouvelle sonde gameplay.

# Gate suivant — enfants récursifs de \`sub_8216D760\`

\`done_when\` : les CFG et listes chaînées de \`0x8219EE40\` et \`0x8219EC88\`
sont qualifiés dans le projet Ghidra démo et remplacés au point d'exécution
commun, ou une ambiguïté précise sur leurs appels virtuels est documentée.
Commencer par leurs corps générés, les slots et les producteurs de leurs têtes
de liste; aucun runtime tant que le contrat statique reste fermable.

# Gate suivant — runtime ciblé Mission 01 des appelants natifs

`done_when` : une fenêtre bornée, avec `SDL_AUDIODRIVER=dummy` sous Xvfb,
observe l'entrée d'au moins un des six appelants natifs et son dispatch slot 1
qualifié, puis un marqueur d'entrée Mission 01; sinon la fenêtre doit produire
un point d'arrêt statique/runtime discriminant.

Observations prévues avant lancement :

- confirmation : entrée `sub_821A00E8`/`0180`/`0240`/`02C0`/`0338`/`03A8`,
  arguments et retour `r3`, stores du nœud, LR/slot 1 qualifié, absence de
  dispatch indirect non qualifié et progression vers le marqueur Mission 01;
- réfutation : trap, cible/slot ou mappage de champs inattendu, ou dispatch
  indirect non qualifié avant ces appelants;
- indécidable : la fenêtre se termine avant tout appelant ou avant le marqueur,
  sans contradiction observée.

Ne capturer que cette fenêtre et ces observables; ne pas lancer de trace globale
ni d'exécution gameplay complète.
# Gate suivant — producteur de l'alias free-list `0x827745E0`

`done_when` : le producteur statique/runtime du pointeur de tas placé dans la
chaîne `0x827745E0` est identifié, ou la valeur reste indécidable à la frontière
d'un service externe.

Le gate précédent est réfuté par un trap à `0x8219ECBC` avant Mission 01. Les
écritures hôte bornées confirment que `ac6_mission_take_node` dépile une chaîne
dont un `next` vaut `0x18BB0300`, puis réécrit le mot 0 de cet objet. Confirme :
un producteur qualifié explique l'insertion de cette adresse dans E0. Réfute :
E0 reste dans le pool statique et l'objet est corrompu par un autre écrivain.
Indécidable : la transition du free-list est effectuée par une allocation ou un
service non couvert par les observables locaux. Ne pas ajouter de garde au
dispatcher; capturer seulement les écritures/lectures E0 et les producteurs
directs jusqu'au tick 61.

# Gate suivant — producteur du sélecteur resolver `0x30`

`done_when` : le slicing statique relie l'index `0x30` au producteur de la
table/descripteur `0x18BD1000`, ou établit qu'il franchit une frontière de
service externe.

Fait établi : au tick 67, `0x821EE0F8` reçoit `index=0x30` avec `count=2` et
retourne zéro; le trap suivant est une conséquence déterministe. Confirmer :
un producteur PPC montre l'origine du sélecteur et son contrat de données.
Réfuter : le même index est borné ou la table possède une troisième entrée dans
le chemin réellement exécuté. Indécidable : la valeur est injectée par un
service non couvert par les artefacts locaux. Reprendre par les producteurs et
consommateurs statiques du champ index, sans garde runtime ni nouvel A/B.

# Gate suivant — producteur virtuel amont de l'index resolver

`done_when` : le slot virtuel d'offset 160 appelé à `0x82191148` est relié à
un corps PPC/service concret qui explique la valeur, ou la frontière externe
est explicitement établie.

Le gate précédent est fermé jusqu'à `sub_821A03A8`: `r8` devient
`object+0x20`, puis `sub_8219E7B0` le consomme comme index. Confirmer : une
cible de vtable qualifiée produit le contrat et la valeur. Réfuter : un autre
écrivain local de `+0x20` est le chemin réellement exécuté. Indécidable : la
cible reste un service/runtime non couvert par les artefacts statiques.
Priorité aux xrefs/vtables et au slicing statique; ne lancer un runtime borné
que si cette frontière ne peut pas être fermée autrement.
# Gate suivant — contrat du resolver après l’index `0x30`

`done_when` : qualifier statiquement si `sub_821EE0F8` doit refuser l’index
`0x30` avec le descripteur `count=2`, ou identifier une incohérence de
producteur/layout; ne pas ajouter de garde avant cette preuve.

Confirmation : corps PPC/native du resolver, layout du descripteur et
producteurs/consommateurs de la table concordent, ou un harness local reproduit
le contrat sans ambiguïté.

Réfutation : une troisième entrée valide existe dans le chemin exécuté, ou le
champ observé comme `count` n’est pas la borne de recherche.

Indécidable : le contrat est fourni par un service/runtime non couvert par les
artefacts statiques. Priorité au slicing local; runtime seulement si le statique
reste insuffisant.
# Gate suivant — réconciliation du champ resolver `+0x20`

`done_when` : qualifier statiquement pourquoi la valeur `0x30` fournie par
`sub_82191088`/ `sub_821A03A8` est consommée comme index dans un descripteur
`count=2`, ou établir que cette divergence appartient à une donnée/service
externe.

Confirmation : un layout d’objet ou un producteur PPC montre que `+0x20`
devrait être un index borné, et identifie le champ/initialisation divergente.

Réfutation : le champ est démontré comme un offset/identifiant distinct et un
mapping statique manquant explique la conversion vers un index valide.

Indécidable : la conversion est fournie par un service/runtime hors des
artefacts locaux. Ne pas modifier le resolver ni lancer de runtime avant cette
réduction statique.

# Gate suivant — producteur de l'objet nul consommé par `sub_8219E580`

`done_when` : relier statiquement le vtable observé `0x82012DC4` et l'objet
`0x18960100` au constructeur/producteur qui précède `sub_8219E580`, puis
qualifier l'initialisation de `object+8`.

Confirmation : le constructeur exécuté initialise légitimement `+8` à zéro et
aucun setter n'est attendu avant le consommateur.

Réfutation : un chemin producteur ou un setter qualifié doit écrire une cible
non nulle pour ce même objet, ou l'objet est démontré comme un alias du chemin
slot 16 déjà confirmé.

Indécidable : le vtable ou le producteur est fourni par un service/runtime
extérieur aux corps PPC locaux. Priorité aux vtables, constructeurs, xrefs et
slicing; un runtime borné ne sera requis qu'après cette réduction statique.

# Gate suivant — producteur de l'argument `out` au site F42C

`done_when` : qualifier statiquement la valeur placée dans le buffer `out` par
`sub_8219F300` (chemin `sub_8219EE40` ou fallback `sub_821E1CD8`) et expliquer
pourquoi F42C reçoit zéro pour les objets E550 observés.

Confirmation : un slicing PPC montre une écriture non nulle dans `out` avant
F42C mais le chemin natif diverge, ou identifie le producteur qui doit fournir
la cible consommée par E580.

Réfutation : le chemin PPC qualifié produit réellement zéro et aucune cible
n'est attendue pour ce type; ne pas ajouter de garde à E580.

Indécidable : la valeur de `out` est fournie par un service/état externe non
présent dans les corps locaux. Runtime borné seulement si le slicing statique
ne distingue pas ces cas; observations prévues : valeur de `out` au F42C,
retour de `sub_8219EE40`/fallback et identité objet.

# Gate suivant — progression mission après correction ABI F300

`done_when` : le runtime natif franchit le jalon mission dans une fenêtre
bornée, ou un nouveau trap/attente est qualifié par les observables scheduler
et guest minimaux.

Fait établi : le cadre F300 corrige la collision de pile et trois appels F42C
reçoivent désormais une cible non nulle; le probe à 100 ticks reste
`max_ticks` avant mission.

Confirmation : `milestone_reached=true`/mission atteint avec absence de trap.

Réfutation : un trap ou une attente stable apparaît après F300; qualifier alors
uniquement le prochain appel/enregistrement fautif.

Indécidable : la fenêtre bornée termine sur budget scheduler sans nouveau
observable discriminant; prolonger seulement la fenêtre, sans trace globale ni
nouvelle correction speculative.

# Gate suivant — événement de réveil des threads mission

`done_when` : identifier statiquement ou par trace bornée le wait/événement
responsable de `runnable=0, blocked=23`, et démontrer soit son producteur, soit
un nouveau retour au scheduler.

Confirmation : un événement qualifié réveille le thread mission et le jalon
mission devient atteignable dans une fenêtre courte.

Réfutation : un wait sans producteur local, une file vide ou un contrat kernel
manquant est démontré; conserver la frontière explicite et ne pas ajouter de
réveil synthétique.

Indécidable : les compteurs restent identiques sans observables locaux; limiter
alors la capture aux appels wait/set-event et à leurs clés.

# Gate suivant — frontière audio/XMA du réveil mixer

Qualifier statiquement le callback render-driver enregistré, le global d'état
audio qu'il consomme et les imports XMA/XAudio susceptibles de publier les huit
événements auto-reset. Utiliser les sources générées et le bridge; ne lancer un
runtime borné que si le slicing ne peut pas distinguer « callback jamais appelé »
de « callback appelé mais publication manquante ». Done_when: chaque événement
du mixer a un producteur console/native explicitement identifié, ou la frontière
audio/XMA est déclarée indécidable sans oracle externe. Ne pas injecter de
publication synthétique dans le chemin par défaut.

# Gate suivant — frontière audio/XMA externe

Le gate local est fermé: le producteur manquant des huit événements est le
pilote audio console, et le bridge ne doit pas le simuler. Une suite ne peut
être ouverte que par une preuve officielle/SDK ou une observation oracle
explicitement bornée qui qualifie le callback audio et les handles concernés.
Done_when: la source externe est identifiée pour chaque handle, ou la limite
reste déclarée indécidable; aucun signal synthétique dans le chemin de parité.

# Gate suivant — preuve externe bornée du producteur audio/XMA

`done_when` : une source officielle XDK/xboxkrnl ou une capture oracle
strictement bornée relie le callback render-driver à un ou plusieurs des huit
handles d'attente du mixer, avec arguments et changements d'état observés.

Confirmation : callback dispatché, handle exact publié, waiter correspondant
réveillé et contrat de cadence/arguments qualifié.

Réfutation : la source externe montre une voie sans publication vers ces
handles; conserver alors la frontière et ne pas ajouter de réveil natif.

Indécidable : aucune source externe autorisée ou aucun handle observable;
laisser le gate ouvert sans activer le chemin expérimental ni lancer une trace
globale.

# Gate suivant — activation du callback après construction guest

Le constructeur guest de `0x829DA528` est maintenant fermé :
`artifacts/audio-xma-client-state-producer-gate/gate.status` montre une
exécution complète de la chaîne au tick 106. Le callback `0x8236DD98` et le
dispatcher `0x82355E58` restent absents de l'atlas jusqu'au tick 253.

`done_when` : une source statique qualifie le chemin d'appel du callback et
son producteur d'événements, ou une capture externe bornée observe le callback,
le handle publié et le waiter réveillé.

Confirmation : callback appelé après la construction, handle d'un des huit
événements identifié et waiter correspondant réveillé.

Réfutation : le callback est appelé mais aucune publication ne correspond à
ces handles; conserver alors la frontière console audio/XMA.

Indécidable : aucun contrat XDK qualifié et aucun observable externe autorisé;
ne pas activer `AC6_DEMO_EXPERIMENTAL_XAUDIO_DRIVE` ni synthétiser un signal.

# Gate suivant — enfant du callback XAudio et publication mixer

Le gate d'activation est fermé par
`artifacts/audio-xma-callback-activation-gate/gate.status`: sous une sonde
explicitement expérimentale, `0x829DA528` devient `0x1005CEBC` et
`0x8236DD98` est appelé aux ticks 62–67. Le premier accès non mappé est
`address=0x4`, `lr=0x82278184`, au tick 67.

`done_when` : qualifier statiquement l'accès fautif dans l'enfant
`0x82355E58`, puis relier — ou réfuter — une publication vers un des huit
handles auto-reset du mixer. Une preuve runtime éventuelle reste limitée à
l'appel enfant, ses arguments et le changement d'état du handle; aucun signal
synthétique et aucune activation par défaut.

Confirmation : le champ fautif et un `KeSetEvent`/réveil correspondant sont
qualifiés avec leurs arguments.

Réfutation : le callback atteint seulement une voie qui ne publie aucun handle;
conserver alors la frontière pilote audio/XMA console.

Indécidable : le layout enfant reste opaque après le slicing statique; laisser
la frontière ouverte et ne pas modifier le bridge.

# Gate suivant — descripteur r4 et signal KeSetEvent du callback

Le gate enfant est fermé par
`artifacts/audio-xma-callback-child-gate/gate.status`: le trap `address=0x4`
est la première lecture `r4+4` de `0x821EDF40`, tandis que `r3` est le client
construit depuis `0x829DA528`.

`done_when` : trouver un producteur binaire qualifié du descripteur passé en
`r4`, puis relier ou réfuter le `KeSetEvent` de `0x82355EA8` à un handle du
mixer. Toute sonde reste limitée à cet argument, aux octets lus et au handle
concerné; ne pas patcher le bridge ni synthétiser un signal.

Confirmation : un producteur fournit un pointeur guest non nul dont les
octets `+4..+6` sont lus sans trap, et l'événement publié est identifié.

Réfutation : les chemins qualifiés laissent `r4` nul ou aucun chemin ne
publie un handle mixer; conserver alors la frontière XDK/XMA console.

Indécidable : le descripteur ne peut être produit que par un contrat externe
non qualifié; laisser la frontière ouverte.

## Après le gate `audio-xma-r4-descriptor`

Le descripteur d'enregistrement est qualifié, mais aucun producteur guest du
`r4` consommé par l'enfant du callback n'existe dans les chemins statiques et
runtime bornés. Le contrat XDK/XMA du contexte enfant reste donc la frontière
active. Une prochaine sonde, si elle devient nécessaire, doit capturer
uniquement l'argument `r4`, les octets `+4..+6` et l'objet/handle d'événement
concerné; elle doit aussi attribuer explicitement le callsite `0x82355EA8`
avant toute conclusion sur le mixer.

Preuve : `artifacts/audio-xma-r4-descriptor-gate/gate.status`.

## Après le gate `audio-xma-event-object`

L'objet `0x829DA518` est maintenant qualifié comme événement interne de la
chaîne worker XAudio; `0x829DA4E4` reste l'événement de terminaison distinct.
Le pont n'est pas modifié et aucun signal n'est synthétisé. La prochaine
frontière est exclusivement le contrat privé du contexte enfant `r4` de
`0x82355E58`/`0x821EDF40`, ou une preuve console XDK/XMA équivalente. Une
sonde éventuelle doit rester bornée à `r4`, aux octets `+4..+6`, à l'objet
événement et au retour de l'appel enfant.

Preuve : `artifacts/audio-xma-event-object-gate/gate.status`.

## Après le gate `next-xdk-xma-r4-context`

Le chemin Xenia r3-only est maintenant séparé du contrat enfant AC6 : le
callback privé consomme un `r4` structurel que le guest ne produit pas dans les
chemins qualifiés, et `object+0x0c` est nul. Le couple CPU4 n'est pas promu sans
preuve. La prochaine action utile est uniquement d'obtenir une source/SDK
XDK-XMA qualifiée ou une capture bornée de `r4`, des octets `+4..+6` et du
handle propriétaire; sinon conserver la frontière et ne pas modifier le pont.

Preuve : `artifacts/next-xdk-xma-r4-context-gate/gate.status`.

## Après le gate `xma-output-slot-table`

La table et les writers guest du slot XMA sont maintenant qualifiés :
`entry+0x40` est un output slot d'import, initialisé par le zero-fill et
réinitialisé au teardown. Ne pas le traiter comme un buffer PCM ni promouvoir
le couple CPU4. La frontière active reste l'ABI privé de `XMACreateContext`,
le pointeur post-appel et l'effet du store MMIO `0x823572D8`; une prochaine
action doit d'abord fournir une source XDK/XMA ou une observation bornée de
ces valeurs avant toute modification du bridge.

Preuve : `artifacts/xma-output-slot-frontier-gate/gate.status`.

## Après le gate `xma-context-free-list`

Le contrat public SDK est maintenant appliqué au bridge : les releases XMA
réintègrent la free-list native et les allocations suivantes réutilisent les
slots libérés. Le type `IXMAContext` reste opaque; ne pas changer la taille de
64 octets, le couple CPU4, ni le store MMIO sans preuve AC6/XDK distincte.
La frontière active est l'effet privé du contexte XMA et du registre
`0x823572D8`; une prochaine action doit qualifier ce consommateur par source
ou observation bornée avant tout nouveau patch.

Preuve : `artifacts/xma-context-free-list-gate/gate.status`.

## Après le gate `xma-private-mmio`

La couche privée XMA est maintenant identifiée par les PDB XDK (`_XMA_CONTEXT_DATA`,
`_XMA_REGISTERS`, `CXMADecoder`), mais ses offsets et son effet matériel ne le
sont pas. Ne pas transformer les mappings expérimentaux `0x7FEA1A80`,
`0x7FEA1940` ou `0x7FEA1A40` en registres fonctionnels, ni synthétiser de
signal audio. La prochaine preuve discriminante doit extraire un layout/offset
privé depuis une source qualifiée, ou capturer bornément le changement d'état
XMA autour de `0x823572D8`.

Preuve : `artifacts/xma-private-mmio-gate/gate.status`.

Le PDB fournit maintenant les coordonnées CodeView des routines XMA ; les
utiliser pour un désassemblage ciblé du noyau, en conservant explicitement la
question de translation segment/offset vers l'adresse guest.

## Après le gate `xma-private-mmio` — rôles indexés

L'arithmétique des contextes matériels et les rôles `Enable`/`Disable` sont
fermés par le noyau officiel : blocs de 64 octets, groupes de 32 bits et
stores one-hot aux bases `0x1A80`, `0x1940`, `0x1A40`, avec lecture `0x1840`.
Ne pas en déduire le layout `_XMA_REGISTERS`, les registres directs de
`CXMADecoder::Initialize`, ni un propriétaire `XMAInitializeContext -> 1A80`;
la contribution publique ne contient pas ce store indexé. Ne pas activer les
mappings expérimentaux ou modifier `0x823572D8`.

La prochaine preuve discriminante doit venir d'un layout privé qualifié ou
d'une observation bornée de l'état du décodeur après cet appel guest. Rapport :
`artifacts/xma-private-mmio-gate/xma-kernel-register-addresses.txt`.

## Après le gate `xma-register-layout`

Les sources officielles disponibles sont épuisées pour le layout privé : SDK
opaque et TPI PDB sans records. Xenia/ReXGlue donnent seulement une table
générique, à conserver comme contexte de comparaison et non comme contrat
AC6. Ne pas activer `0x1A40`, `0x1940` ou `0x1A80`, ni modifier
`0x823572D8` sur cette base.

Le prochain test discriminant est une capture bornée de `context+0..+63`, du
store `0x823572D8`, des accès immédiats aux familles `0x1840`/`0x1940`/
`0x1A40`/`0x1A80` et des retours enfants. Les critères de confirmation/réfutation
sont dans `artifacts/xma-register-layout-gate/hypothesis-criteria.txt`.

## Après la capture bornée `xma-runtime-state`

La capture des trois premiers contexts est fermée: les stores one-hot sont
acceptés mais les 64 octets restent nuls et aucun enfant immédiat n'est
observé. Ne pas transformer ce résultat en layout privé ou en effet audio.
La prochaine gate doit rechercher statiquement le premier consommateur
device-facing post-store (xrefs, accès `0x1840`/`0x1940`/`0x1A40`, ou état
matériel qualifié) avant tout nouveau runtime. Une nouvelle capture ne se
justifie que si cette recherche fournit un discriminant distinct.

Preuve : `artifacts/xma-runtime-state-gate/capture-analysis.txt`.

## Après le gate `xma-post-store-consumer-static`

Le PAL qualifié ne contient aucun consommateur fixe de `0x7FEA1A80` et
`0x82357240` ne lance aucun enfant après son store. Ne pas refaire la même
capture ni promouvoir `1A80` en registre `Kick`. La prochaine preuve doit
suivre statiquement les appels ultérieurs `0x82357310`/`0x823575A8` et leur
appelant, ou qualifier un consommateur noyau officiel; un runtime n'est
justifié que si cette analyse fournit une adresse/état discriminant nouveau.

Preuve : `artifacts/xma-runtime-state-gate/post-store-static-decision.txt`.

## Après le gate `xma-later-lifecycle-static`

La chaîne ultérieure est maintenant bornée statiquement : le registre direct
`0x7FEA1818..0x7FEA181B` sert de comparaison d’index dans `0x82357458`, les
entrées sont recopiées/neutralisées, puis `0x823575A8` flushe le cache,
recopie vers `entry+0x40` et soumet le one-hot à `0x7FEA1940`. Le chemin de
récupération pose aussi `0x7FEA1804=0x03000000`. Ne pas appeler ces valeurs
des registres XMA nommés, ne pas synthétiser de PCM et ne pas activer le bridge.

La prochaine gate doit qualifier le consommateur noyau ou la traduction de cet
état matériel par une source officielle; un runtime n’est justifié que si cette
source laisse une ambiguïté ciblée. Preuve :
`artifacts/xma-runtime-state-gate/later-chain-static-decision.txt`.

## Après le gate `xma-kernel-consumer`

Le consommateur noyau est maintenant qualifié statiquement : l'ISR officielle
`CXMADecoder::InterruptServiceRoutine` lit `0x7FEA1808` et écrit les masques
`0x100/0x200/0x400` à `0x7FEA1A08`. Ne pas attribuer de noms privés ou de PCM à
ces valeurs. La prochaine preuve doit récupérer le layout/les noms de
`_XMA_REGISTERS` par une source qualifiée, ou définir une observation native
bornée qui distingue l'état matériel; ne pas répéter le trap du cinquième bit.

Preuve : `artifacts/xma-kernel-consumer-gate/decision.txt` et `gate.status`.

Le layout privé n'est pas récupérable par les headers XDK, le TPI PDB apparié
ou les sources Xenia locales. Si une observation est nécessaire, elle doit
rester limitée aux lectures `0x7FEA1808`, écritures `0x7FEA1A08`, tick/thread et
à leur ordre relatif autour des trois premiers stores `1A80`; les critères
H1/H2 sont pré-enregistrés dans
`artifacts/xma-kernel-consumer-gate/next-observation.md`.

## Après le gate d'instrumentabilité native ISR

La route native générée n'implémente pas l'ISR XMA et ne mappe pas ses deux
registres directs; la lecture `0x1808` ne peut même pas atteindre l'observateur
sans modifier la sémantique du bridge. H1/H2 restent donc indécidables, sans
nouvelle capture ni patch MMIO. La prochaine gate est la qualification statique
de l'ABI/du retour des imports XMA `XMACreateContext`/`XMAReleaseContext`, ou
une autre frontière PAL indépendante.

Preuve : `artifacts/xma-kernel-consumer-gate/native-isr-decision.md`.

## Après le gate ABI XMA create/release

L'ABI officielle et le cross-match du callsite `0x82357298` sont fermés; le
bridge satisfait le contrat sans implémenter de PCM. La prochaine gate doit
suivre statiquement le consommateur du contexte après `MmGetPhysicalAddress`
ou ouvrir une autre frontière PAL; elle ne doit pas ajouter les imports XMA
publics qui ne sont pas appelés par ce XEX.

Preuve : `artifacts/xma-import-abi-gate/decision.md`.

## Après le gate historique mismatch d’adresse post-création XMA

Ce bloc est historique et supersédé; ne pas reprendre son hypothèse de base
fixe.

Le chemin natif transmet l’adresse brute au MMIO et `MmGetPhysicalAddress`
retourne une identité d’allocation; l’allocateur page-aligné commence à
`0x10000000`, alors que la globale PAL finit par `0x52C`. L’identité actuelle
ne peut donc produire le store exact `0x7FEA1A80` (l’exemple de première
allocation donne `0x7FEA2BAC`, non mappé). Ne pas élargir le mapping ou
modifier la fonction noyau sans qualifier le producteur de l’adresse physique
XMA. Cette lecture est supersédée : `0x829DA52C` est le slot écrit par
`sub_82356510` depuis `0x7FEA1800`, pas une base fixe. Ne pas modifier
`MmGetPhysicalAddress` ni élargir le mapping sur cette base.

## Après le gate producteur statique de la base XMA

Le producteur et le consommateur de la base sont maintenant reliés : le même
tableau XMA passe de `0x7FEA1800` à `0x829DA52C`, puis au calcul d'index de
`sub_82357240`; la trace bornée confirme la chaîne. La prochaine gate doit
qualifier le consommateur privé après le `stwbrx` ou ouvrir une autre frontière
PAL indépendante, sans ajouter de sémantique PCM ou de registre non prouvée.

Preuve : `artifacts/xma-physical-producer-gate/producer-summary.md` et
`gate.status`.

## Après le gate consommateur privé tardif XMA

La chaîne guest tardive est fermée : `0x82357310` soumet le one-hot
`0x7FEA1A40`, `0x823575A8` recopie les entrées puis soumet
`0x7FEA1940`, et `0x82357390`/`0x82357458` encadrent les accès directs
`0x7FEA1804`/`0x7FEA1818`. Ces faits ne donnent toujours ni layout privé ni
effet hardware/PCM. Ne pas activer ces mappings et ne pas répéter le trap
connu. La prochaine gate doit qualifier un consommateur device-facing avec
une nouvelle preuve statique ou, à défaut, une fenêtre runtime strictement
bornée aux apertures et aux entrées concernées.

Preuve : `artifacts/xma-private-consumer-gate/decision.md` et `gate.status`.

## Après le gate des formats de fetch Xenos

Les formats atteints `57` (position, trois composantes) et `38` (couleur,
quatre composantes) sont maintenant qualifiés statiquement, sans effet de
rendu revendiqué. La prochaine gate doit joindre le producteur du vertex
buffer à ces fetches et établir la sortie exacte du pixel shader. L'absence de
l'un de ces deux éléments laisse le draw indécidable et interdit tout bypass
du traducteur fail-closed.

Preuve : `artifacts/shader-static-frontier-gate/decision.md` et `gate.status`.

## Après le gate producteur du vertex buffer

La plage guest, le préfixe de 84 octets, le binding 2 et l'offset local sont
maintenant joints statiquement aux deux fetches du draw atteint. Ne pas ajouter
de vertex buffer hôte ni contourner le fetch ReXGlue. La prochaine gate doit
qualifier la sortie exacte du pixel shader et le writer EDRAM avec un nouveau
discriminant statique; si cette jointure échoue, l'effet du draw reste
indécidable et aucun runtime global ne doit être lancé.

Preuve : `artifacts/vertex-buffer-producer-gate/decision.md` et `gate.status`.

## Après le gate sortie pixel et writer PM4

L'export exact du bootstrap pixel est maintenant fermé : `oC0` part de `r0`
zéro, et le draw atteint couvre toute la surface avant de produire le readback
zéro. Les writers guest des paquets RT0 puis `RB_COPY` sont également joints,
mais ils ne donnent pas les samples EDRAM matériels. Le prochain gate doit
trouver le premier pixel shader atteint non-bootstrap ou une capture bornée du
consommateur EDRAM avant `RB_COPY`; ne pas transformer la projection native du
readback en preuve de contenu guest-owned.

## Après le gate frontière statique pixel non-bootstrap

La route neutre/START est désormais fermée statiquement : toutes les tailles
pixel atteintes sont 9 dwords et les cinq candidats NSXR de 60 octets ne sont
pas joints à un draw. Ne pas sélectionner un candidat par identité de
container et ne pas déduire les samples EDRAM depuis le seul paquet
`RB_COPY`.

La prochaine gate doit ouvrir une route réellement nouvelle après START ou
vers le gameplay, avec une trace bornée au premier `IM_LOAD` pixel différent,
au draw associé, au writer RT0 et à l’observation EDRAM minimale. Si aucun
nouveau load n’est atteint, la question reste indécidable et le runtime ne
doit pas être élargi.

Preuve : `artifacts/next-nonbootstrap-gate/decision.md` et `gate.status`.

## Après la fenêtre runtime post-START

La fenêtre native unique n'a pas atteint frontend : 3 036 ticks, START borné,
2 928 présentations, tous les threads guest bloqués, aucun événement
shader/draw/pixel/EDRAM exploitable. Ne pas relancer Xenia Wine/Edge, l'ancien
runner ou une A/B ; la question pixel reste indécidable.

Prochaine action : joindre statiquement l'adresse/LR de frontière et les attentes
kernel du blocage guest, puis seulement préparer une nouvelle fenêtre si le
chemin vers frontend est démontré.

Preuve : `artifacts/post-start-runtime-window/decision.md` et `gate.status`.

## Après la qualification du blocage guest

Le frontier est maintenant borné à l'attente événement `0xE000004C` après la
ré-entrée dynamique `0x822E559C -> 0x822F8848`; 23 threads restent bloqués.
Ne pas modifier le scheduler, la cible dynamique ou le runtime Xenia. La
prochaine gate doit joindre statiquement le producteur de cet événement et la
paire signal `0xE0000048`, puis seulement réévaluer une capture bornée.

Preuve : `artifacts/static-guest-block-gate/decision.md` et `gate.status`.

## Après la qualification du producteur d'événement

Ne pas relancer Xenia Wine/Edge ni élargir le probe. Le producteur de
`0xE000004C` est maintenant joint : `sub_822EEE10` → `NtSetEvent`, avec la
paire initialisée par `sub_822EED70` et consommée par
`sub_822E4080` → `NtSignalAndWaitForSingleObjectEx`.

Prochaine gate : qualifier statiquement l'ownership et le scheduling du writer
au frontier post-START; si cette jointure ne ferme pas la cause, pré-enregistrer
un unique test borné avec les observations confirmant, réfutant ou laissant la
question indécidable.

Preuve : `artifacts/static-event-producer-gate/decision.md` et `gate.status`.

## Après la qualification de l'ownership du writer

Le thread 12 est le producteur normal de `0xE000004C`, mais il attend
`0xE0000040` au frontier; les 23 fibres sont bloquées et aucune exécution host
ne peut contourner cette attente. Ne pas modifier le scheduler et ne pas
relancer Xenia Wine/Edge.

Prochaine gate : joindre statiquement le producteur de `0xE0000040`, puis
pré-enregistrer un test borné uniquement si cette jointure indique une voie
réveillable.

Preuve : `artifacts/static-event-writer-ownership-gate/decision.md` et
`gate.status`.

## Après la qualification du producteur de `0xE0000040`

Le producteur est la chaîne `0x822E5660 -> 0x822E3EB8 -> 0x821A6AB0`, sur le
handle chargé à `0x82934760`, observée statiquement sur le thread 2. Ne pas
relancer Xenia Wine/Edge et ne pas modifier le scheduler. La prochaine gate
doit qualifier statiquement l'enregistrement du callback au LR indirect
`0x821C5178` et son lien avec le démarrage/affinité du thread 2; seulement si
cette jointure reste indécidable, pré-enregistrer un test borné avec les
observations discriminantes.

Preuve : `artifacts/static-event-producer-0xE0000040-gate/decision.md` et
`gate.status`.

## Après la qualification de l'enregistrement du callback

Le callback `0x822E5660` est installé dans le slot `+16520` par le thread 1,
puis dispatché par `0x821C5090` sur le thread 2 à LR `0x821C5178`. Ne pas
relancer Xenia Wine/Edge et ne pas modifier le scheduler. La prochaine gate
doit joindre statiquement l'objet de registration à l'affinité et au démarrage
du thread 2; un runtime borné reste différé tant que cette jointure est
possible.

Preuve : `artifacts/static-callback-registration-gate/decision.md` et
`gate.status`.

## Après la jointure statique objet/affinité

L’objet retourné par `0x821BB4C8` est maintenant le même pointeur que celui
enregistré par `0x822E5670`; son état `+0x56F8` vaut `0x0C000001`. Aucun appel
statique à `KeSetAffinityThread` ne part de cette construction et aucun appel
dynamique d’affinité n’est observé sur le thread 2. La correspondance CPU
reste donc indécidable sans runtime ciblé.

Prochaine gate : une seule fenêtre bornée autour de `0x821C5090` et de LR
`0x821C5178`, capturant `r31`, `r31+16520`, le thread guest et la valeur
d’affinité publiée. Ne pas modifier le scheduler, ne pas lancer Wine/Edge et
ne pas ajouter de patch natif.

Preuve : `artifacts/static-owner-object-affinity-gate/decision.md`,
`owner-affinity-detail.txt` et `gate.status`.

## Après la fenêtre runtime objet/affinité

La correspondance objet → callback → thread 2 est maintenant confirmée, mais
aucune transition d'affinité du thread 2 n'est observée. Ne pas choisir de CPU
par hypothèse et ne pas modifier le scheduler. Prochaine gate : validation
locale native ciblée (contrats existants, puis build/ctest qualifié) ; `-O3`
est autorisé uniquement si cette validation reste identique. Wine/Edge reste
hors de cette preuve.

Preuve : `artifacts/bounded-dispatch-object-affinity-gate/decision.md`,
`runtime-summary.txt` et `gate.status`.

## Après la validation native O3

La validation locale est fermée et `-O3` est autorisé pour les prochaines
compilations tant que les mêmes tests restent verts. La prochaine gate doit
qualifier visuellement l'état graphique du runtime natif et, seulement si le
jalon de gameplay est atteint, conserver une screencap bornée. Ne pas utiliser
Wine/Edge comme preuve de parité et ne pas modifier le scheduler sur la seule
absence d'affinité du thread 2.

Preuve : `artifacts/native-validation-o3-gate/validation.txt` et
`gate.status`.

## Après l'observation visuelle native

La capture native est réfutée dans la fenêtre actuelle : aucun writeback
graphique qualifié ni jalon frontend n'est atteint. La prochaine observation
discriminante est donc statique et ciblée sur le blocage guest qui précède le
frontend. Ne pas relancer une screencap identique, ne pas optimiser le rendu et
ne pas utiliser Wine/Edge comme preuve de parité.

Preuve : `artifacts/visual-native-gameplay-gate/validation.txt` et
`gate.status`.

## Après la qualification du réveil 0xE0000040

Ne pas relancer la même sonde 0040, ne pas modifier le scheduler, les objets
d'événement ou le rendu. La prochaine gate est statique : reconstruire les
arêtes et l'état guest du consommateur après reprise à `0x821A8C88`, puis du
chemin primaire `0x821A69CC`, afin d'expliquer le retour immédiat en attente
avant le frontend.

Preuve : `artifacts/event0040-native-producer-gate/validation.txt` et
`gate.status`.

## Après la fermeture de la boucle guest post-réveil

Ne pas modifier le scheduler, les événements, le rendu ou les wrappers d'attente.
La prochaine gate reste statique : relier le thread primaire et ses handles
0048/004C aux compteurs `puVar1+4` et aux drapeaux de sortie des boucles
`0x822E4018`, `0x822E4080` et `0x822EEE68`, puis déterminer le producteur qui
devrait permettre le jalon frontend.

Preuve : `artifacts/postwake-static-gate/validation.txt` et `gate.status`.
## Gate suivant — producteur du pointeur transmis à `FUN_82324188`

`done_when` : un producteur statique du pointeur d'objet est relié à son allocation ou à l'initialisation du champ `+0xE8`, ou son absence est établie dans le sous-graphe qualifié.

Partir du seul appel direct `0x820D2B18` de `FUN_82324188`, suivre l'argument qui devient l'objet de contexte et ses producteurs. Éviter toute recherche globale de slot ; rester en analyse statique.
## Gate suivant — instance concrète de la vtable `0x820064D8`

`done_when` : une instance construite statiquement avec la vtable `0x820064D8` est reliée à son site d'appel, ou les constructeurs qualifiés établissent que l'instance ne peut pas être retrouvée par xrefs directes.

Repartir des écritures de vtable dans `Function_820D0AD0` et `Function_820D2738`, puis suivre leurs sites de construction vers les objets persistants. Conserver une analyse statique ; ne pas relancer la recherche globale par offset.
## Gate suivant — writer EDRAM/source `RB_COPY`

`done_when` : le producteur statique de `RB_COPY`, ses plages source/destination et son premier consumer de surface sont qualifiés, ou une absence de chemin vers PRESENT est établie dans le sous-graphe.

Lire d'abord les rapports et outils existants relatifs à `RB_COPY`, puis les structures de commandes et leurs consumers. Définir ensuite un harness natif : une commande bornée doit produire une écriture surface observable. N'utiliser Edge qu'en cas d'ambiguïté statique résiduelle, avec commande, buffers et changement de surface strictement bornés.
## Gate suivant — premier writer non noir du draw RT0

`done_when` : les états shader, ressources et buffers consommés par `execute_vulkan_normal_draw` sont reliés à leur producteur guest, ou le sous-graphe statique établit que la couleur noire est injectée avant le backend.

Partir du draw RT0 à l'offset 239 et de `execute_vulkan_normal_draw`; suivre les constantes, descripteurs et shaders jusqu'aux producteurs guest. Créer ensuite un test synthétique qui injecte uniquement des entrées déjà qualifiées et vérifie un readback non noir avant EDRAM. Edge ne doit être utilisé que si une valeur de ressource ou de shader demeure indécidable.
## Gate suivant — sélection native du draw RT0 atteint

`done_when` : une fonction sélectionne uniquement le `XenosDrawCommand` RT0 qualifié depuis le lot de commandes atteint, l'assigne à `normal_draw_command_`, et un test démontre les cas présent/absent sans synthétiser d'image.

Réutiliser les helpers de sélection déjà présents dans `xenos_present_join.hpp` ou ajouter le plus petit prédicat basé sur le reçu IB qualifié. Ne modifier ni shader, ni EDRAM, ni `RB_COPY`; compiler et exécuter uniquement les tests touchés avec les mêmes résultats sous les options de compilation existantes.

## Gate suivant — observable de première soumission IB

`done_when` : la première soumission de l'IB PointList publie son adresse, sa
taille, son tick et si ses octets existaient avant l'entrée guest, puis le
watcher attribue ou exclut une écriture guest antérieure.

Ajouter l'observable au point partagé de soumission, arrêter immédiatement au
premier IB qualifié et conserver la fenêtre mémoire minimale. Ne pas étendre
la durée du probe, ne pas tracer globalement et ne pas lancer Xenia ou Wine.

## Gate fermé — fenêtre configurable des stores générés

Les cinq adaptateurs `AC6_PPC_STORE_*` passent déjà par
`GuestMemory::store_*`, où `AC6_DEMO_WATCH_ADDR_LO/HI` est appliqué. Aucun
changement n'est requis ; voir
`artifacts/generated-store-window-gate/decision.md`.

## Gate suivant — identité et mapping du bootstrap IB courant

`done_when` : les 48 dwords capturés à `0x16AE0980` sur le build codegen 1741
sont qualifiés comme PointList, zéro ou autre contenu, et le premier événement
de mapping couvrant cette adresse est ordonné avant leur capture.

Réutiliser `trace_ib_capture` et la fenêtre adresse existante. Ajouter au plus
l'impression bornée des 48 dwords et d'un événement de mapping chevauchant la
fenêtre, uniquement lorsque le watcher est actif. Exécuter un tick ; ne pas
modifier le parser PM4, le guest généré ou le renderer.

## Gate suivant — payload borné dans `trace_ib_capture`

`done_when` : sous `AC6_DEMO_WATCH_ADDR_LO/HI`, `trace_ib_capture` imprime les
dwords uniquement pour un IB entièrement contenu dans la fenêtre ; un test
synthétique qualifie un payload connu et ignore un IB extérieur ; le probe
d'un tick classe les 48 mots de `0x16AE0980` comme PointList, zéro ou autre.

Modifier seulement `trace_ib_capture` et le plus petit test existant couvrant
le ring Xenos. Ne changer ni la capture d'IB, ni le parser PM4, ni le renderer.


## Gate suivant — frontière post-IB du premier tick

Exploiter d’abord le rapport et la trace déjà produits par le probe qualifié.
Fermer le gate lorsque ces artefacts prouvent si les sept IB, dont les 24 draws
PointList, sont intégralement consommés et identifient le premier état
scheduler, trap ou service manquant qui suit. Ne relancer le runtime que si le
rapport existant ne contient pas l’observable discriminant.
# Prochain gate — scheduling du callback producteur

Qualifier statiquement l'enregistrement et l'ordonnancement du callback
`0x822E5660`, appelé indirectement depuis `LR=0x821C5178`, puis expliquer son
absence au plateau où le thread 12 attend `0xE0000040`.

## Gate suivant — producteur de la commande `present`

`done_when` : le chemin entre le paquet système écrit par `VdSwap` et le
consommateur `RuntimeRendererFrontier::consume` identifie soit le producteur
guest/native précis qui devrait soumettre une commande `present`, soit le
contrat de soumission absent. Partir des xrefs et des consommateurs statiques ;
ne pas prolonger le runtime et ne pas synthétiser de framebuffer.

## Gate suivant — soumission native bornée de `VdSwap`

`done_when` : un test synthétique fait traverser le flux exact produit par
`VdSwap` au point partagé de soumission et observe un unique
`XenosPresentCommand` qualifié ; le probe natif borné retrouve ensuite une
commande `present` sans modifier les pixels ni déclarer le frontend visible.
Réutiliser l'API de soumission existante et garder la modification au point
partagé le plus court.

## Reprise immédiate — résultat du probe `VdSwap`

Lire seulement les trois fichiers nommés dans
`artifacts/vdswap-bounded-submit-implementation-gate/BLOCKER.md`. Fermer le
gate si le rapport prouve `typed_commands.present_count > 0` et une progression
bornée sans trap ; sinon classer l'échec exact avant toute correction.

## Prochain gate — writer WPTR/IB du `present` réel

`done_when` : à partir des artefacts statiques déjà produits sur
`0x821B9BC8`, identifier le store MMIO ou le producteur indirect qui publie le
ring contenant le `XE_SWAP` réel. Commencer par la fenêtre Ghidra déjà extraite
et les consommateurs du bit pending ; ne pas réintroduire de soumission dans
`VdSwap` et ne pas lancer de runtime avant un test statique discriminant.

## Gate suivant — publication asynchrone de `D3D::CDevice::KickOff`

`done_when` : exécuter le vérificateur D3D local sur l'image PAL démo qualifiée
et suivre `KickOff → callback → publication` jusqu'au dernier store, appel
indirect ou contrat externe. Localiser l'image depuis les manifests existants,
sans nouvel import Ghidra ; ne modifier ni compteurs, ni `VdSwap`, ni renderer.

## Reprise immédiate — provenance de l'image callback D3D

`done_when` : les manifests établissent directement si l'image du build
courant correspond à l'image PAL démo du PASS historique. Comparer uniquement
source, taille, projet et commande de génération. Ensuite exploiter ou relancer
le vérificateur une seule fois, sans sélection par empreinte.

## Reprise immédiate — producteur post-KickOff D3D

La provenance de l'image est fermée. Partir des cinq routes qualifiées et
fermer le slice :

`0x821BA780 KickOff → callback encodé → PM4_INTERRUPT → 0x821B9710 → effet`

`done_when` : le dernier effet producteur est identifié avec son objet ou
adresse, ses champs lus/écrits et son consommateur ; ou la chaîne démontre
qu'elle ne publie aucun travail post-bootstrap. Ne pas lancer de runtime avant
ce résultat statique.

## Après rotation — jointure curseur ring vers WPTR

La chaîne callback est fermée négativement. Reprendre à partir de
`0x821C57D0`, qui fait avancer la file construite, et retrouver le chemin de
données attendu vers `0x821B9BC8` ou un autre writer WPTR.

`done_when` : un champ précis du device relie le curseur produit au writer, et
la condition qui supprime sa publication post-bootstrap est expliquée sans
compteur de performance ; ou tous les consommateurs statiques de ce champ sont
épuisés et un nouveau test discriminant est nommé.

## Gate actif — producteur du payload de file guest

La jointure ring/WPTR est réfutée. Rechercher les writers des slots de 96 octets
autour de `0x82386DD0` et `0x82386D90`, puis remonter leurs valeurs vers les
appelants de `0x820FF710`.

`done_when` : le champ source et le callsite qui devraient fournir un payload
non nul sont identifiés, avec la condition exacte qui laisse le slot nul ; ou
tous les writers statiques sont épuisés et un test synthétique discriminant est
défini. Ne pas synthétiser un payload ni lancer de runtime avant cette preuve.

## Reprise du gate limité — slice P-code du champ `slot+64`

Réutiliser les scripts Ghidra existants pour recenser les stores dont l'adresse
se ramène à `this+0x110 + index*0x60 + 0x40`. Pour chaque writer, émettre
fonction, PC, expression de valeur et xrefs entrantes.

`done_when` : `0x820FF710` est démontré unique et son zéro classé intentionnel,
ou un autre writer non nul et sa condition d'atteinte sont identifiés.

## Reprise immédiate — normalisation P-code des adresses de slot

Le scan D-form est réfuté. Réutiliser le décompilateur Ghidra et ajouter
seulement une passe bornée qui normalise les entrées d'adresse des `STORE`
(`INT_ADD`, `PTRADD`, `PTRSUB`, `INT_MULT`). Filtrer le stride `0x60` et le
sélecteur `slot+0x40`, puis joindre les fonctions candidates à leurs références
entrantes.

`done_when` : la passe retrouve d'abord le témoin `0x820FF710`, puis démontre
qu'il est unique ou identifie un writer non nul et sa condition d'atteinte.

## Reprise processus long — passe P-code

Ajouter seulement `flush` après `CALIBRATION` et un compteur périodique dans
`FindScaledStoreWriters.java`. Relancer Ghidra avec une session longue et
attendre la ligne `SUMMARY`.

`done_when` : calibration persistée et scan global terminé, ou point exact
d'interruption persisté permettant de réduire statiquement la plage.

## Gate actif — atteignabilité du producteur types 1/4

Partir de `0x82117410`, dont les appels aux slots `+0x10` et `+0x1C` publient
sans garde locale les payloads types `1` et `4`. Recenser ses références
entrantes, décompiler uniquement ses appelants, puis joindre la première garde
à un état guest précis.

`done_when` : l'appelant et la condition qui empêchent `0x82117410` de
s'exécuter sont identifiés, avec producteur et consommateur du champ testé ;
ou son exécution est statiquement inconditionnelle et une observation bornée
est définie pour départager la contradiction.

## Reprise du gate limité — contrôle de `0x8210ADB4`

Exporter le CFG de `0x8210A1C0` et calculer les dépendances de contrôle du bloc
contenant `0x8210ADB4` par post-dominateurs. Ne conserver que les branchements
qui contrôlent ce bloc, leurs conditions P-code et les champs mémoire lus.

`done_when` : une garde précise et son champ guest sont identifiés, ou le bloc
est démontré inconditionnel sur la sortie pertinente et un breakpoint unique
est défini.

## Reprise immédiate — paramètres de `0x8210A1C0`

La décompilation de `0x82165CC0` existe déjà dans
`artifacts/render-queue-callsite-control-slice-gate/helper-callers-decompile.log`.
Extraire le contexte de `0x82165D8C`, les six arguments transmis et leurs
champs source. Calculer la dépendance de contrôle du callsite seulement si le C
ne suffit pas.

`done_when` : les paramètres sont joints au dispatch et aux cinq lookups de
`0x8210A1C0`, avec la première valeur susceptible d'éviter `0x8210ADB4`.

## Prochain gate — render-queue-type1-payload-layout

`done_when`: cinq offsets de clés, bornes associées et booléens passés à `0x82117410` sont reliés exactement à `param5/param6`; premier producteur du champ bloquant est identifié statiquement. Aucun runtime, aucun shim avant preuve.

## Prochain gate — resource-10c-producer-slice

`done_when`: expression RHS écrite par `0x8231BCB0` à `record+0x10C`, conditions de zéro/non-zéro, source du record et appelant atteignable sont qualifiés statiquement; un contrat natif est proposé seulement si ce slice prouve une divergence d’implémentation.

## Prochain gate — record-10c-store-enumeration

`done_when`: tous stores PPC exacts de déplacement `0x010C` sont inventoriés sur plage mappée canonique, bases incompatibles éliminées, et au moins un store dont base alias un record de `DAT_826F6124` est prouvé — ou absence prouvée. Réutiliser launcher Ghidra qualifié existant ou scan statique XEX déjà disponible; aucun runtime.

## Prochain gate — resource-record-alias

`done_when`: retour de `0x8210D8A0` est relié ou disjoint, par arithmétique de base/table, aux records consommés dans `0x8210A1C0` via `DAT_826F6124`; contrat `0x8210D950` est alors accepté ou rejeté comme producteur. Décompiler aussi `0x821080D0` seulement si nécessaire. Aucun runtime.

## Prochain gate — native-resource-registration-contract

Reprise aussi limitée après cinq batches. Couverture des quatre racines est prouvée. Prochain test unique: afficher clés non sensibles d’une entrée parmi les 238 imports, construire mapping selon schéma réel, puis réutiliser `callgraph-edges.json` pour trouver chemin minimal vers premier import. `done_when`: premier import, allocation ou service natif réellement manquant est identifié. Ne pas modifier sortie générée et ne pas lancer runtime avant divergence statique.

## Prochain gate — resource-indirect-dispatch

La jointure qualifiée réfute tout import natif direct dans les quatre closures.
`done_when`: le premier appel indirect atteignable depuis `0x82108918` ou
`0x8210DD70` est relié à son objet, son slot et un ensemble borné de cibles;
une cible absente du codegen ou du runtime est identifiée, ou la famille est
réfutée. Réutiliser désassemblages et catalogues existants; aucun runtime.

Gate limité après cinq batches. Reprendre par décompilation Ghidra minimale de
`0x821075A0`; slicer uniquement les `bctrl` à `0x821075E0`, `0x821075F8` et
`0x82107610`. `done_when`: registre cible, objet, offset de slot et ensemble
borné de cibles sont prouvés puis comparés au manifeste codegen.

Slice partiellement fermé après cinq batches: objet `PTR_PTR_82386C10`, slots 1
et 13. Nouveau `done_when`: tous producteurs de `0x82386C10` sont inventoriés,
les vtables possibles sont bornées, leurs entrées `+0x04/+0x34` sont lues, et
chaque cible est classée présente ou absente du manifeste codegen. Aucun runtime.

Gate limité après cinq batches. Nouveau test unique: lire dans Ghidra les mots
big-endian `0x82386C10`, `object+0`, `vtable+0x04` et `vtable+0x34`, avec garde
de mapping à chaque étape. `done_when`: deux cibles exactes sont comparées au
manifeste, ou la valeur statique zéro impose explicitement un slice d'init.

Gate fermé: objet `0x82386C0C`, vtable `0x82008E50`, cibles slots 1/13
`0x820FEED8` et `0x820FEF70`, toutes deux présentes au codegen. Ne plus traiter
`0x821075A0` comme contrat manquant. Prochain gate: sélectionner le premier
autre site indirect atteignable dont cible ou vtable n'est pas déjà bornée.

Gate suivant limité: lire sans relance
`artifacts/resource-next-indirect-family-gate/slot20-producer.txt`. Le receiver
de `0x821154C0` est le retour de `0x82220670`; slots 20/21 à qualifier.
`done_when`: ensemble de vtables borné et deux cibles classées dans le manifeste.
# Gate suivant — ABI PPC du lookup et types des slots 20/21

`done_when` : les registres Xenon du conteneur et de l'indice à chaque appel de
`0x82220670` sont prouvés, puis chaque type d'entrée/alias atteignable est relié
à une vtable dont les slots 20/21 sont lus et comparés au manifest codegen.

Relancer `Ac6XenonWords.java` sur le projet canonique avec
`8210A1C0 82220670 821154C0 82115530`, sans préfixe `0x`. Aucun runtime ni shim.
# Gate suivant — producteurs des types du lookup slots 20/21

`done_when` : tous les producteurs statiques des entrées du tableau consulté
par `0x82220670` et des aliases `entrée+0x18C` sont reliés à un ensemble fini
de vtables; leurs slots `+0x50/+0x54` sont lus et classés dans le manifest
codegen.

Partir de l'ABI qualifiée dans
`artifacts/resource-slot20-ppc-abi-gate/ppc-functions.txt`. Chercher les stores,
constructeurs et setters avant toute nouvelle décompilation large. Aucun
runtime ni shim.
# Gate suivant — xrefs du setter d'alias et constructeurs des valeurs

`done_when` : chaque appelant direct de `0x82220550` est qualifié, ses arguments
sont reliés au conteneur `+0x308`, et les objets écrits dans `+0x18C` sont
reliés à un ensemble fini de constructeurs/vtables.

Utiliser les xrefs Ghidra directs du projet démo canonique, puis décompiler
seulement les appelants. En parallèle statique, remonter les producteurs de
`DAT_826F6124 record+0x10C` et des éléments retournés par `0x821E1D80`. Aucun
runtime ni shim.
# Gate suivant — arguments du sélecteur d'alias et tables de types

`done_when` : les registres `r3/r4/r5` fournis à `0x821E1D80` depuis
`0x82220550` sont prouvés, chaque table indexée est reliée à ses producteurs,
et l'ensemble de vtables retournables est fini.

Exporter le PPC complet de `0x82220550`, `0x821E1D80` et `0x82220750` depuis
le projet canonique. Slicer seulement l'appel au sélecteur et les écritures
`+0x18C`. Aucun runtime ni shim.

### Prochaine branche : accès indexés à la table `+0x29698`

Gate : isoler, parmi les fonctions référençant `PTR_DAT_823C27E0`, les séquences
exactes qui construisent `0x29698` et l'utilisent dans `lwzx/stwx` avec le
pointeur global chargé. `done_when` : tous les sites sont classés lecture ou
écriture et le premier producteur de la table est relié à ses valeurs. Aucun
runtime ni shim.

### Prochaine branche : jointure base globale / accès `+0x29698`

Gate : propager localement l'alias issu du chargement de `0x823C27E0` dans les
fonctions candidates et filtrer les `lwzx/stwx` dont l'autre opérande est
`0x29698`. `done_when` : tous les sites qualifiés sont classés et chaque `stwx`
est relié à la valeur écrite. Aucun runtime ni shim.

### Prochaine branche : propagation CFG minimale du global

Gate : propager par basic blocks trois états GPR seulement — inconnu, constante
haute `0x823C0000`, base chargée via `0x27E0` — et classer les 120 accès exacts
à `+0x29698`. `done_when` : chaque site possède une provenance ou une réfutation
CFG, et chaque `stwx` qualifié est relié à sa valeur. Aucun runtime ni shim.

### Prochaine branche : relance du classifieur SSA

Corriger le parseur des adresses fonction sans préfixe dans
`scripts/ClassifyGlobalIndexedAccesses.java`, puis relancer les 114 fonctions.
`done_when` : calibration complète, aucun échec de décompilation et inventaire
LOAD/STORE exploitable. Aucun runtime ni shim.

### Prochaine branche : allowlist des 120 PC dans le SSA

Faire recevoir à `ClassifyGlobalIndexedAccesses.java` les PC `use=` exacts,
dédupliquer leurs fonctions, et filtrer chaque opération par `Seqnum` avant la
recherche d'expression. `done_when` : 120 PC classés sans ajout, calibrations
inchangées et valeurs de tous les stores qualifiés. Aucun runtime ni shim.
## Prochain gate — orphelin PPC `0x82210188`

Dump statique borné autour de `0x82210188` avec `DumpRange.java`, puis slice
local de la base du `lwzx`.

`done_when`: le PC est classé qualifié ou rejeté par une chaîne PPC explicite;
si la définition de base sort de la fenêtre, consigner l'indécidabilité. Aucun
runtime, Wine ou shim.
## Après fermeture de l'orphelin `0x82210188`

Revenir à la chaîne du receiver de `0x821154C0/0x82115530`: utiliser les 118
consommateurs qualifiés de `*(global+0x29698)` et l'unique writer
`0x82212E54` pour relier les objets retournés par `0x821E1D80` à leurs
constructeurs/vtables, puis résoudre les slots `+0x50/+0x54`. Aucun runtime,
Wine ou shim.
## Prochain gate — stores de `object+0x2E4/+0x2E8`

Recenser les stores PPC exacts aux déplacements `0x2E4` et `0x2E8` avec les
scripts existants, puis remonter la valeur de chaque store vers un
constructeur, une table statique ou une entrée de fonction.

`done_when`: tous les stores directs sont inventoriés et chaque famille de
valeurs est classée; au moins une vtable candidate fournit les slots
`+0x50/+0x54`, ou une indécidabilité statique précise est établie. Aucun
runtime, Wine ou shim.
## Prochain gate — producteur de `piVar16` dans `0x820A4F58`

Extraire du log existant toutes les définitions de `piVar16`, choisir celle
qui domine l'écriture du tableau à `entry+0xD8`, puis résoudre sa factory ou
son constructeur jusqu'à la vtable.

`done_when`: la vtable de chaque famille d'objets pouvant entrer dans le
tableau est qualifiée et ses slots `+0x50/+0x54` sont des adresses de code
précises, ou une source statique exacte reste indécidable. Aucun runtime, Wine
ou shim.
## Prochain gate — vtable `0x82000B94`, slot `+0x14`

Lire la table statique à `0x82000B94`, résoudre l'entrée à `0x82000BA8`, puis
décompiler seulement cette factory. Inventorier les vtables des objets
retournés pour les kinds `0..4` et lire leurs entrées `+0x50/+0x54`.

`done_when`: chaque kind atteignable possède une cible factory et une vtable
receiver qualifiées, avec slots 20/21 précis et présence codegen vérifiée.
Aucun runtime, Wine ou shim.
## Prochain gate — couverture codegen `0x8220E428/0x82211040`

Déterminer dans la configuration et les sorties structurées existantes
pourquoi les deux starts qualifiés sont absents de l'arbre natif. Réutiliser
les outils documentés; ne pas modifier une sortie générée.

`done_when`: les deux fonctions sont soit incluses dans la génération et
couvertes par un test sémantique focalisé, soit un point d'intégration natif
plus amont est prouvé nécessaire. Aucun runtime, Wine, shim ou optimisation.
## Reprise exacte — manifeste structuré XenonRecomp

Interroger le manifeste de fonctions déjà généré, ou produire seulement ce
manifeste via `tools/build_demo.py` s'il manque, pour les starts numériques
`0x8220E428` et `0x82211040`. Ne pas conclure depuis les seuls noms de fichiers
C++ et ne pas écrire de substitut manuel avant ce contrôle.

`done_when`: présence/absence de chacun des deux starts établie dans la liste
effective passée au générateur, avec le callsite indirect correspondant.
## Prochain gate — première frontière réellement fautive

La piste slots 20/21 est close et entièrement générée. Reprendre depuis le
handoff courant et ses observables guest les plus récents pour identifier le
premier PC/target/import qui arrête effectivement la progression. Qualifier
d'abord sa couverture statique et son contrat existant.

`done_when`: une frontière précise est démontrée absente ou incorrecte par un
producteur/consommateur statique et un observable guest déjà capturé; sinon
l'hypothèse candidate est explicitement réfutée. Pas de runtime nouveau tant
que les captures et produits existants suffisent.
## Reprise exacte — divergence post-`0x82327154`

Aligner les deux traces cycle 1761 sur leur unique load64 à `0x82327154`, puis
extraire la première divergence persistante suivante. La classer guest store,
entrée PM4/draw, différence hôte, ou hors couverture; slicer ensuite son seul
producteur jusqu'au consommateur draw/readback.

`done_when`: un tuple précis `(tick, thread, PC, adresse/registre, valeurs A/B)`
est relié statiquement à un consommateur draw/readback, ou les traces retenues
sont prouvées insuffisantes. Aucun nouveau runtime avant ce verdict.
## Prochain gate — fenêtre d'observation START minimale

Les traces cycle 1761 sont prouvées insuffisantes: elles ne divergent que sur
l'entrée contrôlée. Relire les slices statiques déjà qualifiés des cycles
1630/1633/1634/1635 pour sélectionner le premier consommateur START et son
premier store guest potentiel, puis définir une fenêtre bornée autour de ce
PC avant tout runtime.

`done_when`: un ensemble minimal de PC/adresses et une hypothèse dynamique
avec critères confirmer/réfuter/indécidable sont fixés depuis les preuves
statiques existantes. Aucun lancement avant cette fermeture.
## Prochain gate — slice des PC divergents tick268

Dans le projet Ghidra canonique, décompiler et joindre les callers de
`0x820CDC20`, `0x823255F0` et `0x82325644`. Résoudre leurs objets/vtables et
chercher le premier consommateur des champs écrits vers task enqueue,
draw/PM4 ou readback.

`done_when`: la divergence est classée construction/sélection de tâche,
allocation/cleanup, rendu/readback, ou reste statiquement indécidable après
résolution exhaustive des dispatchs. Aucun nouveau runtime avant ce verdict.
## Prochain gate — premier store START persistant hors pile

Le slice tick268 a réfuté les trois deltas candidats: le rapport dérivé avait
renommé le LR en `pc`, et les écritures observées appartiennent aux cadres de
pile. À partir des producteurs/consommateurs START déjà qualifiés, sélectionner
statiquement le premier store hors pile susceptible de survivre au tick 253.
Ne lancer aucun runtime avant d'avoir fixé son adresse, son producteur et son
consommateur attendu.

`done_when`: un store persistant hors pile est relié à une branche, un enqueue
de tâche, un draw/PM4 ou un readback; ou les captures bornées existantes sont
prouvées identiques après exclusion qualifiée de toutes les piles de threads.
## Prochain gate — lecteurs dérivés du bit logique START

La capture tick268 est hors plage pour les globals START et ne peut fermer la
question. Dans le projet Ghidra canonique, résoudre les xrefs directs, TOC et
valeurs propagées de `0x82798488` depuis `0x821DE6F8`. Pour chaque lecteur,
slicer la branche dépendante jusqu'au premier store hors des trois relais
transitoires.

`done_when`: un lecteur est joint à un store durable et à un consommateur de
progression, ou tous les lecteurs sont exhaustivement classés sans effet.
Aucun runtime avant ce verdict.
## Prochain gate — factory et enqueue des consommateurs START

Les lecteurs sont résolus et le premier effet durable existe, mais leurs
fonctions propriétaires ne sont jamais atteintes. Résoudre les vtables,
constructeurs/factories et callsites d'enqueue des propriétaires
`0x82170F58` et `0x82185198`, puis joindre leur publication au dispatcher
`0x82259D10`.

`done_when`: un callsite précis de construction/publication/enqueue est
relié au dispatcher et son prédicat bloquant est identifié, ou les deux tâches
sont prouvées hors phase. Aucun runtime avant ce verdict.
## Prochain gate — contrat de retrait de CTaskLoading

Les consommateurs START sont hors phase tant que la liste bootstrap reste
active. Slicer le retour du slot 4 `0x8218CE20` dans le dispatcher
`0x82259D10`, puis les chemins de retrait/publication
`0x82259E18/0x82259FF8`. Relier l'état interne 1 de `0x8217E3E0` à ce
contrat externe.

`done_when`: le code de retour ou prédicat exact qui retire/remplace
`CTaskLoading` est relié à son producteur, ou son maintien est prouvé
conforme et la prochaine tâche productrice est identifiée. Aucun runtime avant
ce verdict.
## Prochain gate — CFG dispatcher depuis le listing déjà capturé

Ne pas relancer Ghidra. Extraire de
`artifacts/loading-task-retirement-contract-gate/ghidra-function-listings.txt`
les fenêtres `0x8218CE20`, `0x82259D10`, `0x82259E18` et
`0x82259FF8`. Reconstruire le CFG entre retour du slot 4, branche du
dispatcher et mutation de liste.

`done_when`: la valeur de retour est prouvée consommée ou ignorée et le
prédicat de retrait/remplacement est relié à son producteur, ou les fonctions
de liste sont prouvées sans rôle de retrait. Aucun runtime.
## Prochain gate — writers externes de CTaskLoading+0x0C

Le retour du slot 4 est ignoré et aucune mutation automatique ne retire la
tâche. Résoudre tous les stores vers `this+0x0C` pour l'objet
`CTaskLoading`, leurs callers et leurs callbacks, en séparant les écritures
internes états 4/5 des producteurs asynchrones.

`done_when`: un writer capable de faire quitter l'état 1 est relié à son
événement/message/service, ou l'état 1 est prouvé terminal et le producteur
réel de transition de mode est identifié ailleurs. Aucun runtime.

## Next: CTaskLoading field-transition contracts

Gate unique : reconstruire les fonctions `0x8218CBD8`, `0x8218CCD0` et
`0x8218CD78`, qualifier tous leurs stores vers `this+0x09/+0x0A/+0x0C`, puis
remonter leurs appelants directs (`0x8217E458`, `0x82192D64`, `0x8218CE2C`).

`done_when` : chaque valeur écrite à `CTaskLoading+0x0C` est reliée à sa
condition et à son producteur, et le premier contrat manquant de la route native
est identifié ou réfuté. Rester statique ; aucun shim env ni runtime.

## Next: CTaskLoading async-status provider

Gate unique : qualifier statiquement le lookup `0x8219EE40` et le poll
`0x8219F5D0`, puis vérifier leur couverture dans les services natifs existants.

`done_when` : les retours zéro/positif/négatif de `0x8219F5D0` sont reliés à un
producteur concret, et l'implémentation native correspondante est prouvée
présente ou absente. Ne pas utiliser de shim env ni de runtime.

## Next: native loading-contract integration

Gate unique : suivre le startup de `ac6-native` jusqu'au point partagé de
soumission/consommation des ressources de frontend. Qualifier si ce chemin doit
porter un job ternaire conforme au guest ou s'il contourne entièrement
`CTaskLoading`.

`done_when` : un point d'intégration natif unique et le job qui doit progresser
de pending vers done/error sont qualifiés, ou le provider guest est prouvé hors
du chemin exécuté et le verrou réel est relocalisé. Aucun shim env ni runtime.

## Next gate: visible native frontend contract

Question: what is the smallest statically evidenced native contract that can retain one pre-mission frontend state and feed a visible render/present consumer?

Done when: identify the exact state, assets, input transition, and existing render boundary needed for one visible frontend frame, or prove that a required frontend visual producer is absent and name that producer precisely.

Constraints: static evidence first; no shim, Wine, Xenia, runtime trace, optimization, or speculative asynchronous queue.

## Next gate: PAL NFH glyph producer contract

Question: what exact binary-derived NFH leaf layout and decode contract turns one qualified PAL frontend font pack into a glyph atlas plus metrics suitable for a native Title draw?

Done when: identify the qualified FHM leaf selection, NFH header/record fields, texture encoding and glyph metric mapping for one Latin locale, and validate them with an isolated parser fixture; or name the first unresolved binary field and its discriminating static test.

Constraints: reuse existing reports, extraction tools and bounded cache payloads; static binary/data analysis before harness; no placeholder glyphs, runtime oracle, shim, Wine, or full program execution.

## Next gate: canonical PPC NFH header consumer slice

Question: what do the demo PPC consumers prove about `NFH+4` and `NFH+8`?

Done when: recover the qualified function boundaries and dataflow from NFH magic recognition through both field loads, classify each field's role, and identify record stride plus first downstream consumer; or prove the path ends at a specific unresolved indirect dispatch and name its object/vtable slot.

Use the canonical demo Ghidra project and existing scalar/xref/slicing scripts. Static only. Do not repair the cache scan, write a parser, infer from field values, or use runtime until this consumer slice closes.

## Blocked branch resumption: decompile the unique NFH recognizer

Read the declared CLI contract in `scripts/DecompileAt.java`, then decompile `FUN_822E2858` at `0x822E2858` with the correct argument order and export its callers. This is the only next test. Do not rerun discovery, cache scans, or runtime probes.

Done when remains unchanged: classify the loads from NFH offsets 4 and 8 through their first downstream consumer, or name the exact unresolved indirect dispatch.

## Next gate: NFH view vtable identity

Question: which vtable contains the entry at `0x82027B0C`, and what object/slot does `Function_822CC9F0` implement?

Done when: qualify vtable start, RTTI/class identity when present, slot index, assigning constructor/factory, and the first dispatch that consumes the view data at object offset `0x10`; or name the exact missing static edge.

Use the already collected vtable tooling intake. Do not repeat NFH magic discovery or the two completed decompilations.

## Next gate: NFH view record layout

Question: which fields of the `0x20`-byte record returned by `0x822CC378` are consumed by the next native PPC code, and what minimal record contract is justified?

Done when: qualify the first downstream reader(s), field offsets, signedness/stride and producer relationship for one record; or prove the first unresolved indirect dispatch and name its exact slot.

Use the canonical demo Ghidra project and bounded static slicing only. Do not write a parser, add placeholder glyphs, invoke the runtime, or infer semantics from raw payload values before the PPC consumers are qualified.

## Gate limited: NFH receiver provenance

The numeric `+0xB0` scan is insufficient: the slot is shared by unrelated
classes. Resume only by following constructor callers
`0x822CD7C8`, `0x822386F0`, `0x82238870`, `0x822CD818`, `0x82237EB0` and
`0x822CD2D8`, then intersecting their receiver stores with the 24 candidate
dispatches. See
`artifacts/nfh-view-record-layout-gate/BLOCKER.md`.

## Après la borne statique IB — attribution du store généré

Le code généré et le ring natif ne fournissent pas de producteur statiquement
adressable pour la réservation bootstrap. Le prochain test discriminant est
donc un hook borné sur les stores générés, d'abord validé par le harness
synthétique puis appliqué à un seul tick zéro. Réutiliser les critères
confirmé/réfuté/indécidable de
`artifacts/ib-producer-static-resume-gate/BLOCKER.md`; ne pas répéter le
watcher générique `GuestMemory`.

## Record queue writer gate — closed; next boundary is type reachability

The normalized P-code scan closed the writer-discovery question. The record
queue has type-0 through type-4 producers, and `0x820FFCA0 → 0x820FEFA8`
consumes the nonzero variants. The next gate is to connect the actually
reached type to renderer/scheduler publication.

`done_when`: static callsite guards or one predeclared bounded trace establish
whether a type 1–4 record is produced and which publication/wait contract it
reaches. Confirm with a nonzero type and downstream progress; refute with only
type 0 and no payload branch; leave indeterminate if the computed dispatch
cannot be resolved. Do not use a global trace or an oracle before this slice.

## Next gate: writer → publication → renderer/scheduler

The one-tick capture now closes writer attribution: `0x821B20A0` emits the
48-dword PointList payload at `0x16AE0980`. Resume with a static slice from
that writer through the indirect-buffer publication and the first renderer or
scheduler consumer. Done when the qualified payload is either connected to a
non-black/readback effect or the exact downstream wait/consumer contract is
identified. No new generic watcher, Wine, or Xenia run is justified before
that slice.

## Next gate: callsite → type → publication

Le corridor `0x8210A1C0 → 0x82117410` est maintenant qualifié statiquement,
mais son activation native et la publication d'un record non nul restent
ouvertes. Utiliser une seule capture bornée, centrée sur `0x8210A1C0`, pour
relever `param_4`, les appels `0x821080D0`/`0x82117410`, puis le type `+0x110`
et le dispatch `0x820FEFA8`. `done_when` : un record type 1–4 est joint à une
publication/consommation renderer/scheduler, ou la capture réfute ce chemin
pour la fenêtre testée. Les critères confirmation/réfutation/indécidable sont
dans `artifacts/record-type-reachability-gate/BLOCKER.md`.

## Next gate: activation of the static record corridor

La capture corrigée montre `0x820FEFA8(r3=0x2EEEFE90)` 31 fois avec
`*(r3+0x40)=0`; `r4=1` n'est pas le sélecteur. Cette route worker est donc
réfutée comme publication type 1–4 dans la fenêtre, tandis que le corridor
statique `0x8210A1C0 → 0x82117410` n'est jamais activé. `done_when` : qualifier
statiquement le producteur de `param_4` et une fenêtre d'activation de ce
corridor, ou l'exclure par absence de provenance. Ne pas ajouter de shim env,
de contrat natif ou de trace globale.

## Next gate: source indirecte du huitième argument

Le producteur statique de `param_4` est fermé jusqu'à la frontière d'appel :
`r8` entrant dans `0x82165CC0` devient `r6` de `0x8210A1C0`, puis son bas
16 bits. Le seul appelant direct est donc indirect/vtable et aucune fenêtre
START ne l'active avec un selector du corridor. `done_when` : qualifier la
table/slot qui fournit `0x82165CC0` et une valeur `0x1102..0x1210`, ou montrer
statiquement que cette valeur est absente du chemin démo. Aucun nouveau
runtime, shim env ou contrat natif avant cette jointure.

## Closed gate: bounded AVIObjectDemo receiver witness

La table/slot est maintenant qualifiée : `0x8200B6FC[+4] = 0x82165CC0`,
écrite à `objet+0xC` par deux constructeurs. La dispatch observée reste sur la
vtable primaire `0x8200B844`; l'alias ou récepteur ajusté qui activerait le
corridor n'est pas démontré. Le hook borné à `0x82165CC0` n'a vu aucune entrée
pendant START `2990..3021`; le résultat est indécidable pour l'activation
globale. Ne pas ajouter de shim env ni de contrat natif.

## Next gate: static source of the AVIObjectDemo virtual receiver

Le témoin borné ne voit aucune entrée dans `0x82165CC0`, `0x8210A1C0` ou
`0x82117410`; cette absence est indécidable pour l'activation globale et ne
doit pas être répétée dans la même fenêtre. `done_when` : identifier le site de
dispatch indirect, le récepteur ajusté et la provenance de `r8` qui peuvent
atteindre le slot `0x8200B6FC+4`, ou montrer statiquement que cette arête est
inatteignable dans le chemin démo. Aucun nouveau runtime, shim env ou contrat
natif avant cette qualification.

## Next gate: resolve the slot +4 vtable provenance

Le gate statique a borné l'arête entrante à dix sites locaux de dispatch
`+4`; l'unique onzième, `0x82165D6C`, est interne à `0x82165CC0`. `done_when` :
pour chaque candidat, déterminer statiquement le récepteur et la vtable, puis
confirmer `0x8200B6FC` ou exclure le site. Pour un site confirmé, conserver la
provenance de `r8` jusqu'à l'appel à `0x82165CC0`. Aucun runtime, shim env ou
contrat natif avant cette résolution.

## Next gate: qualify the ten remaining receiver expressions

La passe constructeurs ferme les flux connus : la vtable cible n'est créée que
par `0x82166550`/`0x821674A8` à `objet+0xC`, et `0x821710FC` reste sur
`0x8200B844`. Il reste à propager, pour les dix sites locaux, le receiver vers
une vtable concrète et, si elle est `0x8200B6FC`, la provenance de `r8`.
`done_when` : un candidat confirmé avec selector ou tous les candidats réfutés;
un receiver non récupérable reste indécidable. Aucun runtime, shim env ou
contrat natif avant cette jointure. Artefacts :
`artifacts/avi-virtual-edge-static-gate/constructor-flow-final.txt`.

## Resume after quota: decompile recovered callers

Le census a ajouté les callers directs des candidats, mais pas leur vtable.
Reprendre uniquement par les fonctions `0x82167378`, `0x8216EAC4`,
`0x8216EEC4`, `0x8216F920`, `0x821A4454`, `0x821A3634` et `0x821A4400`, puis
propager le receiver. Ne pas refaire START; le résultat `DumpRange` du dernier
batch est invalide à cause d'un argument `--batch` excédentaire.

## Next discriminant: remaining state/manager receivers

Le candidat `0x82169B7C` est réfuté par sa vtable `0x8200BAEC`, et les deux
annotations `bctrl` vers `0x821600C8` sont corrigées comme appels via
`PTR_PTR_82390034[+8]`. La prochaine passe doit qualifier uniquement les
receivers de `0x82166AE0`, `0x8216E218`, `0x8216F640` et `0x8216F7F8` (état,
manager ou sous-objet), sans refaire START ni ajouter de shim.

## Next discriminant: materialize manager/accessor returns

La passe ciblée a réduit les receivers à `FUN_82327104`, `FUN_82327108`,
`FUN_82327100`, `FUN_8232710C` et `PTR_DAT_8238FEF4`; leurs stubs Ghidra ne
révèlent pas les valeurs de retour. Qualifier statiquement leur matérialisation
ou leurs écritures de vtable. `done_when` : un receiver porte concrètement
`0x8200B6FC` (puis joindre `r8`/selector), ou chaque candidat est réfuté par
une autre vtable. Une valeur non récupérable reste indécidable. Aucun runtime,
shim env ou contrat natif avant cette jointure. Artefact compact :
`artifacts/avi-virtual-edge-static-gate/manager-factory-summary-run5.txt`.

## Next discriminant: propagate caller r3 after ABI-helper correction

Les adresses `0x82327100/04/08/0C` sont des entrées contiguës de sauvegarde
ABI, pas des getters. Reprendre les callers de `0x82165230`, `0x82166AE0`,
`0x8216E218`, `0x8216F640`, `0x8216F7F8` et `0x82170F58`, propager leur `r3`
vers une vtable concrète, puis comparer au slot `0x8200B6FC+4`. `done_when` :
un receiver confirmé AVI avec la source de `r8`, ou tous les candidats
réfutés/indécidables avec cause. Le global `0x82170CD0` doit être traité à part
via le producteur de `0x82731150`. Aucun runtime, shim env ou contrat natif
avant cette jointure. Voir
`artifacts/avi-virtual-edge-static-gate/helper-boundary-correction-run8.txt`.

## Next gate: first producer of the dominant guest wake-up

La piste AVI est maintenant bornée : les receivers d'état et la table
`0x8200C5C0..0x8200C664` ne fournissent aucune vtable qualifiée
`0x8200B6FC`; poursuivre ce dépliage n'a pas de preuve discriminante. Revenir
au plateau natif documenté, retrouver statiquement l'objet d'attente dominant
et son dernier producteur parmi événement/fence, callback XMA ou notification
XAM/xboxkrnl. `done_when` : un producteur précis et son contrat minimal sont
reliés au waiter, ou la piste est réfutée par absence de producteur dans la
slice. Ne pas relancer START ni ajouter un shim avant cette relation causale.

## Next discriminant: reconstruct the indirect vtable at `0x8200C624`

Le gate de propagation des callers est fermé : les receivers locaux sont
connus, mais les machines d'état n'ont pas de callers directs et
`0x8216F7F8` n'a qu'une référence de données à `0x8200C624`. Reconstruire
statiquement la table autour de cette adresse, qualifier son slot vers
`0x8216F7F8`, puis retrouver son constructeur/écriture; en parallèle,
qualifier `FUN_823270F8()` dans le chemin `0x82165490 → 0x82165230`.
`done_when` : vtable concrète comparée à `0x8200B6FC`, ou réfutation/indécidabilité
documentée pour chaque arête. Ne pas relancer START, ajouter de shim env, ni
implémenter un contrat natif avant cette preuve.

## Gate fermé : primary counter / wake contract

Le callback `0x822E3EC0`, le producteur `0x822EEE10`, les consommateurs
`0x822E4018/0x822E4080` et le seuil `state+0x10` du worker `0x822E40E8` sont
maintenant reliés statiquement au même état `0x82934708`. Le seuil est zéro au
démarrage; le flag d'arrêt n'est activé que par le chemin shutdown. Ce n'est
donc pas le producteur manquant du plateau natif.

Prochaine gate discriminante : retrouver le premier producteur du draw normal
ou du frontend visible depuis le dispatch post-`START`. `done_when` : un appel
de production et son buffer/commande consommateur sont qualifiés, ou la slice
statique réfute cette branche. Ne pas relancer START tant qu'un slice statique
plus discriminant reste disponible. Voir
`artifacts/primary-counter-static-gate/gate.status`.

## Next gate: decode one real PAL frontend leaf

Le frontend state machine et la présence FHM/NFH sont qualifiés, mais aucun
producteur de pixels/draw-list ne consomme `FrontendState` ou les ressources
frontend. Qualifier statiquement le layout d'une feuille NFH PAL et le contrat
de draw de l'état Title, puis écrire un fixture/parser natif borné. `done_when`:
un glyph/atlas réel est décodé et relié à une primitive de rendu native, ou le
format est réfuté/indécidable avec une cause précise. Aucun placeholder, texte
synthétique, shim env ou runtime global avant cette preuve.

## Gate fermé : CSwgListener `+0x20` du plateau post-START

Le sweep RTTI qualifie 89 dérivés et 40 handlers non-stub, mais le snapshot
actif `M102` ne contient que `CSelectMessageDlgManager` et
`CModeTaskTitleDemoOffline`, tous deux sur le stub `0x820AC748`. Cette branche
ne justifie donc ni service natif ni nouvelle capture runtime. Reprendre le
layout NFH PAL réel et le draw Title. Voir
`artifacts/listener-slot20-sweep-gate/RESULT.md`.

## Gate fermé : borne structurelle NFH PAL

La forme `NFH -> leaf+0x450 -> count × 0x20` est maintenant qualifiée sur les
leaves extraits. Le prochain bord n'est pas un parser hypothétique : il faut
relier statiquement le retour de `0x822CC378` à un consommateur réel de record
au-delà de l'indirection `0x82027A64/+0xB0`, puis seulement chercher l'atlas et
le draw Title. Voir `artifacts/nfh-layout-static-gate/RESULT.md`.
## Gate fermé : consommateur direct NFH `+0xB0`

Le déplacement `+0xB0` ne suffit pas à relier le record NFH à un draw. Le
prochain test statique doit partir d'un appel indirect distinct ou d'une arête
de données plus longue depuis `0x822CC378`; ne pas implémenter de parser ou de
shim avant cette provenance.
## Next gate: first real visual consumer of NFH records

The canonical PPC accessor is closed through indexed `0x20`-byte records.
Qualify the next statically reachable consumer that turns those records into
glyph metrics, an atlas lookup, a Title draw list, or a compositor command.
`done_when`: one such producer/consumer contract is proven, or this route is
refuted/indeterminate with a precise edge. Do not add an NFH parser, env shim,
or runtime capture before that proof.
## Next gate: child resource/view to NFH reader

The title task and `CResourceManager` bookkeeping are qualified and have been
refuted as the visual producer. Follow the resource child/vtable path that can
reach `0x822CC9F0` or `0x822CC378`, then qualify any glyph/atlas-to-draw
contract. `done_when`: a concrete NFH record consumer and its draw/compositor
buffer are proven, or the child path is statically refuted. No env shim, parser,
or runtime before this edge is closed.
## Gate fermé : child resource/view vers métriques NFH

Les wrappers child/view atteignent bien les getters NFH, mais la sortie reste
scalaire ou une copie de record. `0x82327D90` est un memcpy optimisé et
`0x821A00E8` un enqueue de ressource; aucun draw/compositor buffer n'est
atteint. La branche est donc réfutée comme producteur visuel.

Prochaine gate : trouver le premier consommateur title-side qui transforme ces
métriques en atlas, draw-list ou commande compositor, ou fermer cette piste si
le slice ne contient aucun tel buffer. Voir
`artifacts/child-resource-view-next/RESULT.md`.

## Next gate: invoke the qualified RT0 writer

The first non-bootstrap writer is now statically qualified through the vtable
method `0x822F84E0` and its color/vertex ABI into `0x821B55C0`. The next
discriminant is narrow: establish whether the native path invokes this slot
and whether one bounded call produces a non-black RT0 change. Capture only
the call arguments, return, and RT0/resolve state; do not trace the whole
program. `done_when`: a bounded native harness or runtime probe observes the
writer call and a changed RT0/resolve buffer, or refutes this slot as inactive.
No env shim, parser, or `-O3` before that result.

## Next gate: render-queue consumer before RT0 invocation

The RT0 writer is statically qualified, but the runtime never reaches it.
Close the upstream queue edge first: identify the native consumer/worker for
`0x82386CC0`, prove one bounded item transition from producer to consumer, and
observe a nonzero PM4 packet census. Capture only producer/consumer counters,
queue item type/address, child calls, and the resulting packet count.
`done_when`: consumer advances and at least one non-bootstrap packet is
submitted, or the queue path is statically refuted and a different consumer
is named. Do not add an env shim, NFH parser, direct writer call, or
optimization before this edge is closed.

## Next gate: nonzero render-record producer (supersedes consumer gate)

Le contrôle de la queue est maintenant fermé : `0x820FFCA0` consomme et
réinitialise les index, puis appelle `0x820FEFA8` avec un objet pile dont le
record est nul. Qualifier statiquement les appelants/vtables qui peuvent
atteindre `0x820FF788` ou `0x820FF7F8`, puis joindre cette production au champ
`record_type` lu par `0x820FEFA8`.

`done_when` : un record de type 1–4 non nul est observé et son appel enfant est
qualifié, ou la famille de writers est réfutée comme inactive pour la démo.
Ne pas ajouter de shim env, parser NFH, appel direct RT0 ni `-O3` avant ce
résultat.

## Next gate: alternate producer of `state+0x56F8`

Le sous-gate `0x822F85B8 -> 0x821BB4C8` est fermé : le chemin généré ne passe
que `r6=1`, initialise `0x0C000001`, et aucun writer généré ne pose le bit
`0x4` requis par `0x821C57D0`. `sub_821C64E8` n'est pas ce writer.

`done_when` : le projet Ghidra canonique confirme une construction/entrée
indirecte alternative avec un `r6` portant le bit `0x4`, ou réfute cette voie
et nomme le service/import manquant. Ne pas répéter la fenêtre START déjà
qualifiée et ne pas ajouter de shim env, appel direct ou optimisation avant ce
résultat.

## Checkpoint suivant — producteur du sélecteur de render record

Le corridor statique est désormais qualifié : `0x8210A1C0` peut appeler le
builder `0x82117410`, qui atteint les writers type 1–4 par la vtable
`0x82008EF0`. La capture native existante n'entre pas dans ce corridor et
`0x820FEFA8` ne voit que `record_type=0`.

Le prochain gate est donc le producteur/récepteur statique de `param_4` et de
ses données de table. `done_when` : origine et contrat du sélecteur qualifiés,
ou corridor réfuté pour le chemin démo. Ne pas répéter la capture ticks
2990–3021 et ne pas implémenter shim, parser, appel direct ou `-O3` avant ce
résultat.

## Active next gate — alternate producer of `state+0x56F8`

Le sous-gate `0x822F85B8 -> 0x821BB4C8` est fermé : le chemin généré passe
`r6=1`, initialise `0x0C000001`, et aucun writer généré ne pose le bit `0x4`.
Qualifier maintenant dans le Ghidra canonique une voie indirecte/importée
alternative; ne pas répéter la fenêtre START ni ajouter de shim ou optimisation.
Voir `artifacts/wake-producer-gate/RESULT.md`.

L'occurrence `0x821BB4C8` à `0x8207DF10` est désormais classée comme une
paire `.pdata` `BeginAddress`/métadonnée, pas comme un dispatch vivant
(`pdata_false_alternate_constructor`). Le prochain slice cible donc un
producteur importé ou omis hors de la construction qualifiée.

La résolution structurelle des deux computed calls est maintenant fermée par
les vtables RTTI : `0x822E5330` sélectionne le slot 3 de l'objet fabriqué et
`0x822F8590` le slot 6 de `0x8202A488`. Le prochain travail reste donc la
recherche d'une construction/import alternative, pas une nouvelle résolution
de ces deux sites.

## Active next gate — activation du CX360UnitManager

Le verrou renderer direct est qualifié : `0x820A45E0` (`CX360UnitManager` slot
`+0x14`) est l'unique producteur de `(17,6)`; `0x821ADAB8` arme
`device+0x5460`; `0x821C57D0` soumet seulement si ce champ est non nul. Aucun
appel direct au slot ou au callback n'est présent, et les sept sites de
construction connus de l'instance ne sont pas atteints dans le parcours
natif.

`done_when` : joindre statiquement un site de construction à la FSM mission
ou nommer le service/import manquant, puis observer dans un harness borné
`(17,6) → +0x5460 → soumission non-bootstrap`. Ne pas forcer le callback,
modifier le selector, ajouter un shim env ou utiliser `-O3` avant ce contrat.
Voir `artifacts/cx360-unit-manager-gate/RESULT.md`.

## Active next gate — provider de readiness du chargement

La construction `CX360UnitManager` est reclassée comme aval : `CModeTaskGame*`
enregistre `0x8217C4D8`, puis son bras `-3` crée les gestionnaires de mission.
La route native n'entre pas encore dans cette tâche; le chemin forcé s'arrête
dans `0x8219DF00`/`0x8219F5D0` pendant les polls `0x8219AF20` et `0x82195B50`.

`done_when` : relier le retour du provider à un producteur guest/natif précis,
ou réfuter cette attente comme cause du plateau. Capture runtime admissible,
si le statique ne suffit pas : objet actif, champs
`+0x0C/+0x14/+0x1C/+0x20/+0x24`, retour de `0x8219AF20`, atteinte de
`0x82195B50`, retour du slot `+0x14`. Pas de trace globale, shim env, callback
forcé ou `-O3` avant ce contrat. Preuve :
`artifacts/cx360-construction-activation-gate/RESULT.md`.

## Active next gate — transition post-provider et payload non-bootstrap

Le provider de readiness est désormais atteint : l'objet `0x2E3B0040` passe en
état `1`, `0x8219AF20` renvoie `1` au tick `4123`, et la branche `0x82195B50`
reste inactive. Le plateau persiste (`5492 PRESENT`, `23 blocked`, aucun
frontend/mission), donc le prochain test doit qualifier ce qui consomme la
fin du chargement puis le premier record de rendu non-bootstrap.

`done_when` : identifier la transition/consommateur post-provider et son
producteur de payload, ou réfuter ce lien par une fenêtre bornée. Garder les
milestones frontend/mission ouvertes; ne pas ajouter shim env, appel direct,
parser NFH ou `-O3`. Preuve : `artifacts/readiness-provider-gate/RESULT.md`.

## Gate fermé — agrégateur de readiness

La capture directe du résolveur et la trace bornée ont identifié le manager
`0x18BB0100`, drainé `+0x20`, puis remis `+0x24` à zéro au tick 4123. Le gate
« agrégateur inconnu ou non terminé » est fermé; la cause reste en aval.

## Gate suivant — consommation aval du readiness

Le manager `0x18BB0100` est maintenant identifié et drainé : `+0x20` devient
`0`, `+0x24` passe `1→0` au tick 4123. Pourtant aucun thread ne reprend et le
plateau reste `4140` ticks, `23 blocked`, `frontend=false`, `mission=false`.
Rechercher statiquement puis qualifier le consommateur du statut (publication,
callback ou FSM) et le premier état guest modifié après le store `0x18BB0124`.
Ne pas ajouter shim env, appel direct renderer, parser NFH ou `-O3`.

## Gate fermé — transition task/readiness vers listener

La fenêtre bornée confirme le slot virtuel 15 `0x8216CB40`, le store
`task+0x0C=1` et l'inscription listener `0x2E3D00E8`. Le producteur et le
consommateur immédiat du readiness ne sont donc plus le verrou principal.

## Active next gate — reprise de l'attente primaire

Qualifier, par statique puis trace bornée, le consommateur du listener et la
reprise autour de `0xE000004C` (post-wake `0x821A8C88`, wait LR `0x821A69CC`),
puis le premier payload de rendu non-bootstrap.

`done_when` : observer une reprise de thread avec changement de PC/état et un
record de soumission non-bootstrap, ou réfuter causalement cette chaîne. Ne pas
ajouter shim env, appel direct renderer, parser NFH ou `-O3`.

## Active next gate — producteur du premier record utile

Suivre statiquement la provenance de `param_4` et du récepteur virtuel qui
sélectionnent `0x8210A1C0 → 0x82117410 → 0x820FF788`, puis joindre le premier
record type 1–4 à `0x820FEFA8`.

`done_when` : un producteur guest réel et son état d’activation sont qualifiés,
ou un contrat partagé manquant est identifié au point exact. Aucun shim env,
appel forcé, parser NFH, état synthétique, A/B ou optimisation.

## Reprise après blocker — owner AVI puis première émission pixel

Le gate r8 est documenté dans
`artifacts/first-useful-record-r8-gate/`. Le dispatch `0x821710FC` est une
fausse piste (vtable manager); il faut qualifier statiquement l’owner/dispatcher
qui installe `0x8200B6FC` et définit `r8` avant `0x82165CC0`. Ensuite, suivre
les branches de `0x820FEFA8` jusqu’au premier `IM_LOAD_IMMEDIATE` pixel puis
`DRAW_INDX_2`.

Le probe borné a confirmé la frontière d’exécution (PRESENT sans frontend et
ring vide) mais ne doit pas être répété sans une entrée PC/r8 explicitement
instrumentée.

## Suite après l'exécution du seam causal Edge

La sonde Edge PPC/PM4 est désormais disponible mais n'a observé aucun des
quatre PC nommés dans sa fenêtre bornée; le natif est pareillement sans ring
ou PM4. Reprendre statiquement le producteur de `r8`, la publication AVI et le
premier record type 1–4. Une nouvelle trace runtime n'est permise que pour une
ambiguïté causale précise avec entrée, fenêtre et observables nommés; aucune
modification renderer ou optimisation avant l'arrivée mission.

## Active next gate — owner AVI et provenance de r8

Le writer provider est fermé statiquement (`0x82165490 → 0x82114F58 →
0x82114798`); ne pas le patcher. Qualifier maintenant l'instance portant la
vtable AVI `0x8200B6FC` et le producteur de `r8` avant `0x82165CC0`, puis
relier la sélection basse 16 bits à `0x8210A1C0`. Si ce bord reste indécidable,
une trace doit être strictement bornée à cet appel indirect et à ses gardes.
Pas de renderer, shim, appel forcé ou optimisation avant fermeture de ce
contrat.

## Validation effectuée — aucune levée du blocker guest

Les builds codegen-OFF/ON, les tests ciblés et les runners autonomes
writeback/EDRAM/screencap sont passés; l'installation est propre (`bin/bin`
absent, package sans payload de jeu ni C++ généré). La prochaine action reste
le slice statique owner AVI/r8 autour de `0x8200B6FC/+0xC → 0x82165CC0`.
Ne pas convertir cette validation en patch renderer : le runtime n'a toujours
pas produit de record type 1–4 ni de PM4 non-bootstrap.

## Active next gate — producteur de la liste amont

Relier statiquement `state+0xB5B4/B5B8` à la liste/descripteur remis à
`0x82114EA8/0x82114F58`, puis vérifier les callbacks de records encore
indirects. `done_when` : producteur et état d'activation qualifiés, ou contrat
partagé manquant précisément localisé. Aucun runtime sauf trace bornée à cette
ambiguïté, et aucun renderer/shim/optimisation.

## Active next gate — configuration et publication de l'owner secondaire

Le writer `0x82165E68` est maintenant qualifié. Suivre statiquement la source
de `param_2+0x12/+0x16` et la publication de l'instance avec vtable
`0x8200B6FC`, puis joindre son `r8` au sélecteur `0x8210A1C0`. Une trace n'est
permise que si cette arête reste indécidable après les artefacts existants;
aucun patch renderer, sélecteur forcé ou état synthétique.

## Active next gate — valeurs de configuration et table de callbacks

Les adresses et layouts amont sont maintenant qualifiés, mais les valeurs
concrètes de `param_2+0x12/+0x16`, l'instance `object+0xC` portant
`0x8200B6FC`, et les cibles des callbacks `[entry+0x20]`/`[entry+0x1c]`
restent indirectes. Épuiser d'abord les appelants/initialiseurs statiques;
une trace éventuelle devra rester bornée à cette table et à
`0x82165CC0`/`r8`, avec observables et `done_when` nommés. Ne pas modifier le
renderer, forcer le sélecteur ou synthétiser un record.

Le groupe AVI prioritaire est épuisé sans edge qualifiable; poursuivre
uniquement par les initialiseurs de configuration/table encore non résolus.

Le census ciblé de `0x82165E68` ferme aussi cette passe : aucun caller direct,
aucune écriture qualifiée de `param_2+0x12/+0x16`, et aucune vtable primaire
résolue. `done_when` courant : documenter ce contrat amont manquant et ne pas
passer au renderer tant qu'un payload guest réel n'est pas observé.

## Suite immédiate — après réfutation de la piste lock (2026-08-22)

La répétition `RtlTryEnterCriticalSection`/`RtlLeaveCriticalSection` est
classée lookup guest normal (`0x8219AF20`), sans import non géré ni cible
AVI/record/PM4. Aucun correctif natif n'est permis sur cette base. Le prochain
slice doit qualifier le créateur de `param_2` et la provenance des clés
`+0x12/+0x16`, puis l'owner `base+0xC → 0x8200B6FC` et son `r8`; seulement un
payload guest réel pourra ouvrir la lane renderer titre/menu.
Preuve : `artifacts/goal-playable/runtime-frontier/import-window.md`.

## Active next gate — owner AVI final (2026-08-22)

Le consumer `0x820FFCA0`, le contrôle de file `0x820FF710` et le corridor
type 1/4 sont désormais qualifiés. Le seul `done_when` statique est de
résoudre l'owner `0x8200B6FC/+4 → 0x82165CC0`, son `r8` (low16=1) et les cinq
ressources non nulles; toute valeur ou appel forcé reste interdit. Si cette
arête reste opaque, la documenter comme contrat guest non prouvé plutôt que
modifier le renderer.
Preuve : `artifacts/goal-playable/record-population/RESULT-followup-next.md`.

## Suite — île indirecte AVI (2026-08-22)

Le census final ferme les dispatchs simples et ne laisse que l'île opaque
comme discriminant. Si une lecture bornée de cette île ne qualifie pas
`base+0xC → 0x8200B6FC/+4 → 0x82165CC0` et l'affectation de `r8`, documenter
le contrat guest non prouvé et suspendre la lane payload; aucune valeur
`param_4=1` ne doit être injectée.
Preuve : `artifacts/goal-playable/avi-owner/RESULT-final.md`.

## Suite immédiate — frontière payload guest après MAIN_THREAD_PROMPT (2026-08-22)

Ne pas toucher au renderer, au bridge, aux imports ou au scheduler : le seam
Edge n'a pas atteint les PCs causaux et le probe natif ne remplit que des
records type 0 nuls. La prochaine passe doit rester statique et qualifier la
table/initialisation qui fournit l'objet `param_2`, les clés `+0x12/+0x16`,
l'owner `base+0xC → 0x8200B6FC/+4 → 0x82165CC0` et le `r8` dont le low16 doit
sélectionner 1. `done_when` : un chemin guest réel atteint un writer type 1–4
avec ressources non nulles, ou le contrat opaque est documenté comme blocker
précis. Aucun record synthétique ni appel forcé.
Preuves : `artifacts/xenia-edge-causal-gate/RESULT.md`,
`artifacts/goal-playable/runtime-frontier/RESULT-current-run.md`.

## Suite — call-site indirect AVI (2026-08-22)

La table `0x8200C5C0..0x8200C664` est désormais réfutée comme owner AVI;
ne pas la brancher au renderer. Le prochain gate statique doit remonter les
call-sites qui fournissent simultanément le receiver `r4` et le `r8` entrant de
`0x82165CC0` (dernier état qualifié : `0x82165D80: r6 ← r26 ← r8`).
`done_when` : arête guest réelle vers un writer type 1–4, ou blocker opaque
documenté avec son producteur manquant. Preuve :
`artifacts/goal-playable/avi-owner/RESULT-dispatch-table.md`.
# Prochaine action immédiate

Ne pas activer le contrat PM4 callback par défaut : la sonde courante n'a
rencontré aucun PM4, donc la livraison same-slice ne peut pas expliquer le
blocage actuel. Qualifier d'abord la construction et le dispatch du callback
renderer `0x821ADAB8`/`0x821ADC78` et son écriture de `device+0x5460`; si cette
voie reste sans arête vers le producteur de records, conserver le renderer
fail-closed et documenter le blocage.

## Prochaine gate — première ressource visuelle du logo (2026-08-22)

Ne plus modifier RT0, le resolve ou le writeback : la rotation P1/P2/P0 et la
persistance `LOAD/STORE` sont fermées. Remonter statiquement le déclencheur
guest et le producteur de la première soumission après l'IB P0
`0x12ACA8C0`, puis joindre son fetch texture au chargeur de
`brandLogo/003_NTXR`. `done_when` : un tuple texture complet, sa fenêtre guest
et son producteur sont qualifiés, ou une unique observable runtime bornée est
nommée pour fermer l'ambiguïté. Conserver le fetch courant exact et refuser
toute autre ressource tant que cette preuve manque; ne pas injecter l'asset.

## Suite immédiate — fermer le producteur naturel Q (23 août 2026)

L'index SDK du fetch 94 est fermé (`+94*2` correct) et le shader consomme
Q1. Le record naturel observé est toutefois `type=1` :
`0x82118D18` remplit P1/P2/P0, alors que Q reste nul et que le draw produit
zéro fragment. Ne plus modifier RT0, resolve, writeback, render-pass LOAD,
sampler ou shader BC3 sans nouvelle preuve.

Remonter statiquement l'owner/list record qui doit produire type 4/5/6,
relier le passage handle `0x59`/record #2 à `0x82119048`, puis qualifier la
publication Q vers le draw. `done_when` : un record guest naturel couvre Q
avant le draw et le readback natif devient non noir, ou le contrat amont
manquant est documenté précisément. Aucun fallback P→Q ni état synthétique.

## Suite immédiate — producteur du record type 4/5/6 (23 août 2026)

Le contrat `sub_820D5268` est maintenant qualifié : slots 6/21 et sélection
`result+4` sont naturels, mais l'owner retombe à l'index zéro et ne publie pas
le record #2. Remonter statiquement le créateur de la liste/du compteur `>=3`,
la production du handle `0x59`, puis le writer Q associé. `done_when` : un
record guest naturel atteint Q1 avant le draw et le readback natif devient non
noir, ou le contrat producteur manquant est documenté avec son adresse et ses
préconditions. Aucun patch de curseur, fallback P→Q ou pixel synthétique.

## Suite immédiate — énumérateur SWG et publication du record #2 (23 août 2026)

Le consumer `0x82326420` et la ressource `0x0E000059` sont fermés, mais
aucune arête ne relie le descripteur `0x1AA48` à la factory `0x820D18C8` ni à
un owner/list count `>=3`. Rechercher statiquement cet énumérateur, le writer
du lien record #1→#2 et l'entrée naturelle qui sélectionne le callback
`0x82119048`; ne pas lancer de runtime identique tant qu'une ambiguïté
causale nouvelle n'est pas nommée.

`done_when` : un record #2 naturel (`+0x0c=2`, handle `0x59`) traverse
`0x820EA9A0` et remplit Q1 avant le draw, ou le producteur opaque est
documenté avec ses préconditions et son adresse.

## Suite immédiate — factory observée, producteur SWG toujours opaque (23 août 2026)

La factory `0x820D18C8` est maintenant observée au retour, y compris la
création tardive de l'owner `0x2E3CFCD0`; cela ne publie toujours qu'un chemin
à un record/handle `0x57` ou `0x58`. Reprendre statiquement par l'interpréteur
SWG/ACC qui doit joindre `0x1AA48` à la liste et au callback Q
`0x82119048`. Ne modifier ni Xenos/Vulkan ni les pixels tant que
`record#2+0x0c==2` et `0x0E000059` ne sont pas observés naturellement.

`done_when` : payload guest réel vers Q1 et screencap non noir, ou contrat
producteur manquant identifié précisément.

## Suite immédiate — exclure l’interpréteur et qualifier l’entrée Q1

Les offsets `owner+213/+214/+216` de `0x820E50D8/0x820E5140` ne sont pas la
liste SWG. La prochaine passe doit donc joindre, dans le projet Ghidra démo,
le producteur naturel du record vers `0x82119048`, le selector Q1 et les
`0xd0` octets lus par le fetch 94. Une trace runtime n’est justifiée que si
ce join statique laisse une ambiguïté causale nommée ; ne pas copier P→Q,
forcer le callback ou peindre l’image.

## Suite immédiate — matérialisation SWG avant le worker (23 août 2026)

La trace précoce ferme l'ambiguïté runtime restante : le worker
`0x820FEFA8` ne reçoit qu'un record nul et le corridor
`0x8210A1C0 → 0x82117410` n'est pas atteint. Reprendre statiquement par
`0x82323BB8`, les callbacks ASContext/VM et la sélection owner/frame/list qui
doit produire count≥3, record #2 et handle `0x0E000059`. Tant que cette arête
guest n'est pas qualifiée, conserver le renderer fail-closed et le screencap
noir ; aucune correction Xenos/Vulkan n'est autorisée.

`done_when` : un payload guest naturel remplit Q1 avant le draw et le
readback devient non noir, ou le producteur manquant est documenté avec ses
préconditions précises.

## Suite immédiate — seam SWG→ACC à qualifier (23 août 2026)

Le curseur/frame actif est désormais attribué à `0x820E50D8/0x820E5140`, mais
aucun writer naturel ne remplit `owner+0x20`, le count/lien de liste ou
`record#2+0x0c=2` avec handle `0x0E000059`. Examiner statiquement le receiver du
slot 4 de `0x820D18C8`, les appels virtuels de `0x82323BB8` et les writers de
la liste. Ne pas lancer de nouvelle trace identique et ne pas modifier
Xenos/Vulkan tant que Q1 reste sans stores naturels.

## Suite immédiate — producteur du récepteur slot 4 (23 août 2026)

Qualifier statiquement l'écriture de `global_swg_context+0x10` et la forme de
`param_3` aux callsites `0x8232342C`/`0x82323594`. Chercher ensuite leur lien
vers `owner+0x20`, le compteur de liste, record #2 et handle `0x0E000059`.
Une trace n'est autorisée que si cette ambiguïté causale nommée survit à la
recherche statique ; dans ce cas observer uniquement le receiver/vtable et
`param_3+4/+8/+0xc` à ces deux PC. Aucun fallback P→Q, callback forcé ou
modification Xenos/Vulkan.

`done_when` : le writer naturel du contexte/record est qualifié jusqu'au
payload Q1, ou ses préconditions manquantes sont identifiées précisément.

## Suite immédiate — producteur de descripteur avant `0x820D18C8` (23 août 2026)

Le contexte SWG est maintenant qualifié comme init/teardown-only ; le join
owner→contexte est prouvé, sans chemin vers record #2/handle `0x59`. Reprendre
par le writer du descripteur candidat et son stockage avant
`0x820D18C8`, puis qualifier `owner+0x20 → list/count → record+0x0C==2`.
Ne modifier ni Xenos/Vulkan ni Q1 tant que cette arête guest n'est pas fermée.

`done_when` : la source naturelle du descripteur titre est jointe au record
#2/handle `0x59` et au payload Q1, ou la précondition producteur manquante est
nommée précisément.

## Suite immédiate — descripteur SWG vers owner titre (23 août 2026)

La trace confirme que le receiver slot 4 et sa factory s'exécutent naturellement
avec descripteurs `0x0B`/`0x01`, mais cette branche ne rejoint pas la liste titre.
Reprendre statiquement par les writers des descripteurs et l'interpréteur
ASContext qui doivent sélectionner le record type 4/5/6, puis publier count>=3,
record #2 et handle `0x0E000059`. Une nouvelle trace n'est permise que si un
join causal précis reste ambigu ; ne modifier ni Xenos/Vulkan ni Q1 autrement.

`done_when` : lien guest qualifié jusqu'à Q1 et draw non noir, ou précondition
producteur manquante documentée précisément.

## Suite immédiate — parser/producteur du blob passé à `0x82326B80` (23 août 2026)

Le descripteur `0x1AA48` n'a aucun producteur statique qualifié et le pseudo-hit
`0x821846A0` est réfuté. Reprendre par les writers du blob SWG consommé par
`0x82326B80`, puis fermer `owner+0x20 → offsets/list/count → record #2`.
Ne toucher ni au renderer ni au payload Q1 avant cette jonction guest.

`done_when` : le parser naturel produit la liste titre avec le record #2/handle
`0x59`, ou la première précondition manquante du blob est documentée.

## Suite immédiate — Q1 guest avant toute nouvelle sémantique renderer (23 août 2026)

Le probe Vulkan confirme que les fetchs titre 64x64 et 512x512 sont déjà
qualifiés et que les textures arrivent non nulles. La couverture reste nulle
car Q1 `[0x104A4890,0x104A4960)` contient quatre vertices zéro. Reprendre par
le writer naturel SWG/type-4/5/6 qui doit matérialiser Q1 et le record #2/handle
`0x59`; ne pas ajouter de profil fetch, copie P→Q ou pixel fabriqué.

`done_when` : un Q1 guest non nul atteint le draw existant, ou la précondition
producteur manquante est précisément documentée.

## Suite immédiate — entrée amont des writers provider (23 août 2026)

La trace bornée complète 190..430 ne voit aucun writer `0x82114350`,
`0x82114798` ou `0x82114A58`, alors que le worker de queue tourne avec un
record vide. Reprendre statiquement par l'appelant/état qui doit publier le
provider ou le premier record type 1..4 avant `0x8210A1C0`; ne pas ajouter de
profil fetch, copie P→Q, shim d'environnement ou pixel synthétique.

`done_when` : un producteur guest qualifié atteint le corridor et remplit Q1,
ou la précondition kernel/XAM/VFS amont est nommée et implémentée au point
partagé.

## Diagnostic de présentation fermé (23 août 2026)

Ne pas importer le présentateur BGRA de `reconstruction/ace-combat-6`: la pile
démo n'a pas cette interface et son readback CPU est déjà RGBA8 avant l'audit.
Le buffer observé est noir, donc poursuivre par le producteur guest du payload
et non par un swizzle de swapchain.

## Suite immédiate — caller de `0x82165E68` (23 août 2026)

Qualifier le dispatch virtuel/constructeur qui fournit `param_2` à
`0x82165E68`, ainsi que les producteurs des clés `+0x12/+0x16`. Le writer et
le renderer sont déjà qualifiés; ne pas ajouter de trace globale, de valeur
synthétique ou de sémantique Vulkan.

`done_when` : un caller naturel fournit des valeurs non nulles qui passent
`0x821EE130/0x821EE0F8`, ou le contrat partagé manquant est nommé précisément.

## Suite immédiate — observer le contrat `param_2` au caller qualifié (23 août 2026)

Le slot primaire est maintenant joint statiquement : `0x82216498 → +4 →
0x82165E68`. Le second argument observé statiquement (`bVar6`) paraît scalaire,
alors que le callee le traite comme une base avec `+0x12/+0x16`. Une seule
trace bornée à l'entrée `0x82165E68`, aux deux champs et au readback est permise
pour départager une conversion ABI réelle d'un chemin inactif. Ne pas remplir
les champs, forcer l'appel ou modifier Xenos/Vulkan.

`done_when` : l'entrée naturelle fournit un objet/valeurs cohérents et rejoint
le provider, ou le contrat binaire manquant est confirmé sans ambiguïté.

## Suite immédiate — fermer le producteur Q1/SWG, pas le présentateur

Le caller `0x82216498 → 0x82165E68` n'est pas atteint dans la fenêtre
frontend bornée et son contrat `param_2` reste indéterminé. Reprendre
statiquement le producteur SWG/record qui doit publier Q1 à `0x104A4890` avant
de modifier le renderer. Le dump CPU pré-présentation confirme une image
noire ; la correction BGRA de la pile retail n'est pas applicable à cette
démonstration.

`done_when` : un writer guest qualifié remplit Q1 et le draw change RT0, ou une
précondition partagée manquante est identifiée et implémentée au point commun.
# Prochain gate — observation guest-state bornée de l'enfant Title

Observer l'unique invocation naturelle déjà qualifiée au tick 3001, depuis
l'entrée de `0x82323468` jusqu'au retour du premier tick récursif de l'enfant.
Lire `B+0/B+0x38`, la commande `param3`, les cinq mots de `D`, les bornes de
frame de l'enfant, frame 0, ses éléments et les offsets drainés vers
`0x82325288`.

`done_when` : descripteur, frame 0 et premier offset VM joints, ou frame 0
démontrée sans type 6. Une route seulement; pas d'A/B ni de trace globale.
# Prochain gate — cycle 1810

Fermer statiquement le chemin couleur du QuadList logo : bytes guest du dword
`ffff0000`/`bfff0000` → endian `k8in32` → `FMT_8_8_8_8` → swizzle du
`vfetch_full r2.zyxw` → export VS/interpolator1 → multiplication PS. Comparer
ce résultat au format RT/readback pour déterminer lequel annule R et G.

`done_when` : une seule des deux frontières (couleur shader ou RT/readback)
explique les coins `(0,0,255)` et les canaux R=G=0, avec un correctif minimal
statiquement justifié. Ensuite seulement, un run codegen-on borné doit produire
une screencap dont les quatre coins sont blancs selon
`artifacts/goal-playable/hsio-static-gate-20260824/verify_white_background.py`.

Ne pas réactiver le retour HSIO 0, ne pas utiliser codegen-off et ne pas
revendiquer le frontend sur la seule présence de pixels non noirs.
# Gate suivant — xref de donnée du writer P1

Scanner l'image demo qualifiée pour les dwords big-endian égaux à
`0x82118D18` et, séparément, recalculer les cibles de toutes les branches PPC
directes. Pour chaque cellule trouvée, remonter le chargement de table jusqu'au
`mtctr/bctrl`, établir l'index/type sélectionné, puis suivre `r4` et les stores
vers `r4+0x18`.

`done_when` : cellule, index de dispatch, type/adresse du record `r4`,
instruction qui peuple `+0x18` et valeur amont de `BFFF0000` identifiés. Si
aucune cellule n'existe, conserver l'unique callsite indirect avant de définir
un watchpoint borné ; aucun runtime auparavant.

Frontière issue du cycle 1815 : aucun appel direct matérialisé dans le HIR et
`direct_calls=[]` dans l'atlas. Voir
`artifacts/goal-playable/title-p1-color-producer-static-20260824/BLOCKER.md`.
# Gate suivant — consommateur de la table `0x82009E8C`

Chercher les constructions et chargements de la base ou du voisinage de
`0x82009E8C`, puis qualifier le premier calcul `base + index*0x14` qui charge
`entry+0x0C` avant `mtctr/bctrl`. Établir quels champs bruts (`+0x00`, `+0x04`)
sélectionnent les deux entrées P de `0x82009E8C` et `0x82009EB4`. Depuis ce
callsite seulement, suivre la définition de `r4` puis les stores vers
`r4+0x18`.

`done_when` : callsite indirect et index discriminant prouvés, type/adresse du
record `r4`, instruction qui écrit `+0x18` et valeur amont de `BFFF0000`
identifiés. Aucun runtime tant que la xref de base reste statiquement
exploitable.

Frontière :
`artifacts/goal-playable/title-p1-dispatch-xref-static-20260824/BLOCKER.md`.
