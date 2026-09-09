# AC6 retail NTSC-U/J — r508 — échantillonnage direct des threads OS pendant le blocage confirmé : `Main XThread` est activement occupé, pas parqué — un livelock, pas un deadlock analogue à r301

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (lancement direct de `install/ntsc-uj/bin/ac6recomp`
via `run_gate.py`, échantillonnage `/proc` pendant l'exécution — pas
de `gdb`, technique identique à r301).

## Contexte

Nommé par r507 : reproduire la technique d'échantillonnage OS
`/proc/$PID/task/*/{stat,wchan}` de r301 (utilisée avec succès côté
`native` pour trouver un thread réellement parqué dans
`futex_do_wait` par un deadlock réel) sur un lancement
`rexglue-oracle` en plein blocage, pour voir si un thread analogue
existe côté oracle.

## Établi — VRAM vérifiée, capture lancée, blocage confirmé au même point

`nvidia-smi` : 7911 MiB libres avant lancement (règle utilisateur ≥4
Go respectée). `python3 tools/run_gate.py --diagnostic-route
routes/us-pretype28-startup.steps --mission-dump-shaders` lancé sous
`timeout 350`. Le PID réel du binaire (`ac6recomp`, distinct du PID
du wrapper Python) confirmé actif après 1s ; `RESULT.json` final
(après arrêt manuel) : `steps=7/8` — bloqué exactement à l'étape
`wait-pulse type28=30`, comme tous les cycles précédents (r498,
r501, r503, r504, r506).

## Établi — deux échantillons de tous les 68 threads, à 5s d'intervalle, pendant le blocage

Champ 3 de `/proc/<pid>/task/<tid>/stat` (état) et
`/proc/<pid>/task/<tid>/wchan` lus directement (analyse correcte du
format `pid (comm) state ...` — `comm` peut contenir espaces et
parenthèses, la découpe naïve par espace donne un résultat corrompu,
corrigé en découpant après la dernière `)`).

- **`Main XThread` (tid 2445669)** : état `R` aux DEUX échantillons,
  `wchan=0` (jamais bloqué dans le noyau) aux deux, `utime` **3414 →
  4266** (+852 jiffies sur 5s, soit ~85% d'un cœur) — **authentiquement
  occupé, PAS parqué**. C'est le thread guest principal.
- **`Audio Worker` (tid 2445647)** : même profil, `R`→`S`→`utime`
  2774→3511, activité réelle continue (cohérent avec r499 : le
  dispatcher audio exécute du vrai code invité).
- **~50 threads `XThreadNNNNNNNN` (pool de threads ouvriers invités,
  nommés par leur adresse de contexte)** : état `S`,
  `wchan=futex_do_wait` STABLE aux deux échantillons,
  `utime` quasi nul et inchangé — **authentiquement parqués, mais sur
  un pool ouvrier légitimement inactif** (pas de travail assigné),
  pas un signal d'achèvement jamais délivré comme dans le cas r301.
- Threads utilitaires (`gmain`, `gdbus`, `pool-spawner`, `[pango]
  fontcon`, `Vulkan Pipeline`/`Analysis`/`Storage` ×~25) : tous `S`,
  `wchan` stable, activité nulle — infrastructure SDL/Vulkan/glib
  normale, sans rapport.

**Aucun thread ne correspond au motif r301** (parqué dans
`futex_do_wait` en ATTENTE D'UN SIGNAL QUI NE VIENDRA JAMAIS d'un
autre thread qui a besoin du même verrou). Le seul thread qui compte
réellement (`Main XThread`) n'est PAS parqué — il est actif en
continu.

## Non établi

- **Ce que fait exactement `Main XThread` pendant ces 5s d'activité
  continue** — non tracé au niveau instruction (aurait nécessité un
  attachement `gdb`, déjà documenté comme problématique sur ce
  produit par r467 — `ptrace_scope`, SIGSEGV visible seulement sous
  débogueur). L'échantillonnage OS seul ne peut pas dire QUOI le
  thread calcule, seulement QU'IL calcule.
- **Si ce livelock est le même mécanisme que celui déjà nommé par
  r506** (thread occupé en sondage continu dans l'attente « movie
  worker », confirmé non bloquant par r487/r488) ou un mécanisme
  DIFFÉRENT et plus général touchant le thread principal lui-même —
  plausible que ce soit le même (le thread principal EST celui qui
  contient la logique de sondage `movie worker`), mais non confirmé
  par une lecture croisée directe de la pile d'appel à cet instant.

## Décisions prises

- Ne PAS tenter `gdb` — coût/risque déjà documenté par r467 pour ce
  produit précis, hors périmètre d'un cycle d'échantillonnage OS.
- Nettoyer activement (`pkill -9`) plutôt que d'attendre l'expiration
  du `timeout 350` — cohérent avec la discipline établie.
- Documenter ce résultat négatif (pas de deadlock analogue à r301)
  comme une conclusion utile en soi : elle referme la piste « chercher
  un deadlock à la r301 » et pointe vers un livelock du thread
  principal comme cause la plus probable, distincte de ce que r301
  avait trouvé côté natif.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché,
vérifié inchangé avant/après — mêmes 7 fichiers que les cycles
précédents). Aucun processus résiduel après nettoyage
(`pgrep -af "ac6recomp|Xvfb"` vide, hors le shell de vérification
lui-même).

## Named for r509

Le blocage est un **livelock du thread principal invité**, pas un
deadlock classique à la r301. Reste ouvert : tracer précisément ce
que `Main XThread` calcule en boucle (nécessiterait soit un
`gdb`/`strace -p` malgré les risques déjà documentés par r467, soit
une instrumentation de trace supplémentaire dans le binaire — hors
périmètre d'un cycle d'échantillonnage OS pur). Les 3 états cibles de
r478 restent non capturés après 8 cycles/~20 tentatives
(r492-r506).

## Files

Committé : ce rapport, `NEXT.md`. Scratch
(`/fastdata/lavaulta/tmp/r508-stall/`) non conservé.
