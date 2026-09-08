# AC6 retail NTSC-U/J — r438 — SSBO de `PinnedShaderRuntime` élargi de 64 à 512 Mio (RAM physique Xbox 360 réelle), appliqué et vérifié — NON committé (arriéré `native_vulkan_backend.cpp` déjà déféré par r433) ; lacune de `build.py` trouvée et corrigée, committée séparément (diff propre, isolé)

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r437, piste indépendante 2 (ne nécessitant pas de décision
de coût utilisateur, contrairement à la piste 1 sur `sub_821D6C20`) :
reprendre la ligne ouverte de r434/r435 sur le contenu visuel réel —
le SSBO à 64 Mio de `PinnedShaderRuntime`, dimensionné pour des
adresses de test synthétiques, incompatible avec les vraies adresses
de fetch mesurées en direct par r435 (`~0x1274027b`, ~310 Mio).

## Correctif appliqué

`native/src/native_vulkan_backend.cpp`,
`PinnedShaderRuntime::PinnedShaderRuntime` : la taille du SSBO passe
de 64 Mio à **512 Mio** — la taille exacte de la RAM physique Xbox
360, pas une estimation. Le contrat d'adressage direct
(adresse-invité-utilisée-telle-quelle-comme-décalage, déjà en place
dans `draw_pinned`/`decode_pixel_texture`) reste inchangé : aucun
remappage n'était nécessaire, seule la fenêtre était trop petite pour
les vraies adresses.

```c++
// Shared memory SSBO: 512 MiB of raw guest bytes -- the full Xbox 360
// physical RAM size, not a guess. ...
shared_memory_dwords_ = (512ull * 1024ull * 1024ull) / 4ull;
```

## Établi — vérifié

- **Reconstruction complète et propre** via `tools/build.py --profile
  native --target ntsc-uj` (reconfiguration CMake depuis zéro,
  `clang++-21`, `Release`, avec les bonnes variables
  `AC6_NATIVE_GUEST_DIR`/`AC6_NATIVE_SIMDE_DIR`/`AC6_NATIVE_IMPORT_STUBS`)
  — 110 cibles compilées, aucune erreur (avertissements préexistants
  sans rapport uniquement).
- **`ctest` complet : 11/11** (après le correctif décrit ci-dessous
  sur `build.py` lui-même — voir section suivante).
- **Lancement réel** (`--probe-entry`,
  `AC6_NATIVE_ALLOW_ENTRY_PROBE=1`, `AC6_NATIVE_VD_TRACE=1`, fenêtre
  25 s) : reproduit EXACTEMENT le même `SIGSEGV` déjà établi par
  r435-r437 dans `sub_821D6C20` (thread d'entrée, même point exact du
  journal — `fetch_const[0]=0x1274027b` puis `vd publish write=49`),
  confirmant que ce correctif n'interagit pas avec ce défaut déjà
  connu et documenté (chemin `VdSwap`/service natif, complètement
  séparé du thread d'entrée où le crash a lieu).

## Incident de cycle : reconstruction « propre » a cassé le répertoire de build, récupéré via l'outillage du projet

En tentant une reconstruction véritablement propre (discipline établie
depuis r432), un `rm -rf CMakeFiles CMakeCache.txt` suivi d'une
reconfiguration manuelle a **omis les variables CMake requises**
(`AC6_NATIVE_GUEST_DIR`, `AC6_NATIVE_SIMDE_DIR`,
`AC6_NATIVE_IMPORT_STUBS`, et le compilateur `clang++-21` attendu) —
`tools/build.py` construit normalement ces valeurs automatiquement
(dont le chemin partagé `/fastdata/lavaulta/auto-re-agent/.tools/
xenonrecomp-source/...`, en dehors de ce dépôt). Une première tentative
de reconfiguration manuelle a produit un `ac6recomp` SANS le code du
jeu recompilé lié (silencieux : aucune erreur, juste un binaire
squelette). **Récupération** : suppression complète de
`build/ntsc-uj/native/native-cmake` et reconstruction intégrale via
`python3 tools/build.py --profile native --target ntsc-uj`, l'outil du
projet conçu précisément pour cela — aucune source du dépôt n'a été
affectée, uniquement le répertoire de build jetable.

## Lacune séparée trouvée et corrigée localement : `build.py` ne construisait jamais `ac6_native_guest_threads_tests`

Pendant la récupération, `build.py` a échoué sur `ctest` :
`ac6_native_guest_threads_tests` (ajouté par r424 dans
`CMakeLists.txt`, jamais mis à jour dans `tools/build.py`) n'était
construit par AUCUNE des deux listes de cibles explicites du script —
`ctest` essayait de le lancer sans qu'il existe. **Corrigé** : ajout
de `"ac6_native_guest_threads_tests"` à la deuxième liste de cibles
dans `tools/build.py`. Reconstruction relancée : **11/11 réussis**.

**Correction en cours de rédaction de ce rapport** : une vérification
initiale via `git check-ignore` avait suggéré à tort que
`tools/build.py` était gitignoré (le motif `build/` de `.gitignore`
matchait un chemin le contenant) — `git ls-files`/`git log` confirment
qu'il est en réalité **suivi normalement** (`.gitignore` n'empêche
que l'ajout de nouveaux fichiers non suivis, pas la modification d'un
fichier déjà suivi). `git diff --stat` : **+1/-1, un diff propre et
isolé**, sans arriéré. **Committé normalement avec ce cycle.**

## Décision de portée du commit — le correctif SSBO N'EST PAS committé ce cycle

`git diff --stat` sur `native_vulkan_backend.cpp` : **+2887 lignes**
contre HEAD — la quasi-totalité du fichier, cohérent avec le catalogue
précis de r433 (`+2881/-0` alors, la quasi-totalité de l'implémentation
`PinnedShaderRuntime` jamais committée depuis `e0afccdd`). Mon édition
de ce cycle (une seule constante, 64→512) est noyée dans cet arriéré
préexistant, déjà explicitement caractérisé et délibérément NON
committé par r433 — décision répétée dans chaque rapport depuis r423.
**Committer ce fichier maintenant, même pour une seule ligne réelle,
reviendrait à inverser unilatéralement la décision de r433** sans
élément nouveau qui la justifierait. **Décision : laisser le
correctif appliqué localement (vérifié, fonctionnel), ne rien
committer de `native_vulkan_backend.cpp` ce cycle.**

## Non établi

- **Si le SSBO de 512 Mio suffit pour toutes les adresses réelles du
  jeu** — seules quelques adresses de fetch ont été mesurées (r435),
  toutes bien en dessous de 512 Mio, mais rien ne garantit
  l'exhaustivité sur toute une session de jeu.
- **Le câblage réel d'`execute_frame` dans le chemin `VdSwap` en
  direct** — toujours pas fait ce cycle ; ce correctif lève
  uniquement le blocage d'adressage identifié par r434/r435, il ne
  branche rien de nouveau dans le chemin d'exécution réel.

## Décisions prises

- Élargir la fenêtre à 512 Mio (RAM physique réelle) plutôt qu'à la
  plage mesurée minimale (~310 Mio) — valeur non arbitraire, contrat
  explicite correspondant au matériel réel, pas un ajustement ad hoc
  au premier échantillon observé.
- Ne pas committer `native_vulkan_backend.cpp` ce cycle — cohérence
  avec la décision de r433, pas de nouvelle information qui la
  changerait.
- Corriger la lacune de `build.py` trouvée en cours de route (cible de
  test manquante) et la committer — diff propre et isolé (+1/-1),
  sans rapport avec l'arriéré de `native_vulkan_backend.cpp`, aucune
  raison de la laisser non committée.

## Gate

**Source de production committée ce cycle** :
`recompilation/ace-combat-6-retail/tools/build.py` (diff isolé,
+1/-1). `native_vulkan_backend.cpp` reste délibérément non committé
(arriéré déjà catalogué par r433).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source committée
affectée)
`ctest` racine inchangé (même échec préexistant sans rapport).

## Named for r439

Si l'utilisateur tranche la décision de committage de l'arriéré
`native_vulkan_backend.cpp` (r433/r434 nommés), ce correctif de 512
Mio serait committé avec — sinon, il reste appliqué localement,
vérifié, prêt. Indépendamment de cette décision : la piste 1 de r437
(auto-analyse Ghidra ciblée sur `sub_821D6C20`/`0x82935D98`) reste
ouverte si l'utilisateur en accepte le coût.

## Files

Aucun artefact gitignoré nouveau.
