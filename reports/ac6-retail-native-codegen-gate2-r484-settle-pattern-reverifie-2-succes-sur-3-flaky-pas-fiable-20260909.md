# AC6 retail NTSC-U/J — r484 — le motif de repli `sleep 20`+`Escape@90` (r483) est RE-VÉRIFIÉ sur 3 exécutions au total : 2 succès (r483, run « a » de ce cycle), 1 échec identique au blocage « movie worker » (run « b » de ce cycle) — confirmé FLAKY, pas fiable, cohérent avec la variance déjà documentée par r482

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (lancements directs de `install/ntsc-uj/bin/ac6recomp` via
`tools/run_gate.py --diagnostic-route routes/us-menu-navigation-probe.steps
--mission-dump-shaders`, sous Xvfb, deux runs bornés `timeout 240`
supplémentaires — décision utilisateur explicite : « One more cycle:
verify the settle fix », en réponse à la question posée après r483).

## Contexte

Nommé par r483, piste 2 : re-vérifier le motif `sleep 20`+`Escape@90`
sur plusieurs exécutions avant de le considérer fiable, puisqu'un seul
succès (r483) n'écarte pas la variance de cycle déjà documentée par
r482/r280.

**Contrainte de coordination toujours en vigueur, respectée
intégralement** : la session concurrente modifiant `native/` sans
commit reste calme — dernière écriture `2026-09-09 05:07:55`, toujours
identique à `08:36`, soit **plus de 3h28 de silence**, très
au-delà de tout intervalle observé dans sa propre chaîne (3-15 min).
Ce fait est noté pour la décision de la prochaine piste (voir Named
for r485) mais n'a motivé aucune action sur `native/` ce cycle — zéro
fichier sous `recompilation/ace-combat-6-retail/native/` touché,
vérifié `git status` avant et après.

## Établi — deux exécutions supplémentaires, une réussie, une échouée, au même point que les échecs déjà documentés

Deux runs supplémentaires du motif de r483 (`sleep 20` / `capture
post-settle` / `wait-pulse type28=30 Escape@90`), chacun sous un
`Xvfb`/port dédié :

- **Run « a »** : `RESULT.json` — `"clean_shutdown": true,
  "game_status": 0, "error": "", "route": {"executed_steps": 13,
  "steps": 13}`. Les trois captures attendues produites
  (`step-02-post-settle.png`, `step-04-type28-30.png`,
  `step-13-game-data-browser.png`), `game-data-browser` atteint
  proprement.
- **Run « b »** : `RESULT.json` — `"clean_shutdown": false,
  "game_status": -9, "error": "log predicate not reached: type28=30",
  "route": {"executed_steps": 3, "steps": 13}`. Bloqué exactement au
  même point que r482/r479/r480 : la première attente `type28=30`
  n'est jamais satisfaite.

**Bilan cumulé sur les 3 exécutions connues de ce motif exact** (r483 +
run « a » + run « b ») : **2 succès, 1 échec** — un taux d'échec réel
et non négligeable (~33% sur cet échantillon, trop petit pour un
chiffre précis mais assez pour réfuter la fiabilité). Le motif n'est
donc **pas déterministe** : il ne s'agit pas d'un bug de contenu de
route (déjà réfuté par r482 sur un autre motif) ni d'un correctif
fiable, mais d'une réduction du taux d'échec sans élimination de la
cause sous-jacente — cohérent avec la variance de minutage réelle déjà
mesurée par le rapport r280 cité en chaîne par r482 (« le mécanisme
peut compléter un cycle complet en moins de 120 ms... c'est une
VARIANCE non expliquée »).

## Établi — le run réussi ne contient toujours aucune des 3 cibles (recoupe r483)

`shader-dump/` du run « a » contient exactement les 11 identifiants de
nuanceurs déjà rapportés par r483 (`0a6d1dd7767fdf27`,
`1899f02dc6758d8f`, `25ed986fb3f8e797`, `2e372ea28cc404b7`,
`43661348f44add3d`, `472913f460d4b446`, `8f1c48ba92c8e43e`,
`94f8cdbccdcf7a92`, `bbaada3605b82c5a`, `c049a8c9e556f129`,
`ea41c0069ae03769`) — comparaison directe des noms de fichiers
`shader_<HASH>.ucode.*`, aucun ne correspond aux 3 cibles de r478
(`09dd1c7cddae1141`, `57b8e5f14b93cff4`, `4dd456c4ea0923c1`). Pas de
nouvelle traduction lancée ce cycle (l'ensemble de hachages étant
identique à celui déjà traduit et vérifié par r483, une nouvelle passe
de traduction n'aurait rien appris de plus).

## Non établi

- **Le taux d'échec réel** — 3 échantillons est trop peu pour une
  estimation précise (33% ± large marge). Établir un chiffre fiable
  nécessiterait davantage d'exécutions, non justifié par le rendement
  déjà décroissant de cette piste.
- **La cause du blocage lui-même** — toujours non lue (nécessite une
  session Ghidra autour de `0x82916E2C`/`0x82916E3C`/`0x82916E08`,
  piste nommée depuis r482, jamais suivie).
- **Si la session concurrente sur `native/` a atteint un arrêt stable
  ou est simplement en pause longue** — 3h28 de silence est un signal
  fort mais pas une confirmation (aucun commit, aucun message reçu).

## Décisions prises

- Ne pas fusionner les 11 nuanceurs du run « a » dans le registre
  épinglé — contrainte `native/` toujours active, et aucun n'est une
  cible connue de toute façon (même raisonnement que r483).
- Ne pas relancer d'exécutions supplémentaires pour affiner le taux
  d'échec — l'objectif de ce cycle (déterminer si le motif est fiable
  ou chanceux) est atteint avec une réponse claire (« pas fiable »),
  et chaque exécution supplémentaire coûte un budget oracle réel pour
  un gain de précision marginal sur un motif qui, fiable ou non, ne
  capture de toute façon aucune des 3 cibles.
- Ne PAS toucher `native/` ce cycle — vérifié avant et après.
- Ne pas agir sur le silence de 3h28 de la session concurrente
  (ni fusion de registre, ni reprise d'édition `native/`) — ce fait
  est nommé pour la prochaine décision utilisateur, pas pris en
  autonomie, puisque la piste A a déjà nécessité deux points de
  décision explicites (r480, puis la question posée après r483).

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts (aucune source de contrat modifiée ce cycle). `ctest`
natif délibérément non relancé (aucun fichier sous `native/` touché
ce cycle). `git status` confirmé propre sous `native/` avant ce
commit (mêmes 7 fichiers modifiés par l'autre session, aucun ajout).

## Named for r485

**Piste A reste ouverte, sans nouvelle cible capturée depuis r477** —
4 cycles consécutifs (r481, r482, r483, r484) de travail réel et
vérifié (bugs de minutage trouvés et corrigés, mécanisme du blocage
confirmé être un vrai wait noyau, motif de repli caractérisé comme
flaky plutôt que fiable) sans qu'aucun ne rapproche d'une capture des
3 états cibles. C'est le même rendement décroissant que celui qui a
justifié la pause de r480, maintenant sur un nombre de cycles
comparable. Candidats pour la suite, aucun tenté ce cycle :

1. Lire le désassemblage invité autour de `0x82916E2C`/`0x82916E3C`/
   `0x82916E08` (session Ghidra requise) — piste la plus solide,
   nommée par r482 et r483, toujours non suivie ; c'est la seule
   piste qui attaquerait la cause plutôt que de continuer à mesurer
   ses symptômes.
2. Explorer plus loin dans le flux (navigation réelle dans
   `game-data-browser`, hangar, carte tactique) avec `--dump_shaders`
   actif — jamais tenté au-delà d'un seul `key Left`.
3. Reconsidérer la piste HUD de vol (`fetch_const`, r475) — piste
   secondaire jamais suivie.
4. **Recommandation explicite : pause de la piste A** — 4 cycles
   consécutifs sans nouvelle cible, motif de repli maintenant
   caractérisé (pas fiable, ne capture de toute façon pas les cibles
   même quand il réussit) ; décision utilisateur à confirmer, comme
   pour r480.
5. **Fait notable pour toute reprise de la coordination `native/`** :
   la session concurrente est maintenant silencieuse depuis 3h28,
   largement au-delà de son propre rythme observé (3-15 min) — un
   signal fort qu'elle pourrait avoir atteint une pause stable, mais
   non confirmé (aucun commit, aucune réponse à la coordination
   tentée par le cycle parent). À vérifier avant toute décision de
   fusion du registre ou de reprise d'édition `native/`.

## Files

Committés : ce rapport, `NEXT.md` (entrée ajoutée après celle de
r483, pointeur actif de la session concurrente non touché). Non
conservés (scratch) : `/fastdata/lavaulta/tmp/r484v/`,
`/fastdata/lavaulta/tmp/r484-verify-a/`,
`/fastdata/lavaulta/tmp/r484-verify-b/` (sorties des runs, dumps de
nuanceurs). `reports/handoff/CURRENT.json` délibérément non touché
(pointeur actif de l'autre session, même raisonnement que r481-r483).
Aucun fichier sous `recompilation/ace-combat-6-retail/native/`
modifié.
