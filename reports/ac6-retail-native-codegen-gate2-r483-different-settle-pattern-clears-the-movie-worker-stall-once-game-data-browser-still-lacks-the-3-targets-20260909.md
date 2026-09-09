# AC6 retail NTSC-U/J — r483 — un motif de repli DIFFÉRENT (celui déjà utilisé par `run_gate.py` lui-même) franchit le blocage « movie worker » que r482 avait réfuté pour un autre motif ; `game-data-browser` atteint proprement, mais ne contient toujours aucune des 3 cibles

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (lancements directs de `install/ntsc-uj/bin/ac6recomp` via
`tools/run_gate.py --diagnostic-route ... --mission-dump-shaders`, sous
Xvfb, deux runs bornés `timeout 240`).

## Contexte et numérotation

Ce cycle a été exécuté par une sous-tâche reprise après une interruption
médiane (le fork original s'est arrêté en attendant la fin d'un
processus). Pendant cette même fenêtre, un **autre** cycle (également
numéroté r482 dans cette chaîne, commit `53625ef5`,
`reports/ac6-retail-native-codegen-gate2-r482-menu-navigation-probe-
stall-is-not-caused-by-preamble-timing-likely-flaky-boot-not-route-
content-20260909.md`) a testé et réfuté une hypothèse différente sur
le même blocage, et l'a committé en premier. Ce cycle-ci est donc
**renuméroté r483** (pas de deuxième r482) et se construit
explicitement sur les résultats de r482, qu'il ne contredit pas : les
deux cycles ont testé des correctifs *différents* du même symptôme,
séquentiellement (le fichier de route lu par cette sous-tâche
contenait déjà le préambule byte-identique de r482 au moment où elle a
commencé à travailler dessus), pas en parallèle sur un état commun.

**Contrainte de coordination toujours en vigueur** : vérifiée avant et
après ce cycle, `native/` inchangé (dernière écriture `05:07:55`,
toujours identique à `05:41`, 34 min de silence — la plus longue plage
calme observée dans la chaîne concurrente r488+ à ce jour). Aucun
fichier sous `recompilation/ace-combat-6-retail/native/` touché ce
cycle.

## Établi — r482 a réfuté l'hypothèse « préambule identique à une route
qui marche » ; ce cycle en a testé une autre, différente, qui a marché

r482 a rendu le préambule de `us-menu-navigation-probe.steps`
**byte-identique** à `us-pretype28-startup.steps` (jusqu'à
`wait-pulse type28=30 Escape+space` compris) — le blocage s'est
reproduit à l'identique (journal muet dès `05:32:27.839`, process actif
121% CPU jusqu'au timeout). C'est une réfutation directe et solide :
faire correspondre le contenu à une route qui marche par ailleurs ne
suffit pas.

Ce cycle a testé un troisième motif, **différent des deux précédents**
(ni le motif littéral fragile de `Escape+space` immédiat, ni sa copie
byte-identique) : celui que `tools/run_gate.py:745-756` utilise
lui-même en interne pour ses propres chemins scellés
(`mission_cinematic_handoff`/`mission_render_summary`), qui **remplace**
les 4 premiers pas de la route scellée par
`sleep 20` puis `wait-pulse type28=30 Escape@90` (pulsation `Escape`
seule, pas de `space`, fenêtre de stabilisation de 20 s avant la
première pulsation). Le commentaire de ce code documente précisément
pourquoi ce motif existe : « a later failed receipt stopped presenting
when the fixed Escape/A pair straddled the movie transition » — un
diagnostic déjà écrit dans ce dépôt pour un symptôme très proche de
celui que r482 vient de confirmer être un vrai wait noyau (r482 :
`xboxkrnl_threading.cpp:100-114`, `IsAc6MovieWorkerWait()`).

**Résultat** : `RESULT.json` du run avec ce troisième motif —
`"clean_shutdown": true`, `"game_status": 0`, `"error": ""`,
`"duration_seconds": 68.99`. Les trois captures attendues produites
(`step-02-post-settle.png`, `step-04-type28-30.png`,
`step-13-game-data-browser.png`, tailles non nulles). 22 fichiers
`.ucode` (11 nuanceurs uniques) et un couple `.xsh`/`.fsi.vk.xpso`
(`4E4D07D1`) exploitable — traduit avec succès
(`parse_rexglue_cache.py` : 11 nuanceurs, 8 pipelines, 12 paires).

**Ce résultat ne contredit pas r482** : les deux motifs testés
(préambule byte-identique à pretype28, contenant `Escape+space` sans
délai de repos ; motif `sleep 20`+`Escape@90` de `run_gate.py`) sont
concrètement différents. r482 a réfuté le premier ; ce cycle a
confirmé — une seule fois — que le second fonctionne. **Une seule
réussite n'est pas une preuve contre la variance de cycle documentée
par r482** (« le mécanisme peut compléter un cycle complet en moins de
120 ms... c'est une VARIANCE non expliquée entre cycles rapides et
cycles de 1,4 s ou plus », rapport r280 cité par r482's chaîne) : il
reste possible que ce succès soit en partie un tirage favorable de
cette même variance plutôt qu'un correctif déterministe. Une seule
exécution positive contre plusieurs échecs documentés (r479, r480,
r482) n'est pas assez pour classer ce motif comme fiable sans
re-vérification.

## Établi — traduction et requête : `game-data-browser` ne contient
toujours aucune des 3 cibles de r478 (recoupe le constat déjà connu)

Comparaison directe (script Python, conversion hex/decimal des
hachages `xsh_records`/`pipeline_records`) contre les 3 cibles de r478
(`09dd1c7cddae1141`, `57b8e5f14b93cff4`, `4dd456c4ea0923c1`) —
**aucune correspondance**, ni comme nuanceur vertex ni comme nuanceur
pixel, dans aucun des 8 pipelines. Les 11 nuanceurs recouvrent
uniquement des identifiants déjà connus depuis r479
(`0a6d1dd7767fdf27`, `c049a8c9e556f129`, `472913f460d4b446`,
`bbaada3605b82c5a`) plus 7 nouveaux (`2e372ea28cc404b7`,
`8f1c48ba92c8e43e`, `1899f02dc6758d8f`, `25ed986fb3f8e797`,
`43661348f44add3d`, `ea41c0069ae03769`, `94f8cdbccdcf7a92`), aucun ne
correspondant aux 3 cibles.

## Non établi

- **Si le motif `sleep 20`+`Escape@90` est fiable ou juste chanceux
  cette fois** — nécessite une re-vérification (plusieurs runs), pas
  faite ce cycle (budget/temps).
- Où, dans le flux de menus/écrans, les 3 nuanceurs cibles apparaissent
  réellement — ni l'écran-titre (r476), ni le menu principal (r481),
  ni `game-data-browser` (r482/ce cycle) ne les contiennent.
- Si les 7 nouveaux nuanceurs capturés ce cycle méritent une fusion
  dans le registre — **non fait ce cycle**, voir Décisions.

## Décisions prises

- Renuméroter ce cycle r483 plutôt que d'écraser ou de contester
  l'entrée r482 déjà committée — les deux résultats sont réels et
  complémentaires, pas contradictoires une fois la chronologie
  reconstituée.
- Ne PAS fusionner les 7 nouveaux nuanceurs dans
  `native/fixtures/pinned-shader-registry.v1.bin` ce cycle — contrainte
  de coordination `native/` toujours active, et aucun des 7 n'est une
  cible connue de toute façon.
- Ne pas re-tenter immédiatement une troisième variante de minutage
  sans nouvelle preuve motivante — cohérent avec la décision déjà prise
  par r482 sur le même principe (rendement décroissant).
- Ne pas toucher `reports/handoff/CURRENT.json` ni réordonner le
  pointeur en tête de `NEXT.md` (pointeur actif de la session
  concurrente).

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts (aucune source de contrat modifiée ce cycle). `ctest` natif
délibérément non relancé (aucun fichier sous `native/` touché ce cycle,
et le lancer contesterait le répertoire de build avec la session
concurrente). `git status` confirmé propre sous `native/` avant ce
commit.

## Named for r484

**Où chercher les 3 nuanceurs cibles ensuite** — inchangé par rapport
à ce que r482 avait déjà nommé, plus une option supplémentaire issue
de ce cycle :
1. Lire le désassemblage invité autour de `0x82916E2C`/`0x82916E3C`/
   `0x82916E08` (session Ghidra requise) pour comprendre la vraie
   condition de signalement du « movie worker » — la piste la plus
   solide, nommée par r482, toujours non suivie.
2. **Re-vérifier le motif `sleep 20`+`Escape@90` de ce cycle sur
   plusieurs runs** avant de le considérer fiable — un seul succès
   n'écarte pas la variance documentée par r482/r280.
3. Explorer la navigation RÉELLE dans `game-data-browser` (la route
   actuelle ne fait qu'un seul `key Left` avant l'écran final, sans
   explorer la liste elle-même) ou un écran plus loin dans le flux
   (hangar, carte tactique) avec `--dump_shaders` actif.
4. Reconsidérer la piste HUD de vol (`fetch_const`, r475) — piste
   secondaire jamais suivie, nommée depuis le plan initial.
5. Envisager une pause de la piste A — 3 cycles consécutifs (r481,
   r482, r483) sans nouvelle capture utile touchant une cible depuis
   r477 ; à confirmer avec l'utilisateur plutôt qu'à décider seul, le
   dernier arrêt de piste A ayant déjà nécessité une décision
   explicite (r480).

`native/` restait calme à la fin de ce cycle (05:07:55 → au-delà de
05:41, >34 min) — plus long qu'aucun intervalle observé dans la chaîne
concurrente r482-r489 (3-15 min) ; à reconfirmer avant toute fusion de
registre, mais un signe possible que cette session a atteint une pause
stable.

## Files

Committés : ce rapport,
`recompilation/ace-combat-6-retail/routes/us-menu-navigation-probe.steps`
(commentaire mis à jour, motif de repli changé), `NEXT.md` (entrée
ajoutée après celle de r482, pointeur actif de la session concurrente
non touché).
Non conservés (scratch) :
`/fastdata/lavaulta/tmp/r482-scratch/` (sorties des runs, JSON de
traduction intermédiaires — nom de répertoire hérité du cycle
précédent, non renommé).
