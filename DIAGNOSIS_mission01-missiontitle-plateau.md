# DIAGNOSIS — Le boot plafonne à ~1700 updates (Opening tardif → MissionTitle) et n'atteint pas le vol Mission 01

Sujet : pourquoi le binaire recompilé natif n'atteint pas la condition du goal
(un `r567 mode ordinal` avec vptr au-delà de Briefing `0x8206360c` et `updates`
qui croît en vol). Mesures en lecture seule sur les runs existants. Tables et
trajectoires : `diagnosis/mission01-missiontitle-plateau/trajectory.md`.

## 1. Écart

- **Métrique** : `updates` (compteur de frame de la boucle de jeu `r563 boot`)
  et `mode_vptr` atteint.
- **Cible (goal)** : `mode_vptr ≥ 0x8206360c` (Briefing) puis vol, avec `updates`
  +30 après l'entrée en vol.
- **Valeur observée** : `updates` plafonne à **1700–1703** ; `mode_vptr`
  atteint au mieux **0x82065064 (MissionTitle)** (Run A), sinon reste à
  `0x820661fc (Opening)` (Run B). Jamais Briefing. Écart = **~2 transitions de
  mode entières** (MissionTitle→Briefing→chargement→vol).
- **Dispersion (2 runs)** : Run A atteint MissionTitle (frame 1702) puis fige ;
  Run B s'arrête à update 1700 (fenêtre de sonde). Les deux plafonnent à
  `updates≈1700–1703`, `presents≈1703–1706`. La variance inter-run se limite à
  savoir si les 2–3 derniers updates (la transition) passent avant la fin de la
  fenêtre — une course que A gagne et B perd. **Le plateau lui-même est stable.**
  L'écart dépasse largement cette dispersion → distinguable, on continue.

## 2. Comparabilité

- Même binaire `ac6recomp`, même ISO, même point d'entrée sonde, même cadence
  de snapshot (100 ms). Preuve : en-têtes de log identiques.
- Diff d'instrumentation : Run A = `VD_TRACE+OPENING_TRACE` ; Run B =
  `BOOT_PHASE_TRACE` ; hooks bogués dans les deux (corrigés seulement dans le
  run C, en cours). Les hooks sont des `fprintf` gated → n'altèrent pas le
  timing guest de façon matérielle. Comparable pour la trajectoire d'updates.
- `ms=` est **l'horloge murale** (7 200 000 ms = 120 min = durée mtime du log).
- Taille d'échantillon face à l'écart : le plateau est vu sur 2 runs
  indépendants avec la même valeur → suffisant pour établir le plateau, pas pour
  une statistique fine.

## 3. Hypothèses (écrites avant attribution)

- **H1 — logique de readiness (guest)** : le tree-walk de MissionTitle
  `Function_821D2860` renvoie 0 parce qu'un enfant du nœud `0x18a30200` reste
  incomplet (liste `+0x1c/+0x20` non vide, un enfant `vtable[0x14]()`→0).
  Mesure discriminante : treepoll corrigé (run C) — listes non vides + un enfant
  bloqué à 0. Prédiction si vraie : `asset_calls` **plafonne** pendant que
  `updates` rampe.
- **H2 — frame-gate / fence GPU** : la boucle attend le flip fence
  (`sub_821E61A8`, slot `producer+10896`) ; chaque frame attend car les presents
  sont lents ou une fence ring n'avance pas. Mesure : corrélation
  presents↔updates. Prédiction : `updates ≈ presents`, presents lents.
- **H3 — débit de streaming d'assets / contention GPU (ressource)** : chaque
  frame avance un pas de préchargement d'assets (r580 opening-ready, 42
  éléments depuis DATA00.PAC = 2,2 Gio) ; le streaming est étranglé par la
  contention GPU/IO (neural_amp sature le GPU à ~18 Gio). Prédiction :
  `asset_calls`/`o3a4` montent **~1:1 avec updates** dans la bande de collapse,
  sans plateau.
- **H4 — signal HLE non émis (r590)** : un event guest `0x10001a00` jamais
  signalé, thread parké. Mesure : /proc du thread game-loop.

## 4. Mesures et verdict

- **H4 — RÉFUTÉE** : /proc du run A (tid 3823197) = `state=R`, `utime` avance,
  `syscall` vide → le thread **spinne en userspace**, il n'est pas parké sur un
  futex. (r590 venait d'un run THREAD_SAMPLE différent.)
- **H2 — soutenue partiellement** : Run A `presents=1706 ≈ updates=1703` (≈1:1)
  → la boucle est bien cadencée par le present/frame-gate. Mais cela n'explique
  pas le **collapse ~250×** : voir H3.
- **H3 — soutenue (dominante)** : le collapse du taux d'updates est **net et
  localisé** à update≈1655 (Run A) / ≈1680 (Run B), pas uniforme. Dans la bande,
  `updates` monte ~12/slice **en lockstep** avec `r580 opening-ready asset_calls`
  (44→56→68, +12/slice) et `o3a4` (13→19→25). Soit **~1 élément d'asset par
  update, ~50 s/update**. Les assets **montent encore** au dernier échantillon
  (pas de plateau) → étranglement de **débit**, pas (pour l'instant) un verrou
  logique. Baseline : presents ~0,24 fps tôt, ~0,004 fps dans la bande — GPU
  gravement affamé par neural_amp.
- **H1 — INDÉCIDABLE** : les treepoll/loadpoll existants (`list1c=0 f20=0
  realkey=0`) sont des **artefacts d'instrument** (r596 : mauvaise cellule +0xE0,
  gardes d'adresse rejetant le tas), donc sans valeur. Le run C (hooks corrigés)
  tranchera H1 vs H3.

## 5. Contribution retenue

- **Prédicat** : ~toute l'excès de temps mural se concentre sur les updates
  **≥ ~1655–1680** (Opening state=1 « opening-ready » → MissionTitle), où le
  coût par update passe de <1 s à **~50–270 s**, en lockstep avec le
  préchargement d'assets (`asset_calls`/`o3a4` ~1:1 avec `updates`).
- **Mécanisme (meilleur courant, H3)** : préchargement d'assets **synchrone par
  frame** (r580, 42 éléments depuis DATA00.PAC 2,2 Gio) au débit étranglé par la
  contention GPU/IO (neural_amp sature le GPU). La boucle n'est pas bloquée : elle
  progresse d'un asset par frame, très lentement.
- **Prédiction chiffrée qu'un correctif devra satisfaire** : un correctif (ou
  un GPU libéré) qui restaure un débit d'asset proche du régime pré-collapse
  (~700 updates/slice) doit faire croître `updates` **de +30 au-delà de la
  transition MissionTitle (frame 1702) dans la fenêtre de 180 min**, avec
  `r580 ready-complete result=1` pour les 42 éléments. **Test décisif H3 vs H1**
  (run C, treepoll corrigé) : si `asset_calls` continue de monter pendant le
  crawl et les listes du nœud `0x18a30200` sont non vides → H3 (débit) ; si
  `asset_calls` plafonne ou un enfant reste à 0 avec listes stables → H1
  (verrou logique), et la prédiction bascule sur « débloquer cet enfant ».

## 6. Non expliqué, non vérifié

- **~65× de l'écart de coût/update reste non attribué au seul present** : 270
  s/update ≫ 1 present (~4 s) → chaque update fait plus qu'attendre un present
  (travail CPU d'asset ? plusieurs presents ? attente fence composée). Non
  mesuré : le détail par-update (aucun hook de timing par update).
- **Valeurs guest réelles** (`realkey` attendu ~b362294c, listes du nœud
  `0x18a30200`, retour réel de `Function_821D2860`) : non lues — `/proc/mem`
  refusé sous `ptrace_scope=1`, run C corrigé en cours (~2–3 h sous contention).
- **IP exact du spin** de tid 3823197 : non épinglé (ptrace bloqué).
- **Si le préchargement finirait un jour** à GPU libre : non testé (neural_amp
  « pas touche » ; contention non levée cette session).

Correctif : voir skill controlled-fix.
