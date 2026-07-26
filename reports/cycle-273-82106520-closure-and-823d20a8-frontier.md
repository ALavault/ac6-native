# AC6 cycle 273 — fermeture `0x82106520` et front `0x823D20A8`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : `0x82106520` est-elle une entrée réelle ou un départ interne à
  la double boucle de `sub_82106358` ?

## Preuve headless

Le vérificateur en lecture seule passe **30/30** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify82106520Boundary.java
```

Il établit :

- l'entrée réelle `0x82106358` et son helper de sauvegarde ;
- l'initialisation de `r28=0`, `r8=1`, `r27` et `r29` avant les boucles ;
- la tête extérieure `0x82106400` ;
- le store scratch `std r9,-0x68(r1)` à `0x82106514` ;
- à `0x82106520`, le load apparié `lfd f13,-0x68(r1)`, sans référence
  entrante ;
- la boucle interne `0x82106530..0x82106560` ;
- la branche extérieure `0x82106578 -> 0x82106400` ;
- le restore commun `0x8210657C -> 0x82382F34` ;
- un nouveau frame indépendant à `0x82106580`.

Verdict : `0x82106520` est un départ interne **confirmed**.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x82106520 = { name = "rex_sub_82106520" }
```

- hash TOML avant :
  `f448f47321d7aad1b6720f99990e94e9b3d3d9869c3a00b84bbe8b09720b9f1b` ;
- hash TOML après :
  `1d08c0dacad2e7a7c45063aea39cd5981828d08c67106454c0a48197ca706d4b`.

Le codegen passe avec **23 349** fonctions en 12,865 s, puis le runtime se
lie avec `-j16`. L'unité `.5.cpp` a désormais le SHA-256
`748e74b94ffe1d7fef5d67662e3add99bed150038da13bf081ad02d60917606b`.
Le symbole `rex_sub_82106520` et le fatal vers `0x82106400` ont disparu.
Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke borné atteint le code PPC puis reçoit `SIGABRT` :

```text
__imp__sub_823D2088
  -> Unresolved branch from 0x823D20C0 to 0x823D20A8
  -> __imp__sub_821F7AE8
  -> __imp__xstart
```

La configuration contient `0x823D20B0`, au milieu de cette famille. Cette
adresse est le prochain départ à qualifier ; elle reste intacte dans ce cycle.

## Validation native

- build AC6 `-j16` : PASS ;
- CTest : **48/48 PASS** en 31,66 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels AC6/GDB : 0 après clôture ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 349 ;
- frontière `0x82106400` : fermée ;
- nouvelle frontière : `0x823D20C0 -> 0x823D20A8` ;
- entrée configurée à auditer : `0x823D20B0` dans `sub_823D2088` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
