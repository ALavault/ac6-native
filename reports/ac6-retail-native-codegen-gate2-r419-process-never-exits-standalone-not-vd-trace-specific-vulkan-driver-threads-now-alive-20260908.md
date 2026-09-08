# AC6 retail NTSC-U/J — r419 — le symptôme d'arrêt lent de r418 N'EST PAS spécifique à `AC6_NATIVE_VD_TRACE` : le processus autonome ne se termine JAMAIS de lui-même après avoir imprimé son dernier diagnostic ; correction méthodologique importante ; des threads pilote Vulkan (`vkcf`/`vkrt`/`vkps`) sont désormais VIVANTS pour la première fois

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r418 (nommé pour r419) : isoler si le nouvel arrêt de
processus long est spécifique à `AC6_NATIVE_VD_TRACE=1`, avant de le
qualifier de blocage réel.

## Établi

### Le symptôme se reproduit SANS `AC6_NATIVE_VD_TRACE`, en exécution autonome (pas sous gdb)

Lancement direct (`build/ntsc-uj/native/native-cmake/ac6recomp`,
fenêtre `25000ms`, sans le drapeau de trace, `timeout 60` en
enveloppe) : le processus imprime bien `presented_frames=0 state=1`
puis `generated entry terminated its own thread` en quelques
secondes — MAIS **ne se termine jamais de lui-même ensuite**. `timeout
60` (SIGTERM après 60s) **n'a PAS arrêté le processus** — sans option
`-k`, `timeout` envoie `SIGTERM` une seule fois et attend
indéfiniment ; le processus a survécu à ce signal sans se terminer.
Observé vivant à `3:49` de temps CPU écoulé (`123%` CPU cumulé,
8 threads), terminé de force (`kill -9`) après constat. **Le
symptôme de r418 n'est donc PAS spécifique au drapeau de trace GPU —
c'est un comportement du binaire lui-même, en exécution autonome.**

### Correction méthodologique importante

**Toutes les vérifications "sortie normale" de r414 à r417 ont été
faites SOUS GDB** (`gdb -batch ... run ... [script] ... quit`). Le
`quit` de fin de script gdb **termine de force le processus inférieur
à la fin du script**, qu'il se soit réellement terminé de lui-même ou
non. Les messages `Inferior 1 (process ...) exited normally` observés
dans ces captures reflètent donc soit une vraie sortie normale, soit
— comme démontré ce cycle — un processus encore actif que gdb a
arrêté sans le signaler comme tel dans ces logs (le message exact n'a
pas été revérifié pour distinguer les deux cas). **Aucune des
vérifications r414-r417 ne prouve, à elle seule, que le processus se
termine de lui-même sans intervention externe.** C'est ce cycle qui
teste ce fait pour la première fois, avec un lancement autonome.

### Inventaire des fils au moment du blocage — des threads pilote Vulkan sont désormais vivants

`/proc/<pid>/task/*/status` et `/wchan`, un instantané : 8 fils, tous
`S (sleeping)` sur `futex_do_wait` (sauf un sur `hrtimer_nanosleep`),
dont **trois portant des noms de threads internes au pilote Vulkan** :
`[vkcf] Analysis`, `[vkrt] Analysis`, `[vkps] Update` — des pools de
threads internes typiques d'un pilote Vulkan (Mesa/RADV ou similaire)
pour la compilation de shaders et la mise à jour d'état. **Ceci
indique qu'un vrai périphérique Vulkan est désormais initialisé et
actif** — une observation nouvelle par rapport à toute capture
précédente de cette campagne, cohérente avec le boot qui progresse
désormais bien plus loin (r416/r417) et avec l'anneau GPU qui reste
actif au lieu de se taire (r418). Ceci n'a PAS été vérifié comme étant
absent des runs précédant le correctif de r414 (aucun binaire
non-corrigé n'a été relancé pour comparaison ce cycle).

## Ce que ceci établit

**Le blocage nommé par r418 est réel et confirmé, indépendant du
drapeau de trace.** L'hypothèse la plus directe, NON VÉRIFIÉE ce
cycle : le correctif de r414 fait maintenant progresser le boot assez
loin pour initialiser un vrai périphérique/instance Vulkan — un chemin
de code jamais exercé auparavant dans cette campagne — et la séquence
d'arrêt du runtime (`shutdown()`, dont le mécanisme
`native_guest_threads_stop_and_join()` couvre les threads INVITÉS via
`GuestThreadTerminated`, r277) ne couvre probablement PAS la
destruction propre du périphérique/instance Vulkan lui-même — les
threads internes du pilote (`vkcf`/`vkrt`/`vkps`) ne répondent à
aucune sollicitation invitée, ils répondent à `vkDestroyDevice`/
`vkDeviceWaitIdle`/`vkDestroyInstance`, dont l'appel (ou l'absence
d'appel) au moment de l'arrêt n'a pas été vérifié ce cycle. Ce serait
un défaut DIFFÉRENT de tout ce que r399-r418 ont documenté — un
défaut d'arrêt propre du backend Vulkan hôte, pas de l'allocateur ni
du chargement du jeu invité.

## Non établi

- **La cause exacte du blocage** — non tracée ligne à ligne. L'hypothèse
  Vulkan ci-dessus est plausible et cohérente avec les threads
  observés, mais n'est PAS vérifiée par lecture du code d'arrêt du
  backend Vulkan (`native/src/native_vulkan_backend.cpp`, jamais lu ce
  cycle) ni par une capture en direct de ce qui bloque précisément
  (l'attache `ptrace` a été refusée par la politique `yama.
  ptrace_scope` de ce bac à sable — pas de contournement tenté).
- **Si ce blocage existait AVANT le correctif de r414** — non
  vérifié, aucun binaire antérieur n'a été relancé en autonome pour
  comparaison. Étant donné que les threads Vulkan pilote ne sont
  vraisemblablement vivants QUE si le boot atteint l'initialisation
  Vulkan (jamais observée avant r414/r416), l'hypothèse la plus
  probable est que ce blocage n'était simplement jamais ATTEIGNABLE
  avant ce correctif — mais ce n'est pas une preuve directe.
- Aucun correctif proposé ni appliqué ce cycle.

## Décisions prises

- Terminer de force le processus après l'avoir laissé tourner
  suffisamment longtemps pour être certain qu'il ne se termine pas de
  lui-même (`3:49` de CPU cumulé, très au-delà de la fenêtre de sonde
  de `25s` et du `timeout 60`) plutôt que d'attendre indéfiniment —
  cohérent avec la discipline déjà appliquée en r418.
- Ne pas tenter de contourner la restriction `ptrace_scope` de ce bac
  à sable (ex. `sudo`, modification de sysctl) pour inspecter le
  processus plus en profondeur — hors du périmètre d'un cycle de
  documentation, et une modification système ne serait pas anodine
  sans consultation préalable.
- Nommer explicitement la lacune méthodologique des captures gdb
  r414-r417 plutôt que de la laisser implicite — corriger sa propre
  discipline de vérification dès qu'elle s'avère insuffisante
  (précédent CLAUDE.md : corriger prédécesseurs ET soi-même).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r420

Lire `native/src/native_vulkan_backend.cpp` (jamais lu dans cette
chaîne) pour localiser sa séquence de destruction
(`vkDeviceWaitIdle`/`vkDestroyDevice`/`vkDestroyInstance`) et
déterminer si `shutdown()` l'appelle réellement avant que `main()` ne
retourne. Si l'attache `gdb -p` reste bloquée par `ptrace_scope`,
envisager un point d'arrêt interne (compilé) ou un journal de trace
supplémentaire côté C++ plutôt qu'une inspection externe.

## Files

Aucun fichier gitignové créé ce cycle au-delà des logs `/tmp`
éphémères déjà nettoyés (processus terminé de force, pas de capture
gdb exploitable obtenue).
