# AC6 cycle 274 — fermeture `0x823D20B0` et front `0x8212C820`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : `0x823D20B0` est-elle une entrée réelle ou un départ interne à
  la boucle d'initialisation de `sub_823D2088` ?

## Preuve headless

Le vérificateur en lecture seule passe **27/27** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify823D20B0Boundary.java
```

Il établit :

- l'entrée réelle `0x823D2088`, son frame et la sauvegarde de `r31` ;
- la dérivation de la base `r31` et des curseurs `r10/r11` avant la boucle ;
- la tête `0x823D20A8`, qui copie `r11` vers `r9` puis avance `r11` ;
- à `0x823D20B0`, un store qui consomme `r9/r10`, sans référence entrante ;
- la progression du curseur et la branche `0x823D20C0 -> 0x823D20A8` ;
- la remise à zéro des deux régions après la boucle ;
- le restore tail jusqu'au `blr` à `0x823D2104` ;
- un nouveau frame indépendant à `0x823D2108`.

Verdict : `0x823D20B0` est un départ interne **confirmed**.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x823D20B0 = { name = "rex_sub_823D20B0" }
```

- hash TOML avant :
  `1d08c0dacad2e7a7c45063aea39cd5981828d08c67106454c0a48197ca706d4b` ;
- hash TOML après :
  `1f11717b4087a17367ba59effcdef631a10ca4bad4ea488ca9b0c42ecccd4f5e`.

Le codegen passe avec **23 348** fonctions en 12,610 s, puis le runtime se
lie avec `-j16`. L'unité `.49.cpp` a désormais le SHA-256
`5c8c80aa337189b8d7595101cd235fef9b8c82feab606b4bd3d2dab27d4ea939`.
Le symbole `rex_sub_823D20B0` et le fatal vers `0x823D20A8` ont disparu.
Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke borné atteint le code PPC puis reçoit `SIGABRT` :

```text
__imp__sub_8212C7F8
  -> Unresolved branch from 0x8212C834 to 0x8212C820
  -> __imp__sub_82183C80
  -> __imp__sub_823D2530
```

La configuration contient `0x8212C830`, au milieu de cette famille. Cette
adresse est le prochain départ à qualifier ; elle reste intacte dans ce cycle.

## Validation native

- build AC6 `-j16` : PASS ;
- CTest : **48/48 PASS** en 32,97 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels AC6/GDB : 0 après clôture ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 348 ;
- frontière `0x823D20A8` : fermée ;
- nouvelle frontière : `0x8212C834 -> 0x8212C820` ;
- entrée configurée à auditer : `0x8212C830` dans `sub_8212C7F8` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
