# Cycle 1644 — analyse runtime Xenia AC6 et gel POSIX

## Vérification de l’archive

Source lue sans modification :
`/fastdata/lavaulta/autoo-re-agent/workspaces/ace-combat-6/xenia-runtime-results-20260815-153556.tar.zst`.
Le README est vérifié en premier (`README.md`, lignes 1–52) et le SHA-256 de
l’archive est `094148a4…a891ea` pour 1 145 602 octets. L’extraction de travail
est restée sous `/fastdata/lavaulta/tmp/ac6-xenia-runtime-analysis.*`.

Les sept manifests ont été lus. Pour les six manifests contenant une table de
tailles, les quatre fichiers effectivement sélectionnés par run
(`runtime-events.bin`, `semantic-events.jsonl`, `xenia-debug.log` et la config)
correspondent aux tailles déclarées. Le hash de configuration recopiée
correspond au manifest pour six sessions; `150700` est une exception de
repackaging (TOML sémantiquement équivalent, mais format/commentaires/ordre
différents), explicitement non promue comme byte-identique. Les caches shader,
`controller.jsonl` et
les autres sorties originales ne sont pas dans l’archive; leurs absences sont
un choix de sélection, pas une preuve de non-production. Les trois sommes
source (jeu, Xenia, configuration) sont des valeurs enregistrées dans les
manifests; seuls les fichiers de configuration copiés ont été re-hashés et
correspondent à ces valeurs.

Le reçu structuré est
[`ac6-demo-xenia-runtime-wait-analysis-v1.json`](../analysis/demo/ac6-demo-xenia-runtime-wait-analysis-v1.json).

## Comparaison contrôlée

| session | variante | watchdog | événements binaires |
|---|---|---|---|
| `145806` | native baseline | 62/62 : threads 6 et 17, `0xF8000088→0xF800008C` | 16 créations, 1 swap, 0 audio |
| `150700` | référence README sous `strace` | thread 17 seul dans la paire (156 snapshots), 2 snapshots vides | 23 créations, 3 078 swaps, 14 683 audio |
| `151137` | `posix_duplicate_signal_and_wait_bypass=true` | 338/338 : 6 et 17 bloqués | 16 créations, 1 swap, 0 audio |
| `151937` | timeout principal 1 ms | 68/68 : 6 et 17 bloqués | 16 créations, 1 swap, 0 audio |
| `152122` | timeout principal 16 ms | thread 6 `0xF8000074→78`, thread 17 `0xF8000088→8C` | 16 créations, 1 swap, 0 audio |
| `152734` | expérience timeout retirée | 59/59 : paire originale | 16 créations, 1 swap, 0 audio |
| `153303` | gel + backtraces GDB | 402/402 : paire originale | 16 créations, 1 swap, 0 audio |

Les données proviennent des `runtime-events.bin` décodés par
`tools/analyser-trace-runtime.py` et des lignes `watchdog_snapshot` de chaque
`runtime/semantic-events.jsonl`. Le README qualifie `150700` de run positif;
les 3 078 swaps et 14 683 soumissions audio en donnent une corroboration
dynamique, sans constituer à eux seuls un jalon gameplay pixelisé.

## Chronologie factuelle

1. Les créations de threads partent du thread invité 6, à `0x821A8CFC/0x821A8D00`;
   le run positif en compte 23 (`runtime-events.bin`).
2. Chaque run atteint un premier `frame_swap` à `0x821C5A1C/0x821C5A20`, avec
   les dwords bruts `BA9A0000, 1A9A0000, 0500, 02D0, 0006`. Les deux dwords
   centraux sont compatibles avec 1280×720, mais l’archive ne donne pas l’ABI
   nommée de cet événement.
3. Dans les runs natifs, le watchdog voit ensuite les threads 6 et 17 dans
   `NtSignalAndWaitForSingleObjectEx` au caller `0x821A69C8/0x821A69CC`, signal
   `0xF8000088`, attente `0xF800008C` (ex. `145806` lignes 1 et 62).
4. Dans `150700`, le thread 17 reste dans cette paire mais le thread 6 produit
   52 562 `kernel_wait_multiple` (26 281 entrées et sorties), puis des
   snapshots de PC actifs tels que `0x820D29E0`, `0x821BB078` et `0x821E5430`.
5. Les accès contenu du run positif commencent par `DATA.TBL`, `DATA00.PAC` et
   `DATA01.PAC` (lignes 2–6), puis `bgmpack.bin` (115), `demopack_eng.bin`
   (1043) et `voicepack_eng.bin` (16421). Les lectures sont émises par
   `0x821A6264/0x821A6268`; les ouvertures par `0x821A5FE8/0x821A5FEC`.
6. Le même run produit les soumissions audio à `0x8234D360/0x8234D364`, avec
   les arguments bruts `0, 0x40020700, 0, 0, 0`, et continue les swaps jusqu’au
   tick binaire `4 132 281 468`.
7. Les variantes bypass/timeout ne dépassent pas le premier swap. Le timeout
   16 ms déplace seulement la paire du thread 6; la paire du thread 17 reste
   inchangée (`152122`, ligne 1 et 119).

## Cartographie thread → primitive

| thread invité | PC/LR observés | primitive/handles | statut |
|---:|---|---|---|
| 17 | `0x821A69C8/0x821A69CC` | `SignalAndWait`, `F8000088→F800008C` | qualifié dans les snapshots |
| 6 | mêmes PC en gel | même paire; en référence `0x821A6A78/0x821A6A7C`, handles `F8000010/F800001C/F8000028/F8000034` | qualifié, sortie active seulement sous la référence |
| 4 | `0x8234D360/0x8234D364` | `audio_submit`, second argument `0x40020700` | observé uniquement dans la référence |
| 23/27 | `0x821A5FE8/0x821A5FEC`, puis `0x821A6264/0x821A6268` | ouvertures/lectures de contenu | observé dans la référence |

Le log Xenia expose les thunks import utiles : `NtSignalAndWaitForSingleObjectEx`
`0x82375D84`, `NtWaitForMultipleObjectsEx` `0x82375D94`,
`NtWaitForSingleObjectEx` `0x82375CF4`, `KeWaitForSingleObject`
`0x823760E4`, `VdSwap` `0x82376114` et
`XAudioSubmitRenderDriverFrame` `0x82376704` (par exemple
`runs/20260815-150700/xenia-debug.log`, lignes 849–982).

## Join hôte et hypothèses

Le backtrace GDB (`runs/20260815-153303/host-backtraces.txt`) montre pour deux
XThreads les frames `pthread_cond_wait → PosixConditionBase::Wait →
XObject::SignalAndWait → NtSignalAndWaitForSingleObjectEx` (lignes 421–427 et
583–586). D’autres XThreads sont dans `XObject::Wait → NtWaitForSingleObjectEx`
(lignes 592–599). Le fichier ne contient pas l’identifiant invité dans la pile
hôte : la jointure avec 6/17 vient donc des watchdogs, pas d’un nommage GDB.

Faits compatibles avec le défaut de synchronisation : le gel natif est
reproductible, le bypass dédié et les timeouts 1/16 ms n’ouvrent pas le
corridor, et la perturbation d’ordonnancement du run README laisse le thread 6
actif. Hypothèses à tester, non implémentées : perte/ordre incorrect d’un
réveil auto-reset; suspension hôte pendant un wait ou une région critique;
cycle de vie/aliasing des handles. Le rendu et l’audio ne sont pas la cause
première démontrée : ils divergent surtout parce que le run positif progresse.

## Priorités de décompilation et instrumentation

1. Joindre `0x821A69C8`, `0x821A6A78` et les imports `0x82375D84/94/CF4` au
   projet Ghidra PAL canonique, puis qualifier les bytes et l’ABI, sans prendre
   les symboles Xenia pour des bornes AC6.
2. Activer une trace ciblée des opérations d’événements pour
   `F8000088/8C` et `F8000074/78` : create/set/clear/pulse, séquence, état
   signalé, nombre de waiters, thread/PC/LR invité et thread hôte. La config
   actuelle désactive `decomp_trace_event_operations` (config, ligne 94).
3. Corréler `XObject::SignalAndWait`, `XObject::Wait`, `KeSetEvent`,
   `NtSetEvent`, `NtPulseEvent`, `NtClearEvent`, `NtResumeThread` et les appels
   `XThread` suspend/resume avec un identifiant de hand-off; limiter la fenêtre
   au premier gel pour éviter une trace volumineuse.
4. Ajouter un compteur de scheduler/yield et les retours de
   `pthread_cond_wait`/`Signal` côté hôte; comparer une exécution native à la
   référence sous perturbation sans modifier la sémantique guest.
5. Pour le renderer, conserver `VdSwap` et les dwords bruts, puis joindre le
   dernier resolve et un readback guest-owned; aucun screenshot n’est promu par
   cette archive. Pour l’audio, journaliser packet XMA, timestamp, volume et
   route de décodage autour de `0x8234D360`.
6. Pour l’entrée, réintroduire dans une prochaine sélection un
   `controller.jsonl` borné et joindre chaque changement à un PC invité; son
   absence ici interdit toute conclusion sur la manette.

## Risques résiduels

Le run `strace` est une référence d’ordonnancement décrite par le README, pas
une preuve de parité. Les événements binaires ne contiennent ni fonctions
first-hit ni arêtes indirectes dans ces configurations, et le parseur fourni
ne résume pas automatiquement `kernel_wait_multiple` dans sa fonction
`summarize_semantic_events`. Les conclusions Xenia restent `xenia-generic` et
ne doivent pas être fusionnées avec les preuves PAL démo `de917873…5da8`.
