# AC6 retail NTSC-U/J — r422 — correctif appliqué et vérifié en direct : IRQL résiduel libéré à la terminaison propre d'un thread, blocage d'arrêt disparu (4/4 lancements autonomes, avec et sans `AC6_NATIVE_VD_TRACE`)

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r421 (nommé pour r422) : concevoir et vérifier en direct un
correctif au blocage identifié — soit un IRQL réellement
`thread_local`, soit une garde d'exception libérant l'IRQL résiduel
sur le chemin `GuestThreadTerminated`.

## Décision de conception

Rendre l'IRQL réellement `thread_local` changerait la sémantique que
`g_dpc_level_mutex` fournit délibérément (r191 : « aucun autre code de
niveau DPC-ou-plus ne s'exécute en même temps », une VRAIE exclusion
mutuelle entre threads hôtes réels — nécessaire précisément parce que
ce projet exécute le code invité sur des threads concurrents, à la
différence du matériel Xbox 360 mono-cœur où l'élévation d'IRQL seule
suffit). Rendre l'état purement local au thread annulerait cette
exclusion mutuelle et pourrait réintroduire les data races que r191
visait à éviter. **Retenu : une garde d'exception qui libère l'IRQL
résiduel** — préserve l'exclusion mutuelle voulue dans le cas normal,
n'agit que dans le cas anormal (terminaison pendant une fenêtre
Raise-sans-Lower).

## Correctif appliqué

Trois fichiers, sous contrôle de version :

**`native/include/ac6/native_guest_threads.h`** : trois nouvelles
déclarations, `raise_dpc_level()`, `lower_dpc_level()`,
`release_residual_dpc_level()`.

**`native/src/native_guest_threads.cpp`** : `g_dpc_level_mutex`
(le texte et la justification `r191` déjà présents dans l'ancien
emplacement sont préservés ici, migrés depuis
`tools/materialize_native_import_stubs.py`) déplacé ici, accompagné
d'un nouveau compteur `thread_local int g_dpc_level_depth` :

```c++
void raise_dpc_level() noexcept {
  g_dpc_level_mutex.lock();
  ++g_dpc_level_depth;
}
void lower_dpc_level() noexcept {
  --g_dpc_level_depth;
  g_dpc_level_mutex.unlock();
}
void release_residual_dpc_level() noexcept {
  while (g_dpc_level_depth > 0) {
    --g_dpc_level_depth;
    g_dpc_level_mutex.unlock();
  }
}
```

**`tools/materialize_native_import_stubs.py`** : `KeRaiseIrqlToDpcLevel`/
`KfLowerIrql` génèrent désormais des appels à
`ac6::native::raise_dpc_level()`/`lower_dpc_level()` au lieu de
toucher le mutex directement ; le catch `GuestThreadTerminated` du
travailleur `ExCreateThread` appelle `release_residual_dpc_level()`.

**`native/src/ac6recomp_main.cpp`** : même appel ajouté dans les deux
blocs `catch` du thread d'entrée (`GuestThreadTerminated` ET
`catch(...)`, par défense en profondeur — seul `GuestThreadTerminated`
est confirmé comme mécanisme de lancer par r417/r419/r421, mais rien
ne coûte à couvrir aussi le cas générique).

`native-import-stubs.cpp` régénéré
(`python3 tools/materialize_native_import_stubs.py --mapping … --output …`,
229 imports, inchangé en nombre) et la copie de mise en scène
`build/.../native-source/` synchronisée manuellement (le pipeline
`prepare.py` normal fait un `shutil.copytree` complet ; ce cycle a
recopié seulement les trois fichiers modifiés, vérifié identiques par
`diff`).

## Établi — vérifié en direct

### Reconstruction propre

`ninja ac6recomp` : succès, aucune erreur (avertissements
préexistants sans rapport, `simde`).

### Le blocage d'arrêt a disparu, en lancement AUTONOME (pas sous gdb — leçon de r419 appliquée)

Quatre lancements indépendants, sans gdb :

| run | fenêtre | résultat |
|---|---|---|
| 1 | `10000ms` | `exit=0`, `elapsed=10s` |
| 2 | `25000ms` | `exit=0`, `elapsed=25s` |
| 3 | `25000ms` | `exit=0`, `elapsed=25s` |
| 4 | `25000ms` | `exit=0`, `elapsed=26s` |
| 5 (`AC6_NATIVE_VD_TRACE=1`) | `25000ms` | `exit=0`, `elapsed=26s` |

**Chaque run se termine avec un code de sortie normal, dans un délai
correspondant exactement à sa fenêtre de sonde configurée — plus
aucun dépassement.** Le déclencheur original de r418
(`AC6_NATIVE_VD_TRACE=1`) est explicitement retesté et ne reproduit
plus le blocage.

### Suites de tests

- `ctest` (`build/ntsc-uj/native/native-cmake`) : **10/10** réussis
  (`ac6_native_xenos_tests` inclus, `15.91s`).
- `pytest tests/test_materialize_native_import_stubs.py` : **105/105**
  — trois assertions de test mises à jour pour refléter le nouveau
  câblage (`g_dpc_level_mutex.lock()`/`unlock()` littéral dans le
  fichier généré remplacé par `ac6::native::raise_dpc_level()`/
  `lower_dpc_level()` ; assertion ajoutée que le catch
  `GuestThreadTerminated` d'`ExCreateThread` appelle bien
  `release_residual_dpc_level()`).
- `pytest tests/` (suite complète du produit) : **222 réussis, 1
  ignoré** (préexistant, sans rapport).
- Gates du dépôt (`audit_ac6_mission01_native_gate.py`,
  `audit_ac6_contract_artifacts.py`, `audit_ac6_contract_addresses.py`)
  et `ctest` racine (`reconstruction/ace-combat-6/build`) : tous verts,
  même échec préexistant sans rapport (`ac6-cpp-complexity`).

## Ce que ceci établit

**La chaîne r418-r422 est close** : le blocage d'arrêt découvert par
r418, confirmé indépendant du drapeau de trace par r419, root-causé
depuis le code seul par r420, localisé en direct par r421, est
maintenant corrigé et vérifié en direct sur quatre lancements
autonomes indépendants plus une suite de tests complète. Le
mécanisme d'arrêt propre de r277 fonctionne désormais correctement
même quand un thread est terminé pendant qu'il détient un IRQL élevé.

## Non établi

- **Couverture par un test automatisé de la régression elle-même** —
  aucun test ne reproduit délibérément « lancer `GuestThreadTerminated`
  pendant un IRQL élevé » pour vérifier que `release_residual_dpc_level()`
  s'exécute réellement dans ce scénario exact (les vérifications de ce
  cycle portent sur le texte généré et le comportement de bout en
  bout, pas sur un test unitaire ciblé de ce chemin précis).
- **Si d'autres primitives noyau émulées de façon similaire (verrou
  global partagé entre threads hôtes) partagent la même classe de
  défaut** — non recherché systématiquement au-delà de
  `g_dpc_level_mutex`.

## Décisions prises

- Choisir la garde d'exception plutôt que l'IRQL `thread_local` —
  préserve la sémantique d'exclusion mutuelle que r191 avait
  délibérément construite, ne change le comportement que dans le cas
  anormal (précédent CLAUDE.md : ne pas remplacer un choix de
  conception réfléchi sans nécessité).
- Ajouter `release_residual_dpc_level()` aussi au `catch(...)`
  générique du thread d'entrée (`ac6recomp_main.cpp`), pas seulement
  au `catch (GuestThreadTerminated&)` confirmé — coût nul, filet de
  sécurité pour toute exception non anticipée qui traverserait la même
  fenêtre.
- Recopier manuellement les trois fichiers modifiés vers la mise en
  scène `native-source/` plutôt que relancer tout `prepare.py` —
  vérifié identique par `diff` avant la reconstruction, évite de
  retélécharger/reconfigurer tout le pipeline pour un changement
  ciblé.

## Gate

**Sources de production éditées ce cycle** :
`recompilation/ace-combat-6-retail/native/include/ac6/native_guest_threads.h`,
`recompilation/ace-combat-6-retail/native/src/native_guest_threads.cpp`,
`recompilation/ace-combat-6-retail/native/src/ac6recomp_main.cpp`,
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
`recompilation/ace-combat-6-retail/tests/test_materialize_native_import_stubs.py`.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus)
`ctest` (racine et natif) relancés en entier, `pytest` relancé en
entier.

## Named for r423

Aucun blocage restant identifié dans la chaîne r399-r422. Ligne
ouverte la moins chère : ajouter un test ciblé qui simule directement
« IRQL élevé puis `GuestThreadTerminated` » pour couvrir cette
régression précise en continu. Sinon, revenir à `presented_frames=0`/
`present=0` (r418, jamais résolu — la boucle par image tourne mais
n'émet toujours aucun paquet `Present`) comme prochaine question
produit, désormais sans blocage d'arrêt pour compliquer
l'investigation.

## Files

`recompilation/ace-combat-6-retail/build/ntsc-uj/native/codegen-20260831-mapfix-96838/native-import-stubs.cpp`
(régénéré, gitignoré) ; `/tmp/r422_verify*.log` (captures de
vérification ad hoc, non trackées).
