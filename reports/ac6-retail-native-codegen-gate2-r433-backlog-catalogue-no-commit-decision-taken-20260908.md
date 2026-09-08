# AC6 retail NTSC-U/J — r433 — catalogue précis de l'arriéré `native/` élargi (r432) : chaque fichier identifié, sa provenance, sa dernière trace committée — décision de committer explicitement NON prise ce cycle

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r432 : cataloguer précisément (quels fichiers, quels cycles,
quel volume) l'arriéré `native/` élargi découvert par `git status`,
avant toute décision de committer — sans prendre cette décision seul.

## Établi — inventaire complet, fichier par fichier

`git status --short recompilation/ace-combat-6-retail/native/` liste
20 chemins. Pour chacun, `git log -1` (fichiers suivis) ou
confirmation d'absence totale (fichiers jamais committés), et
`git diff --numstat` pour la taille réelle du delta contre HEAD :

### Modifiés (suivis par git, delta contre HEAD)

| Fichier | Dernier commit | +/- | Lignes totales |
|---|---|---|---|
| `native/CMakeLists.txt` | `65b385c0` 2026-09-08 (r238/r424) | +26/-1 | 175 |
| `include/ac6/native_guest_media.h` | `596147c0` 2026-09-02 (r189) | +5/-0 | 78 |
| `include/ac6/native_guest_memory.h` | `e0afccdd` 2026-08-31 (rattrapage r2-r76) | +12/-0 | 48 |
| `include/ac6/native_shader_translator.h` | `e0afccdd` 2026-08-31 | +79/-0 | 107 |
| `include/ac6/native_vulkan_backend.h` | `e0afccdd` 2026-08-31 | +336/-0 | 387 |
| `include/ac6/native_xenos.h` | `8bda3081` 2026-09-01 | +22/-0 | 372 |
| `src/native_guest_media.cpp` | `596147c0` 2026-09-02 (r189) | +33/-12 | 150 |
| `src/native_guest_memory.cpp` | `e0afccdd` 2026-08-31 | +15/-1 | 54 |
| `src/native_runtime.cpp` | `510aaf1d` 2026-09-01 (r130-r131) | +37/-2 | 206 |
| `src/native_shader_translator.cpp` | `e0afccdd` 2026-08-31 | +193/-3 | 207 |
| `src/native_vulkan_backend.cpp` | `e0afccdd` 2026-08-31 | **+2881/-0** | 3015 |
| `src/native_xenos.cpp` | `4bf6942f` 2026-09-01 (r100) | +28/-2 | 632 |
| `tests/native_runtime_tests.cpp` | `d1dce99b` 2026-09-01 | +14/-0 | 136 |
| `tests/native_xenos_tests.cpp` | `4bf6942f` 2026-09-01 (r100) | **+3825/-16** | 4241 |

### Jamais committés (aucune trace dans HEAD à aucun commit)

| Fichier | Lignes / taille |
|---|---|
| `include/ac6/native_pinned_shaders.h` | 46 lignes |
| `include/ac6/native_vulkan_device.h` | 198 lignes |
| `src/native_pinned_shaders.cpp` | 252 lignes |
| `src/native_vulkan_device.cpp` | 1399 lignes |
| `src/native_vulkan_backend.cpp.new_header_part` | 79 lignes (scratch, disposition connue depuis r432 : ne pas toucher) |
| `fixtures/pinned-shader-registry.v1.bin` | 6 989 412 octets, binaire opaque |
| `tools/materialize_pinned_shader_data.py` | générateur au moment de la configuration |

## Ce que ceci établit

**Le fait dominant** : `native_vulkan_backend.cpp` et
`native_xenos_tests.cpp` ne sont pas des ajouts marginaux — leur HEAD
committée (`e0afccdd`, `4bf6942f`) est un squelette précoce ; la quasi
-totalité de leur contenu actuel (2881 des 3015 lignes du premier,
3825 des 4241 du second) n'a **jamais** été committée. Ce n'est pas un
arriéré « en plus » du moteur natif : **c'est le moteur de rendu natif
lui-même** (backend Vulkan, device Vulkan, shaders épinglés,
traducteur de shaders, tests Xenos) qui vit entièrement hors de HEAD.
`native/CMakeLists.txt` référence déjà `native_vulkan_device.cpp` et
`native_pinned_shaders.cpp` (lignes 35/37, vérifié) — un clone HEAD ne
compilerait même pas la configuration CMake actuelle, cohérent avec la
lacune « jamais buildable en clone propre » déjà nommée depuis r424.

**Provenance par grep des rapports** (chaque fichier cité par des
rapports depuis) : `native_vulkan_backend` remonte à r100/r256-r258 ;
`native_xenos` à r94-r98 ; `native_shader_translator` à r144/r254 ;
`native_guest_media` à r100/r130-r131 ; `native_pinned_shaders` à
r255/r259 ; `native_vulkan_device` à r248. **Cet arriéré n'est pas
récent** — il s'étend sur la quasi-totalité de la campagne native
(r94 à r432), pas seulement depuis la découverte de r432.

**Ce n'est pas du travail spéculatif ou non testé.** Ce même code —
dans son état de travail actuel, pas celui de HEAD — est exactement ce
que `ctest` (`ac6_native_xenos_tests`, `ac6_native_runtime_tests`,
etc.) exerce et fait passer à chaque cycle de cette chaîne depuis
r413, y compris la reconstruction complètement propre de r432. La
fonctionnalité présente/vérifiée en direct tout au long de r427-r432
(present_count incrémentant, `presented_frames=5 state=2`) **dépend
de ce même arriéré** — ce n'est pas un code alternatif ou concurrent,
c'est le seul code qui existe pour ces sous-systèmes.

**Un précédent existe déjà pour ce type de committage groupé** : le
commit `e0afccdd` (« catch-up: commit the uncommitted retail-native
Gate 0-2 backlog (r2-r76) », 2026-08-31) a fait exactement ceci une
fois pour les cycles r2-r76. L'arriéré catalogué ici est la même
situation répétée pour r94-r432 : du travail réel, testé en continu,
jamais rattrapé par un commit équivalent depuis.

## Non établi

- **Si chaque fichier compile et teste correctement de façon
  strictement isolée** (indépendamment des autres fichiers de
  l'arriéré) — seule la combinaison complète de l'arbre de travail
  actuel a été vérifiée (reconstruction propre + `ctest` 11/11, r432).
  Aucun test n'a isolé un sous-ensemble.
- **Le contenu précis du delta de chaque fichier** (quelles lignes
  exactement) — ce cycle mesure la taille et la provenance, pas le
  contenu ligne à ligne de chacun des 14 fichiers modifiés.

## Décisions prises

- **Ne pas committer ce cycle.** Catalogue seul, pas d'action sur le
  contenu de l'arriéré — conforme à la décision explicite de r432 et
  au principe répété depuis r423 : la situation d'arbre non committé
  reste la décision de l'utilisateur.
- Ne pas isoler ni tester séparément chaque fichier de l'arriéré ce
  cycle — la preuve de fonctionnement existante (ctest complet sur
  l'arbre de travail entier) est déjà la preuve la plus forte
  disponible sans dépenser un cycle entier sur des reconstructions
  partielles hypothétiques.

## Gate

Aucune source de production éditée ce cycle (catalogue en lecture
seule).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, déjà vertes avant ce cycle, aucun
changement de source qui les affecterait)
`ctest` racine déjà relancé pour r432, aucun changement depuis.

## Named for r434

Si l'utilisateur souhaite committer cet arriéré : il est maintenant
catalogué précisément (tableau ci-dessus, 20 fichiers, deux groupes —
suivis avec delta massif contre HEAD, jamais committés) avec un
précédent de committage groupé déjà dans l'historique (`e0afccdd`).
La décision elle-même — committer en un seul commit de rattrapage
comme `e0afccdd`, ou par sous-système, ou pas du tout — reste à
prendre par l'utilisateur, pas par ce cycle.

## Files

Aucun artefact gitignoré nouveau.
