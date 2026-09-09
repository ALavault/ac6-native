# AC6 retail NTSC-U/J — r500 — applique le correctif `AudioRuntime` identifié par r499 (décision utilisateur explicite), sous-module `AC6_recomp` mis à jour

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle lancé ce cycle (correctif appliqué sur la base de la
lecture de code de r499, pas de nouvelle capture).

## Contexte

r499 a lu directement `WorkerThreadMain()`
(`upstream/AC6_recomp/thirdparty/rexglue-sdk/src/native/audio/audio_runtime.cpp`)
et identifié précisément pourquoi `Shutdown()` peut dépasser son
attente de 2000 ms (observé une fois par r495) : `worker_running_`
n'est revérifié qu'une fois par tour de boucle externe, pas entre
chaque client (jusqu'à 8) dont les rappels audio invités sont
dispatchés via `function_dispatcher_`. r499 a délibérément NE PAS
appliqué le correctif (modification d'un sous-module git, décision
distincte par précédent de cette campagne — voir r463). Question posée
à l'utilisateur après r499 : appliquer le correctif d'une ligne
identifié, ou laisser le sous-module inchangé. **Réponse : appliquer.**

## Établi — correctif appliqué et committé dans le sous-module

Ajout d'une revérification de `worker_running_` au sommet de la boucle
`for (size_t i = 0; i < clients_.size(); ++i)` (juste avant le
traitement de chaque client), permettant à l'arrêt d'interrompre le
dispatch entre deux clients plutôt qu'uniquement entre deux tours
complets de la boucle externe. Diff minimal (10 lignes, dont 6 de
commentaire citant r499), aucune autre ligne touchée.

Committé dans le sous-module `AC6_recomp`
(`upstream/AC6_recomp`, commit `2e79f3f4`, identité git réutilisée via
variables d'environnement `GIT_AUTHOR_*`/`GIT_COMMITTER_*` — le
sous-module n'a pas d'identité git locale configurée, même procédure
que le commit du sous-module en r463 ; `git config` du dépôt lui-même
non modifié). Pointeur du sous-module mis à jour dans le dépôt parent.

## Non établi

- **Si ce correctif élimine réellement l'avertissement `AudioRuntime`
  de façon fiable** — non re-testé par une capture oracle ce cycle
  (décision utilisateur précédente de ne pas relancer de capture ce
  cycle, non révoquée par la question posée ici, qui portait
  uniquement sur le correctif). Une future capture réussie (8/8
  étapes) qui atteint l'arrêt propre permettrait de vérifier.
- **Si ce correctif a un effet sur les 3 états cibles de r478**
  (non capturés) — aucun rapport connu ; le blocage `AudioRuntime` se
  produit APRÈS un succès complet de route, une phase disjointe des
  échecs `sleep(4)`/`type28=30` documentés par r492-r498.

## Décisions prises

- Appliquer le correctif d'une ligne dans le sous-module, sur
  autorisation explicite de l'utilisateur, exactement comme r499 l'a
  spécifié (revérification entre clients, pas de refonte plus large).
- Ne PAS relancer de capture oracle ce cycle — la question posée ne
  portait que sur le correctif, pas sur une reprise de piste A.
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/` — hors
  périmètre, aucun rapport avec ce correctif.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché — ce
correctif concerne uniquement le sous-module `AC6_recomp`, consommé
par le profil `rexglue-oracle`, pas par le profil `native`). `git
status --porcelain` sous `recompilation/ace-combat-6-retail/native/`
confirmé inchangé.

## Named for r501

Le correctif `AudioRuntime` est en place mais non vérifié par une
capture réelle. Reste ouvert, sans reprise décidée : les 3 états
cibles de r478 toujours non capturés après 5 cycles/~12 tentatives
(r492-r498) sans cause unique identifiée malgré l'exploration de six
dimensions de contention/minutage (CPU r489, GPU r497, RAM r498,
`sleep(4)` et `AudioRuntime` r499-r500).

## Files

Committé : ce rapport (dépôt parent). Sous-module `AC6_recomp` :
`thirdparty/rexglue-sdk/src/native/audio/audio_runtime.cpp` (commit
`2e79f3f4`), pointeur mis à jour dans le dépôt parent. Aucun fichier
sous `recompilation/ace-combat-6-retail/native/` modifié.
