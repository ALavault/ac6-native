# AC6 retail NTSC-U/J — r431 — correctif appliqué et vérifié en direct : `presented_frames` reflète enfin la réalité (`5`), `state=2` (`kRunning`) — la chaîne r399-r431 est close, plus aucun blocage connu

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r430 (nommé pour r431) : reconnecter
`diagnostics_.presented_frames` à `backend_.present_count()` sur le
chemin réel, pas seulement dans le code mort `submit_ring()`.

## Correctif appliqué

`native/include/ac6/native_runtime.h` — l'accesseur `diagnostics()`
(déjà `const`) synchronise désormais `presented_frames`/`state` à la
LECTURE plutôt qu'à l'écriture, exactement le même calcul que
`submit_ring()` (r292) mais exécuté à chaque appel :

```c++
mutable RuntimeDiagnostics diagnostics_;
...
[[nodiscard]] const RuntimeDiagnostics& diagnostics() const noexcept {
  if (backend_.present_count() != diagnostics_.presented_frames) {
    diagnostics_.presented_frames = backend_.present_count();
    if (diagnostics_.state == RuntimeState::kBooted) {
      diagnostics_.state = RuntimeState::kRunning;
    }
  }
  return diagnostics_;
}
```

**Synchronisation à la lecture, pas à l'écriture** : les présents
réels arrivent de façon asynchrone sur le thread qui appelle
`VdSwap` (`NativeGuestVdService`), sans lien direct vers cet objet —
rattraper l'état à chaque lecture est plus simple et tout aussi
correct que d'essayer de pousser une mise à jour depuis là-bas.
`diagnostics_` devient `mutable` (nécessaire pour muter dans un
accesseur `const`) ; `state()` est routé à travers `diagnostics()`
pour bénéficier de la même synchronisation plutôt que de risquer un
`kBooted` périmé.

## Établi — vérifié en direct

- Reconstruction (`ninja ac6recomp`) : succès, aucun avertissement
  nouveau.
- **Lancement avec `AC6_NATIVE_VD_TRACE=1`** :
  ```
  vd swap presented 1280x720 present_count=5
  ac6recomp: presented_frames=5 state=2
  ```
  **Le diagnostic affiché correspond ENFIN exactement au compteur réel
  du backend.** (`state=2` = `RuntimeState::kRunning`, cohérent avec
  l'énumération `kCreated=0, kBooted=1, kRunning=2, ...`.)
- **Lancement autonome de contrôle** (sans gdb, sans trace) :
  `exit=0`, `elapsed=25s`, `presented_frames=5 state=2` — identique,
  conforme à la leçon méthodologique de r419.
- `ctest` natif : **11/11** réussis (`ac6_native_runtime_tests`,
  qui exerce `diagnostics()` directement, inclus et vert).
- Gates du dépôt et `ctest` racine : tous verts, même échec
  préexistant sans rapport (`ac6-cpp-complexity`).

## Ce que ceci établit

**La chaîne d'investigation r399-r431 est close.** Résumé de bout en
bout : un correctif antérieur mal conçu (r366) avait un défaut
d'exception-safety qui orphelinait silencieusement un tas géré par le
jeu (r413/r414), lui-même masqué par une sentinelle mal comparée
(r412), causant un débordement non borné (r410/r411) — corrigé et
vérifié (r414/r415). Le boot progresse ensuite bien plus loin qu'à
aucun moment de toute la campagne (r416/r417), révélant un second
défaut de robustesse (un verrou IRQL global non protégé contre une
terminaison d'exception, r420/r421) — corrigé et vérifié (r422),
couvert par un test automatisé (r424). Enfin, la présentation d'image
elle-même était cassée par une lacune d'implémentation locale du
stub `VdSwap` (r427), corrigée par un appel direct au lieu d'une
tentative d'injection PM4 risquée (r429 abandonné, r430 appliqué), et
le dernier maillon — le diagnostic affiché lui-même — est maintenant
reconnecté (ce cycle). **Plus aucun blocage connu dans cette chaîne.**

## Non établi

- **Le contenu visuel réel des images présentées** — toujours un
  simple effacement de couleur (`present_to_offscreen`, chantier
  séparé déjà nommé, sans rapport avec le mécanisme de présentation
  ou son diagnostic, tous deux désormais corrects).

## Décisions prises

- Synchroniser à la LECTURE (dans l'accesseur `diagnostics()`) plutôt
  qu'à l'ÉCRITURE (depuis `NativeGuestVdService`) — évite d'ajouter un
  couplage direct entre deux sous-systèmes qui n'en avaient pas besoin
  jusqu'ici, et couvre tout appelant de `diagnostics()`/`state()`
  uniformément, pas seulement le chemin `submit_ring()` historique.
- Corriger sa propre affirmation erronée (« aucun arriéré sur ce
  fichier ») avant publication, dès qu'un `git diff --stat` l'a
  contredite — précédent r423 directement répété ici, cette fois
  attrapé avant le commit plutôt qu'après.

## Divulgation de portée du commit — même situation que r430

`native_runtime.h` porte lui aussi un arriéré non committé
préexistant (`r277`, `r295` — l'inclusion de `native_vulkan_device.h`,
`native_guest_threads_stop_and_join()`, `has_offscreen_present_target()`,
`offscreen_device_`/`offscreen_target_`). Vérifié par `git diff
--stat` AVANT de rédiger cette section (une première version de ce
rapport affirmait à tort l'absence d'arriéré sans avoir vérifié — corrigé
ici avant publication). Comme pour r430, ce correctif dépend
directement de ce même arriéré (`backend_`/`offscreen_target_` déjà
présents) et ne peut pas en être isolé proprement. **Décision** :
committer l'état complet du fichier, divulgué explicitement.

## Gate

**Source de production éditée ce cycle** (avec divulgation
ci-dessus) :
`recompilation/ace-combat-6-retail/native/include/ac6/native_runtime.h`.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus)
`ctest` (racine et natif) relancés en entier.

## Named for r432

Aucun blocage restant identifié dans la chaîne allocateur/threads/
présentation. Ligne ouverte la moins chère, si poursuivie : le
contenu visuel réel des présents (remplacer l'effacement placeholder
par une vraie résolution EDRAM→image), ou l'audit dédié de l'arriéré
Vulkan/registre de shaders épinglés déjà nommé par r424 (avec son
fichier suspect `native_vulkan_backend.cpp.new_header_part`).

## Files

Aucun artefact gitignoré nouveau (journaux de vérification sous
`/tmp`, non conservés).
