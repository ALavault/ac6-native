# AC6 retail NTSC-U/J — r497 — instrumentation au-delà de la charge CPU révèle une contention GPU réelle et non mesurée jusqu'ici ; la charge CPU seule (r489) n'explique pas le motif d'échec observé par r496

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (un lancement réel, instrumenté, pas pour capturer les
cibles mais pour mesurer la contention pendant l'exécution — décision
utilisateur explicite : mesurer plutôt que retenter à l'aveugle).

## Contexte

Nommé par r496 : quatre cycles consécutifs (r492, r494, r495, r496,
huit tentatives) ont échoué à capturer les 3 états cibles de r478.
r489 avait corrélé les échecs avec la charge CPU (`load average`
40-52). r496 a échoué ENCORE PLUS TÔT que les cycles précédents sous
la charge CPU la PLUS BASSE mesurée (31-35) — contredisant directement
l'hypothèse de corrélation simple par charge CPU. L'utilisateur a
choisi : mesurer la ressource réellement contestée plutôt que
deviner ou retenter à l'aveugle.

## Établi — le GPU est sollicité en continu par des tâches sans rapport, dès avant le lancement d'`ac6recomp`

Avant même le lancement de ce cycle, `nvidia-smi` montrait déjà
**61 % d'utilisation GPU et 16073/24467 MiB de VRAM utilisée**
(1 seul GPU sur cette machine, `NVIDIA RTX PRO 4000 Blackwell`) —
causé par les deux jobs déjà identifiés par r489
(`generated_image_extract_frozen_features.py`,
`.venvs/blackwell_torch_cu128`, un environnement CUDA/PyTorch) plus
un troisième job GPU non vu par r489
(`run_teacher_v10_confirmation.py`, `/fastdata/lavaulta/neural_amp`).
**r489 n'avait mesuré QUE la charge CPU** (`uptime`) — jamais le GPU,
alors que `ac6recomp` utilise Vulkan pour son rendu et partage donc le
même périphérique physique.

## Établi — l'utilisation GPU oscille violemment pendant l'exécution, avec des pointes à 100 % juste avant l'échec

40 échantillons pris toutes les 5 s pendant un lancement réel
(`--diagnostic-route routes/us-pretype28-startup.steps
--mission-dump-shaders`, `--display :281`) : l'utilisation GPU
oscille sans motif stable entre 15 % et 100 % tout au long de
l'exécution (valeurs observées : 16, 42, 30, 55, 15, 69, 57, 26, 85,
60, 50, 62, 87, 87, 33, 85, 57, 62, 23, 54, 40, 40, 60, 81, 61, 58,
79, 33, 43, 36, 34, 46, 49, 58, **100**, **90**, 30, 26, 50). Deux
pointes à 90-100 % surviennent dans les 25 dernières secondes avant
l'échec par timeout (240 s). La charge CPU (`load average` 1 min) a
elle-même grimpé de façon continue pendant l'exécution (28,20 → 40,22)
— en partie causée par `ac6recomp` lui-même, comportement attendu,
mais cohérent avec une contention croissante plutôt qu'un état stable.

## Établi — la swap est active et croît lentement pendant l'exécution

`free -m` avant lancement : 10 696 Mio de swap déjà utilisés (sur
65 535 Mio) ; croissance continue et régulière à 10 935 Mio au moment
de l'échec (+239 Mio sur ~215 s). `vmstat` montre `si`/`so` à 0 dans
les échantillons pris (pas de swap-in/out actif au moment précis des
échantillons), mais la croissance nette du swap utilisé démontre une
pression mémoire réelle et progressive, jamais mesurée par r489 (qui
n'avait relevé que `ps aux --sort=-%cpu`).

## Non établi

- **Causalité directe entre une pointe GPU précise et l'échec de ce
  cycle précis** — corrélation temporelle observée (deux pointes
  90-100 % dans les 25 s précédant le timeout), pas une preuve de
  cause. Une seule exécution instrumentée ; pas de répétition pour
  confirmer le motif statistiquement.
- **Si la contention GPU (plutôt que la charge CPU) explique
  également les échecs plus précoces de r496** (chargé CPU le plus
  bas, échec le plus rapide) — plausible étant donné que le GPU était
  déjà à 61 % avant même le lancement d'r497, mais r496 n'a pas
  mesuré le GPU à l'époque, donc aucune comparaison directe n'est
  possible.
- **Le rôle de `sde`/`/bigdata2`** (89,8 % d'utilisation disque
  mesuré par `iostat` avant le lancement) — écarté comme piste
  directe : ce périphérique héberge `/bigdata2`, sans rapport avec les
  chemins utilisés par `ac6recomp` (ISO et scratch sous `/fastdata`,
  `sdd1`, à 0 % d'utilisation dans le même relevé). Contention
  possible au niveau du contrôleur de stockage partagé, non mesurée,
  hors périmètre de ce cycle.

## Décisions prises

- Ne PAS forcer de fusion — cette exécution était instrumentée pour
  le diagnostic, pas pour capturer les 3 cibles ; elle a échoué par
  timeout avant même de produire un dump de nuanceurs utile (`RESULT.json`
  absent, journal `run_gate.py` interrompu par le `KeyboardInterrupt`
  du gestionnaire SIGTERM de r494 — comportement correct et attendu,
  **zéro processus résiduel** vérifié après coup, confirmant que le
  correctif de r493/r494 continue de fonctionner).
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/` — aucune
  donnée à fusionner ce cycle.
- Ne pas relancer une deuxième exécution instrumentée ce cycle
  (objectif de diagnostic déjà atteint avec un jeu de données réel et
  concret ; une répétition supplémentaire n'aurait pas changé la
  conclusion qualitative : le GPU ET la mémoire sont des dimensions de
  contention réelles jamais mesurées par r489, indépendamment de
  combien d'échantillons supplémentaires seraient pris).

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché).
`git status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé avant et
après ce cycle (mêmes 7 fichiers modifiés par l'autre session).

## Named for r498

**La charge CPU seule (r489) est maintenant établie comme une mesure
insuffisante** — le GPU (partagé avec au moins 3 jobs CUDA/PyTorch
externes, déjà à 61 % avant tout lancement) et la pression mémoire
(swap en croissance continue) sont deux dimensions réelles,
mesurables, jamais suivies avant ce cycle. Candidats pour la suite :
1. Répéter cette instrumentation sur 2-3 exécutions supplémentaires
   pour établir une vraie corrélation statistique GPU-échec plutôt
   qu'une observation ponctuelle.
2. Vérifier si l'utilisation GPU baisse à un moment où les jobs
   `blackwell_torch_cu128`/`neural_amp` sont eux-mêmes en pause
   (aucun moyen direct de le savoir sans les arrêter, ce qui serait
   interférer avec le travail d'un autre — hors périmètre, à ne pas
   faire sans autorisation explicite).
3. Toujours ouvert, indépendant : le bug `AudioRuntime` de r495
   (non réinvestigué ce cycle — l'exécution de ce cycle n'a jamais
   atteint la fin de la route).

## Files

Committé : ce rapport, `NEXT.md`. Scratch (`/fastdata/lavaulta/tmp/r497-capture/`,
`console.log`) supprimé après extraction des données ci-dessus, non
conservé. Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié.
