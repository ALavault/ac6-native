# AC6 cycle 269 — fermeture `0x822760B8` et front `0x82338ACC`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : `0x822760B8` est-elle une entrée réelle ou un fallthrough de la
  fonction `0x82276098` et de sa boucle `0x822760B0` ?

## Preuve headless

Le nouveau vérificateur en lecture seule passe **21/21** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify822760B8Boundary.java
```

Il établit :

- un prologue autonome à `0x82276098`, suivi du helper de sauvegarde et d'un
  frame de `0x70` octets ;
- l'initialisation de `r29`, du compteur `r30=4` et de `r31=r29+0x20` ;
- la tête de boucle `0x822760B0`, qui charge `r11` puis `r5` ;
- le fallthrough `0x822760B4 -> 0x822760B8` ;
- à `0x822760B8`, un simple `addi r6,r11,0x3560` qui consomme l'état préparé
  avant cette adresse ;
- aucune référence entrante vers `0x822760B8` ;
- la branche arrière `0x822760EC -> 0x822760B0` et le restore tail apparié ;
- un nouveau prologue autonome à `0x82276100`.

Verdict : `0x822760B8` est un départ interne **confirmed**. Aucune sémantique
de gameplay n'est inférée.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x822760B8 = { name = "rex_sub_822760B8" }
```

- hash TOML avant :
  `79e90e105a13e71596538a506dd5e786700e0dd03dfed3d9561ec72b054e76f8` ;
- hash TOML après :
  `8be6a53c282a43f1e7360674305474039603556546b4248df2e20521b89340ae`.

```bash
cmake --build .tools/ac6-recomp-reference/out/build/linux-amd64-runtime-localdev \
  --target ac6recomp_codegen -j16
cmake --build .tools/ac6-recomp-reference/out/build/linux-amd64-runtime-localdev \
  --target ac6recomp -j16
```

Les deux commandes passent. Le codegen traite **23 353** fonctions en
13,030 s. L'unité `.24.cpp` a désormais le SHA-256
`adca9ff05aec2183cc9a4a6b0fea67cc1aaa3991c4625304f64c839643977982`.
Le symbole `rex_sub_822760B8` et le fatal
`0x822760EC -> 0x822760B0` ont disparu. Aucun fichier généré n'a été modifié
manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le même smoke instrumenté borné atteint le code PPC puis reçoit `SIGABRT` :

```bash
timeout --signal=TERM --kill-after=5s 60s xvfb-run -a \
  gdb -q -batch -ex 'set pagination off' -ex run \
  -ex 'thread apply all bt 12' --args ./ac6recomp
```

La première frame PPC fautive est désormais :

```text
__imp__sub_82338AB8
  -> Unresolved branch from 0x82338AE8 to 0x82338ACC
  -> __imp__rex_sub_8233A630
  -> __imp__sub_8233A6A0
```

Le généré montre une boucle dans `sub_82338AB8`, de tête `0x82338ACC`, avec
branche arrière à `0x82338AE8`. La configuration contient précisément
`0x82338AE8`. C'est la prochaine frontière à qualifier; elle n'est ni auditée
ni modifiée dans ce cycle.

## Validation native

```bash
cmake --build .build/ace-combat-6-clang-probes -j16
ctest --test-dir .build/ace-combat-6-clang-probes \
  --output-on-failure -j16
cmake --install .build/ace-combat-6-clang-probes --prefix "$PWD"
test ! -e bin/bin
```

- build : PASS ;
- CTest : **48/48 PASS** en 29,98 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS.

## État

- fonctions générées : 23 353 ;
- frontière runtime `0x822760B0` : fermée ;
- nouvelle frontière : `0x82338AE8 -> 0x82338ACC` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée ;
- processus résiduels AC6/GDB : 0 après contrôle de clôture.
