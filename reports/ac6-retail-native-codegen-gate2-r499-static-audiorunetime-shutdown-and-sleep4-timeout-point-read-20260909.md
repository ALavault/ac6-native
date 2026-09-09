# AC6 retail NTSC-U/J — r499 — lecture statique de deux pistes de r495/r498 : l'arrêt `AudioRuntime` n'est pas un verrou mort mais une fenêtre de préemption grossière (un seul point de contrôle par tour de boucle) ; le `sleep(4)` de r498 est un délai de réglage documenté, sans rapport de cause avec le blocage audio

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle lancé ce cycle (lecture de code statique uniquement, sur
décision utilisateur explicite : arrêter les tentatives de capture et
investiguer les deux pistes statiques nommées par r495/r498).

## Contexte

Nommé par r498 (piste 2 et 3), lui-même reprenant r495 (piste 1) :
cinq cycles de capture (r492, r494, r495, r496, r498), ~12 tentatives,
aucune donnée fusionnable pour les 3 états cibles de r478. Deux pistes
statiques jamais suivies :
1. Le blocage `AudioRuntime: worker thread did not exit within 2s,
   terminating` (r495, une tentative par ailleurs réussie — 8/8
   étapes, 22 nuanceurs — mais `clean_shutdown=False`).
2. Le point exact où la tentative 3 de r498 a expiré, en plein
   `sleep(4)` (`tools/ac6-oracle-run.py:535` selon r498, vérifié
   ci-dessous).

## Établi — le `sleep(4)` de r498 est un délai de réglage documenté, sans lien de cause avec le blocage audio

Lu directement `tools/ac6-oracle-run.py`, méthode `wait_log()`
(lignes ~508-546). Le code : après chaque pulsation de touche
(`input_edge`), un commentaire explicite justifie le délai :

```
# Guest transitions may complete nearly two seconds after
# an edge, then need one flush interval to become visible.
# Settle before the next key so a late pulse cannot accept
# the following dialog.
self.sleep(4)
```

`self.sleep()` (ligne ~454) boucle en dormant par tranches de 0,2s et
appelle `require_time()` à chaque tranche — qui lève `RunError` dès
que `time.monotonic() >= self.deadline` (le délai global de la route,
pas un délai propre à cette ligne). Le timeout mesuré par r498 pendant
ce `sleep(4)` précis ne révèle donc rien de spécifique à cette étape :
**c'est simplement l'endroit où l'horloge globale a expiré**, parce
que l'exécution était déjà trop lente pour atteindre `type28=30` dans
la fenêtre impartie — cohérent avec un ralentissement général
(contention), pas un bug localisé à ce `sleep`. Aucune anomalie de
code trouvée à cet endroit.

## Établi — le blocage `AudioRuntime` : PAS un verrou mort, mais une fenêtre de préemption grossière (un seul point de contrôle par tour de boucle externe)

Lu directement
`upstream/AC6_recomp/thirdparty/rexglue-sdk/src/native/audio/audio_runtime.cpp`.

**`Shutdown()` (lignes 346-385)** fait exactement ce qu'il faut :
sous verrou, positionne `worker_running_.store(false, ...)`, signale
`shutdown_event_->Set()` ET `worker_wake_event_->Set()`, puis attend
`rex::thread::Wait(worker_thread_.get(), false,
std::chrono::milliseconds(2000))`. Si timeout, log l'avertissement et
appelle `worker_thread_->Terminate(0)` (arrêt forcé, pas un blocage
permanent de l'application — l'avertissement n'empêche pas la suite
de l'arrêt).

**`WorkerThreadMain()` (lignes 802-935+)** : la boucle externe
`while (worker_running_.load(...))` attend au maximum 5 ms
(`WaitAny(wait_handles, 2, true, std::chrono::milliseconds(5))`) puis
revérifie `worker_running_` **une seule fois, immédiatement après ce
wait** (ligne 816-818). Ensuite, elle itère sur
`clients_` (`std::array<AudioClientState, kMaximumAudioClientCount>`,
`kMaximumAudioClientCount = 8`,
`include/native/audio/audio_client.h:17`), et pour CHAQUE client actif
exécute une boucle interne `while (true)` qui peut dispatcher
plusieurs rappels audio invités via
`function_dispatcher_->Execute(worker_thread_context_->thread_state(),
client_callback, args, ...)` (ligne ~929) — **une exécution réelle de
code invité**, pas un simple flag. **`worker_running_` n'est PAS
revérifié entre chaque client ni entre chaque round de dispatch** —
seulement au sommet du tour de boucle externe suivant.

**Conséquence directe** : si, au moment où `Shutdown()` signale
l'arrêt, le thread est engagé dans un tour de boucle où plusieurs
clients (jusqu'à 8) doivent chacun dispatcher un ou plusieurs rappels,
et que ces rappels invités sont lents à s'exécuter (contention hôte
CPU/GPU déjà mesurée par r489/r497, qui peut ralentir n'importe quelle
exécution de code invité via le dispatcher de fonctions), le thread ne
revient au sommet de la boucle — donc ne revérifie `worker_running_`
— qu'après avoir fini ce tour complet. Un tour suffisamment lent
(plusieurs clients actifs × rappels lents) peut dépasser les 2000 ms
codés en dur, sans qu'aucun verrou mort ne soit en cause : ni deadlock
ni bug de synchronisation classique, mais une granularité de
préemption grossière (un seul point de contrôle par tour, pas par
client ni par rappel).

## Établi — les deux pistes sont INDÉPENDANTES, pas une cause commune

Le `sleep(4)` de r498 se produit **avant** que `type28=30` soit
atteint (démarrage précoce, avant tout dispatch audio significatif —
r473-r475 ont déjà caractérisé cette phase comme dominée par des
tirages UI 1-sommet, pas du contenu audio actif). Le blocage
`AudioRuntime` de r495 se produit **après** un succès complet (8/8
étapes, cible atteinte), pendant l'arrêt propre qui suit. Ce sont deux
phases temporellement disjointes de l'exécution — aucune donnée ne
suggère une cause commune.

## Non établi

- **Si le blocage `AudioRuntime` se reproduit de façon fiable sous
  contention, ou seulement occasionnellement** — un seul cas observé
  (r495), pas assez pour caractériser un taux. Nécessiterait plusieurs
  captures réussies (8/8 étapes) pour comparer.
- **Le nombre réel de clients actifs et de rappels en attente au
  moment précis de l'arrêt observé par r495** — non mesuré (aucune
  instrumentation de trace n'a été ajoutée ce cycle, lecture de code
  seule).
- **Si porter `worker_running_` à une granularité de vérification plus
  fine (entre chaque client, ou avec un budget de temps par tour)
  résoudrait cela de façon fiable** — plausible d'après la lecture,
  mais non vérifié par un test ; modifier ce fichier reviendrait à
  modifier le sous-module `AC6_recomp` (`upstream/AC6_recomp/...`),
  une décision distincte du simple ajustement de `native/`, hors
  périmètre de ce cycle de lecture statique.

## Décisions prises

- Ne PAS modifier `upstream/AC6_recomp/...` ni
  `recompilation/ace-combat-6-retail/native/` ce cycle — la piste
  `AudioRuntime` touche un sous-module git, dont la modification a
  toujours été traitée comme une décision distincte dans cette
  campagne (voir le committage du sous-module en r463), pas un
  correctif à faire sans décision explicite.
- Ne PAS relancer de capture oracle ce cycle — conforme à la décision
  utilisateur reçue avant ce cycle.
- Documenter les deux pistes comme résolues **au niveau de la
  compréhension** (le mécanisme exact est maintenant lu et compris)
  mais PAS corrigées — la décision de corriger ou non revient à la
  session parente / l'utilisateur.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts, inchangés. `ctest` natif non relancé (`native/` non
touché). `git status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé avant et
après ce cycle. Aucun fichier sous `upstream/AC6_recomp/` modifié.

## Named for r500

**Piste `AudioRuntime`** : si une correction est décidée, la plus
directe et la moins risquée serait de revérifier `worker_running_` à
l'intérieur de la boucle `for (clients_)` (entre chaque client, pas
seulement au sommet du tour externe) — un changement d'une ligne,
citation-grounded (lu directement ci-dessus), mais touche le
sous-module `AC6_recomp` et nécessite donc une décision utilisateur
explicite avant tout committage, même raisonnement que r463.

**Piste `sleep(4)`** : refermée — comportement attendu, documenté,
sans anomalie trouvée. Rien à corriger.

**Reste ouvert** : les 3 états cibles de r478, toujours non capturés.
Six pistes de contention/timing ont maintenant été explorées (charge
CPU r489, contention GPU r497, RAM r498, `sleep(4)` et `AudioRuntime`
r499) sans identifier de cause unique et corrigible qui expliquerait
la totalité de la variance observée depuis r479. Une décision
utilisateur sur la suite (nouvelle tentative de capture, correctif
`AudioRuntime` sous réserve d'accord, ou pause de Piste A) reste à
prendre.

## Files

Committé : ce rapport, `NEXT.md`. Aucun fichier sous `native/` ni
`upstream/AC6_recomp/` modifié.
