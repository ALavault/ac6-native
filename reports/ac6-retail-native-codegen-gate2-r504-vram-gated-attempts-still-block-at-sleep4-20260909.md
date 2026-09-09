# AC6 retail NTSC-U/J — r504 — trois tentatives sous garde VRAM ≥4 Go (règle explicite de l'utilisateur), toujours aucune donnée fusionnable : le blocage `sleep(4)` de r501/r503 n'est PAS lié à la VRAM

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (3 tentatives réelles).

## Contexte

Nommé par r503 : le crash `SIGABRT`/`EDRAM buffer` était corrélé à
une VRAM libre de ~7,9 Go sur 24 Go (job externe `neural_amp`
occupant ~16 Go). L'utilisateur a posé une règle explicite : **ne
tenter une capture que si ≥4 Go de VRAM libre, sinon ne rien
lancer.** Ce cycle vérifie la VRAM avant CHAQUE tentative (pas
seulement au début) et respecte cette garde strictement.

## Établi — VRAM stable, largement au-dessus du seuil, tout le cycle

`nvidia-smi --query-gpu=memory.free` vérifié avant chaque tentative
et après chaque échec : **7911 MiB libres, constant du début à la
fin des 3 tentatives** (aucune fluctuation observée — le job externe
`neural_amp` gardait une empreinte VRAM stable). Largement au-dessus
du seuil de 4096 MiB — aucune tentative n'a été sautée, aucun
« no go » nécessaire ce cycle.

## Établi — les 3 tentatives échouent au MÊME point que r501/r503, sans lien avec la VRAM

Les 3 tentatives (`--display :301/:302/:303`) ont échoué à
l'identique : `KeyboardInterrupt` levée depuis
`_raise_keyboard_interrupt_on_sigterm()` pendant
`tools/ac6-oracle-run.py::sleep(4)`, appelé depuis `wait_log()`
(ligne 535) — le délai de réglage de 4s après chaque pulsation de
touche (commentaire ligne 532-534 : « Guest transitions may complete
nearly two seconds after an edge, then need one flush interval to
become visible »), avant que le `timeout 240` externe ne déclenche
le `SIGTERM` du wrapper. Aucun `RESULT.json` produit dans les 3 cas —
échec avant la fin de route, pas un timeout de route complétée.

La tentative 3 a néanmoins produit 6 fichiers de dump (3 nuanceurs :
`0a6d1dd7767fdf27`, `2e372ea28cc404b7`, `c049a8c9e556f129`) avant
d'être interrompue — **aucun des 3 ne correspond aux cibles de r478**
(`09dd1c7cddae1141`, `57b8e5f14b93cff4`, `4dd456c4ea0923c1`). Les
deux premiers digests sont déjà connus depuis r479 (lancement direct
zéro-entrée) ; `2e372ea28cc404b7` (nuanceur fragment) est nouveau
mais non pertinent — aucun des 3 objectifs.

**Conclusion directe** : ce blocage précis à `sleep(4)` — le même
que r498/r501/r503 ont documenté — se produit avec une VRAM libre
constante et confortable (7,9 Go, 2x le seuil fixé). La cause de CE
blocage spécifique n'est donc PAS la VRAM, contrairement au crash
`SIGABRT`/`EDRAM` distinct que r503 a trouvé sous une VRAM plus
basse. Ce sont deux modes d'échec différents, déjà notés comme
disjoints par r503 lui-même.

## Établi — le correctif SIGTERM de r502 tient, troisième vérification consécutive

Les 3 tentatives ont chacune été terminées par `timeout 240` (donc un
`SIGTERM` réel envoyé au wrapper) — **aucun processus résiduel après
aucune des 3** (`pgrep -af "Xvfb|ac6recomp"` vide après chaque
nettoyage manuel de scratch). Confirme r502 en usage réel, sans
régression.

## Non établi

- **La cause exacte du blocage `sleep(4)`** — toujours non
  identifiée. r499 avait fermé cette piste comme « juste l'endroit où
  l'horloge globale a expiré », mais la RÉPÉTITION exacte à ce même
  point précis, sur 3 tentatives consécutives sous VRAM stable, dans
  ce cycle ET dans r498/r501/r503, suggère quelque chose de plus
  systématique qu'une simple variance de minutage aléatoire — non
  investigué plus avant ce cycle (hors périmètre : aurait nécessité
  une lecture Ghidra ciblée sur ce qui se passe côté invité pendant
  cette fenêtre de 4s précise, jamais tentée).
- **Le correctif `AudioRuntime` de r500** — toujours non vérifié,
  aucune tentative n'ayant atteint l'arrêt propre ce cycle non plus.

## Décisions prises

- Respecter strictement la règle VRAM ≥4 Go posée par l'utilisateur
  — vérifiée avant chaque tentative, jamais sautée ce cycle (VRAM
  toujours confortable).
- Plafonner à 3 tentatives, conforme aux instructions.
- Ne PAS fusionner les 3 nuanceurs capturés — aucun des 3 cibles.
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/`.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts, inchangés. `ctest` natif non relancé (`native/` non
touché). `git status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé avant
et après ce cycle.

## Named for r505

**Huit cycles de capture (r492, r494, r495, r496, r498, r501, r503,
r504), ~19 tentatives individuelles, toujours aucune des 3 cibles de
r478 capturée.** Ce cycle ferme l'hypothèse VRAM pour le blocage
`sleep(4)` spécifiquement (VRAM confortable, blocage identique quand
même) — cette cause précise reste ouverte et non expliquée. Candidat
le plus direct et jamais tenté : lecture Ghidra statique de ce qui se
passe côté invité pendant la fenêtre `sleep(4)` de 4s après chaque
pulsation de touche, plutôt qu'une nouvelle tentative de capture
empirique.

## Files

Committé : ce rapport, `NEXT.md`. Scratch
(`/fastdata/lavaulta/tmp/r504/`) supprimé après extraction des
données, non conservé. Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié.
