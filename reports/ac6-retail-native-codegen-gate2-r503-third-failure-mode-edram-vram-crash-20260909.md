# AC6 retail NTSC-U/J — r503 — septième cycle de capture (3 tentatives) : deux blocages `sleep(4)` identiques + un NOUVEAU mode d'échec, crash `SIGABRT` par échec d'allocation du tampon EDRAM, cohérent avec l'épuisement mesuré de la VRAM

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (3 tentatives de capture directe).

## Contexte

Suite à r502 (correctif du nettoyage `SIGTERM` ré-entrant). Aucune
contrainte de charge en vigueur (levée à r498) ; seule la RAM
disponible est vérifiée (`free -m`, 103 Go disponibles avant ce
cycle — non limitant).

## Établi — deux tentatives (1, 2b) bloquées au MÊME point exact que r498/r501

`timeout 240` sur `routes/us-pretype28-startup.steps` : les deux
tentatives expirent dans `tools/ac6-oracle-run.py::sleep()`, appelée
depuis `wait_log()` (ligne 535, `self.sleep(4)`), exactement comme
r498 et r501. Nettoyage vérifié propre après chaque timeout (aucun
processus résiduel — le correctif `SIGTERM` de r502 fonctionne
correctement en usage réel, deux fois de suite).

**Fait notable** : ce point de blocage identique sur 2/3 tentatives
(et déjà observé par r498 et r501 avant) commence à ressembler à un
point de blocage QUASI-SYSTÉMATIQUE plutôt qu'à une variance
aléatoire — non confirmé statistiquement (échantillon encore petit),
mais le motif se répète trop souvent pour être ignoré.

## Établi — NOUVEAU mode d'échec (tentative 3) : crash `SIGABRT` par échec d'allocation Vulkan, pas un timeout

La tentative 3 (`--display :304`) s'est terminée en 4/8 étapes avec
`error="runtime exited early with status -6"` (signal `SIGABRT`) —
**jamais observé dans les 7 cycles précédents de cette campagne**
(r492-r501, tous des timeouts ou des arrêts non propres, jamais un
crash direct). Lecture directe de `ac6recomp.log` :

```
[error] [gpu] VulkanRenderTargetCache: Failed to create the EDRAM buffer
[error] [gpu] Failed to initialize the render target cache
```

Ceci survient juste après une initialisation Vulkan par ailleurs
réussie (le GPU `NVIDIA RTX PRO 4000 Blackwell` est détecté, les
capacités du périphérique sont énumérées normalement, le système GPU
et les threads XThread s'initialisent avec succès).

**Corrélation directe avec la VRAM mesurée au moment du crash** :
`nvidia-smi` immédiatement après montre **16075 MiB / 24467 MiB**
déjà utilisés par UN SEUL processus externe
(`/fastdata/lavaulta/neural_amp/.venv/bin/python
scripts/run_teacher_v10_confirmation.py`, actif en continu depuis le
7 septembre selon les cycles précédents de cette campagne) — ne
laissant que ~8,4 Go de VRAM libre. r497 avait déjà mesuré une
utilisation GPU élevée par des jobs CUDA/PyTorch externes sans jamais
observer cet échec précis d'allocation ; c'est la première fois
qu'un lien direct et concret (message d'erreur exact + mesure VRAM
immédiate) est établi entre la pression GPU et un échec observable,
plutôt qu'une simple corrélation temporelle de charge.

## Non établi

- **Si le point de blocage `sleep(4)` a la même cause que le crash
  EDRAM** (contention VRAM se manifestant différemment selon le
  moment exact où elle survient) — plausible mais non démontré ; les
  deux tentatives `sleep(4)` de ce cycle n'ont pas atteint
  l'initialisation Vulkan assez tôt dans leur journal pour comparer
  (`ac6recomp.log` vide/non exploitable avant le timeout, motif déjà
  noté par r479/r480).
- **Le seuil de VRAM libre en dessous duquel ce crash EDRAM se
  déclenche systématiquement** — un seul cas observé, pas assez pour
  caractériser un seuil.
- **Si nettoyer un peu de VRAM externe changerait quoi que ce soit**
  — non testé (n'implique pas d'arrêter le job d'un tiers sans
  autorisation).

## Décisions prises

- Documenter les deux modes d'échec séparément plutôt que de les
  fusionner en une seule catégorie « échec » — ce sont des symptômes
  distincts (blocage temporel vs. crash d'allocation) qui pourraient
  avoir des causes distinctes.
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/` —
  aucune donnée fusionnable obtenue (3 nuanceurs uniques capturés
  par la tentative 2b, tous déjà connus depuis r479, aucun des 3
  états cibles de r478).
- Ne pas tenter de réduire la VRAM utilisée par le job externe — ce
  n'est pas un processus de cette campagne, décision hors périmètre.
- Nettoyer les sockets X11 fantômes trouvés en cours de cycle
  (`/tmp/.X11-unix/X192`, `X231`, `X233`, `X235`, `X261`, `X302` —
  aucun processus Xvfb vivant associé, bloquaient la réutilisation de
  ces numéros d'affichage) — nettoyage d'hygiène, sans rapport avec
  la cause des échecs eux-mêmes.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché). `git
status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé avant
et après ce cycle (mêmes 7 fichiers de la session concurrente).

## Named for r504

Deux pistes concrètes, plus solides que les précédentes :
1. **Le crash EDRAM/VRAM** — pourrait être reproductible en
   observant `nvidia-smi` juste avant chaque lancement futur et en
   corrélant directement avec succès/échec (r497 mesurait pendant
   l'exécution, pas au moment précis du crash). Si confirmé
   systématique sous forte pression VRAM, ce serait la première
   cause concrète et actionnable trouvée dans cette chaîne de 7
   cycles.
2. Le blocage `sleep(4)` répété (3/9 tentatives sur les 3 derniers
   cycles r498/r501/r503) mérite une lecture directe du code source
   à ce point précis plutôt que d'autres tentatives aveugles — jamais
   fait malgré sa récurrence.

Les 3 états cibles de r478 restent non capturés après 7 cycles/~18
tentatives (r492-r498, r501, r503).

## Files

Aucun fichier committé sous `native/`. Committé : ce rapport,
`NEXT.md`. Scratch (`/fastdata/lavaulta/tmp/r503-*`) nettoyé, non
conservé.
