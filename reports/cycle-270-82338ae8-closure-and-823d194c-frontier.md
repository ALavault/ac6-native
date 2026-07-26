# AC6 cycle 270 — fermeture `0x82338AE8` et front `0x823D194C`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : `0x82338AE8` est-elle une entrée réelle ou la branche arrière
  interne de la fonction feuille `0x82338AB8` et de sa boucle
  `0x82338ACC` ?

La nouvelle archive racine `ac6_82275f78_boundary_audit_v1.zip`, SHA-256
`2d62c6886342ec8c1c46539507c609314eaa6a5cb205aa402ffd4f961ee0f21d`,
concerne la famille précédente déjà consommée aux cycles 268–269. Elle ne
contredit pas et ne qualifie pas la frontière présente.

## Preuve headless

Le vérificateur en lecture seule passe **19/19** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify82338AE8Boundary.java
```

Il établit :

- l'initialisation de `r10`, du nombre d'éléments à `r3+0x30` et de `r11`
  depuis l'entrée réelle `0x82338AB8` ;
- la tête de boucle `0x82338ACC` et ses mises à jour de `r9`, `r10` et `r11` ;
- la comparaison `cmpw cr6,r10,r9` à `0x82338AE4` ;
- la branche arrière `blt cr6,0x82338ACC` à `0x82338AE8`, qui dépend donc
  directement d'un état antérieur ;
- aucune référence entrante vers `0x82338AE8` ;
- le fallthrough dans la même fonction jusqu'au `blr` à `0x82338B50` ;
- un nouveau prologue autonome à `0x82338B58`.

Verdict : `0x82338AE8` est un départ interne **confirmed**. Aucune sémantique
de gameplay n'est inférée.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x82338AE8 = { name = "rex_sub_82338AE8" }
```

- hash TOML avant :
  `8be6a53c282a43f1e7360674305474039603556546b4248df2e20521b89340ae` ;
- hash TOML après :
  `8f658a822164689d436e07c33aac92b667ac2e8ec796bbc69973e266c7bd7e9e`.

```bash
cmake --build .tools/ac6-recomp-reference/out/build/linux-amd64-runtime-localdev \
  --target ac6recomp_codegen -j16
cmake --build .tools/ac6-recomp-reference/out/build/linux-amd64-runtime-localdev \
  --target ac6recomp -j16
```

Les deux commandes passent. Le codegen traite **23 352** fonctions en
13,429 s. L'unité `.37.cpp` a désormais le SHA-256
`ba6ddc84439c2fe8598efc697ddd89df0c2c1c259a41b963d0a0175a4b7029f7`.
Le symbole `rex_sub_82338AE8` et le fatal
`0x82338AE8 -> 0x82338ACC` ont disparu. Aucun fichier généré n'a été modifié
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
__imp__sub_823D1938
  -> Unresolved branch from 0x823D1960 to 0x823D194C
  -> __imp__sub_821F7AE8
  -> __imp__xstart
```

Le généré montre deux boucles imbriquées dans `sub_823D1938`, avec têtes
`0x823D1948` et `0x823D194C`. La configuration contient précisément
`0x823D1958`, au milieu du corps interne avant la première branche arrière.
Cette famille est la prochaine frontière à qualifier; elle n'est ni auditée
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
- CTest : **48/48 PASS** en 35,89 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS.

## État

- fonctions générées : 23 352 ;
- frontière runtime `0x82338ACC` : fermée ;
- nouvelle frontière : `0x823D1960 -> 0x823D194C` ;
- entrée configurée à auditer : `0x823D1958` dans `sub_823D1938` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée ;
- processus résiduels AC6/GDB : 0 après contrôle de clôture.
