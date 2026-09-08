# AC6 retail NTSC-U/J — r424 — test ciblé ajouté et vérifié pour la classe de régression IRQL de r421/r422

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r423 (correction de traçabilité, pas de contenu technique).
`reports/handoff/CURRENT.json` nommait, comme piste la moins chère
encore ouverte : ajouter un test ciblé qui simule directement « IRQL
élevé puis `GuestThreadTerminated` » pour couvrir la classe de
régression fixée par r422 sans dépendre uniquement de la vérification
manuelle en lancement autonome.

## Correctif — nouveau fichier de test

`native/tests/native_guest_threads_tests.cpp` (nouveau, sous contrôle
de version), enregistré dans `native/CMakeLists.txt` comme
`ac6_native_guest_threads_tests`, avec `TIMEOUT 10` (une régression
réintroduisant le blocage bloquerait indéfiniment plutôt que
d'échouer proprement — le délai borne cet échec à un `CTest TIMEOUT`
net plutôt qu'un blocage de toute la suite).

Deux vérifications :

1. **`residual_irql_is_released_on_simulated_termination`** : un
   thread appelle `raise_dpc_level()` deux fois (imbriqué, motif réel
   des sites d'appel `KeRaiseIrqlToDpcLevel`), PUIS
   `release_residual_dpc_level()` sans jamais appeler
   `lower_dpc_level()` — reproduit exactement ce que fait le
   déroulement d'exception `GuestThreadTerminated` (r421). Un second
   thread tente ensuite `raise_dpc_level()` : s'il progresse (constaté
   par un `std::atomic<bool>`), le verrou n'a pas été orphelin.
2. **`raise_dpc_level_excludes_concurrent_threads`** : vérifie que le
   correctif n'a PAS affaibli l'exclusion mutuelle réelle que
   `g_dpc_level_mutex` fournit délibérément (r191) — un second thread
   ne doit PAS pouvoir élever l'IRQL tant que le premier ne l'a pas
   explicitement redescendue par un appariement normal Raise/Lower.

## Établi — vérifié en direct

- `cmake .` (reconfiguration, nouvelle cible) puis
  `ninja ac6_native_guest_threads_tests` : succès.
- Exécution directe : `exit=0`.
- `ctest` (`build/ntsc-uj/native/native-cmake`) : **11/11** réussis
  (nouvelle cible incluse, `0.06s`).
- `ninja ac6recomp` : aucun travail nécessaire (déjà à jour depuis
  r422), lancement autonome de contrôle (`exit=0`, sortie propre) —
  confirme que l'ajout du test n'a rien perturbé du binaire principal.
- Gates du dépôt et `ctest` racine
  (`reconstruction/ace-combat-6/build`) : tous verts, même échec
  préexistant sans rapport (`ac6-cpp-complexity`).

## Découverte séparée, non liée à ce qui précède : `HEAD` n'a jamais été constructible depuis un clone franc

En essayant de committer une version MINIMALE de `CMakeLists.txt`
(seulement mon ajout, sans le reste du contenu non committé déjà
présent dans l'arbre de travail — `find_package(Vulkan)`, l'enrobage
du registre de shaders épinglés, `native_vulkan_device.cpp`, etc.), la
reconstruction locale a échoué à l'étape de LIEN :
`native_vulkan_backend.cpp` (déjà committé de longue date, appelle de
vraies fonctions Vulkan comme `vkCmdDraw`/`vkQueueSubmit`) n'a jamais
eu de `find_package(Vulkan REQUIRED)`/`target_link_libraries(...
Vulkan::Vulkan...)` dans AUCUNE version committée de ce fichier
(`git log -p` sur tout l'historique de `CMakeLists.txt` : zéro
occurrence de "Vulkan"). **`HEAD` — indépendamment de tout ce qui
précède aujourd'hui — n'a jamais été constructible depuis un clone
franc du dépôt**, uniquement depuis un arbre de travail où les
fichiers non committés nécessaires sont physiquement déjà présents
sur disque. Ceci affecte TOUTE la suite `ctest` existante (chaque
cible de test lie déjà `ac6_native_xenos`, qui contient
`native_vulkan_backend.cpp`), pas seulement la nouvelle cible de ce
cycle. **Décision** : ne pas tenter de corriger cet écart plus ancien
et plus large dans ce cycle (portée bien au-delà d'« ajouter un test
») — committer uniquement mon ajout minimal (qui ne change rien à
cette situation préexistante, ni en bien ni en mal) et nommer la
découverte pour un cycle dédié.

## Ce que ceci établit

La classe de régression que r421 a localisée et que r422 a corrigée
est désormais couverte par un test automatisé et rapide (`0.06s`),
plutôt que par la seule vérification manuelle en lancement autonome
que r422 avait faite. Toute réintroduction future du même défaut
(par exemple si une refonte de `native_guest_threads.cpp` oublie
d'appeler `release_residual_dpc_level()` dans un nouveau site de
capture) échouera à `ctest` immédiatement, avec un message clair
(`TIMEOUT` si le verrou est réellement orphelin, `assert()` échoué si
l'exclusion mutuelle a été affaiblie par erreur) plutôt qu'un blocage
d'arrêt observable seulement en production.

## Non établi

- **Couverture d'autres primitives noyau émulées par un verrou global
  partagé** (déjà noté non résolu par r422) — ce cycle ajoute un test
  spécifique à `g_dpc_level_mutex`, pas une revue systématique des
  autres verrous partagés du projet.

## Décisions prises

- Ajouter un délai (`TIMEOUT 10`) à ce test précis plutôt qu'à toute
  la suite — c'est le seul test dont un échec attendu (verrou
  orphelin) serait un blocage silencieux plutôt qu'un échec net, donc
  le seul qui a besoin de cette protection.
- Recopier manuellement `CMakeLists.txt` et le nouveau fichier de test
  vers la mise en scène `native-source/` (même geste que r422) plutôt
  que relancer `prepare.py` en entier.

## Gate

**Sources de production éditées ce cycle** :
`recompilation/ace-combat-6-retail/native/tests/native_guest_threads_tests.cpp`
(nouveau),
`recompilation/ace-combat-6-retail/native/CMakeLists.txt`.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus)
`ctest` (racine et natif) relancés en entier.

## Named for r425

Deux pistes, sans rapport entre elles :

1. **La découverte séparée ci-dessus** — auditer et committer
   proprement l'arriéré Vulkan/registre de shaders épinglés
   (`native_vulkan_device.h/.cpp`, `native_pinned_shaders.h/.cpp`, la
   fixture, `native/tools/`) pour que `HEAD` redevienne constructible
   depuis un clone franc — un cycle dédié, pas une correction rapide
   (`native/src/native_vulkan_backend.cpp.new_header_part`, un nom de
   fichier suggérant un travail en cours inachevé, mérite un examen
   attentif avant tout committage).
2. Revenir à `presented_frames=0`/`present=0` (r418, jamais résolu)
   comme prochaine question produit — la boucle par image tourne mais
   le jeu invité n'émet toujours aucun paquet `Present` observé dans
   la fenêtre de sonde.

## Files

Aucun artefact gitignoré ce cycle au-delà des logs `/tmp` déjà
consultés.
