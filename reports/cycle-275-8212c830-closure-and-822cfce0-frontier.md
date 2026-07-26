# AC6 cycle 275 — fermeture `0x8212C830` et front `0x822CFCE0`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : `0x8212C830` est-elle une entrée réelle ou un départ interne à
  la première boucle de remise à zéro de `sub_8212C7F8` ?

## Preuve headless

Le vérificateur en lecture seule passe **30/30** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify8212C830Boundary.java
```

Il établit :

- l'entrée leaf `0x8212C7F8`, qui initialise `r11=0`, `r8=-1`, le curseur
  `r10` et le compteur `r9=0x800` ;
- la première boucle `0x8212C820..0x8212C834`, qui efface deux tableaux ;
- à `0x8212C830`, une comparaison du compteur de cette boucle, sans référence
  entrante ;
- la branche arrière `0x8212C834 -> 0x8212C820` ;
- une seconde boucle `0x8212C840..0x8212C854` puis une boucle CTR
  `0x8212C87C..0x8212C884` qui consomment encore l'état de l'entrée ;
- le `blr` réel à `0x8212C888` ;
- un nouveau frame indépendant à `0x8212C890`.

Verdict : `0x8212C830` est un départ interne **confirmed**.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x8212C830 = { name = "rex_sub_8212C830" }
```

- hash TOML avant :
  `1f11717b4087a17367ba59effcdef631a10ca4bad4ea488ca9b0c42ecccd4f5e` ;
- hash TOML après :
  `ba50e515b25a533b352af4755dfb8bb8d67d588a2080f3013f516c5f2f55c418`.

Le codegen passe avec **23 347** fonctions en 13,411 s, puis le runtime se
lie avec `-j16`. L'unité `.7.cpp` a désormais le SHA-256
`f025b122613b3b266d817afdc97858de4c270953687ec73bc6ce09ae2c6998b4`.
Le symbole `rex_sub_8212C830` et le fatal vers `0x8212C820` ont disparu.
Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke borné atteint le code PPC puis reçoit `SIGABRT` :

```text
__imp__sub_822CFCA8
  -> Unresolved branch from 0x822CFCF4 to 0x822CFCE0
  -> __imp__sub_82183C80
  -> __imp__sub_823D2530
```

La configuration contient `0x822CFCE8`, au milieu de cette famille. Cette
adresse est le prochain départ à qualifier ; elle reste intacte dans ce cycle.

## Validation native

- build AC6 `-j16` : PASS ;
- CTest : **48/48 PASS** en 29,21 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels AC6/GDB : 0 après clôture ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 347 ;
- frontière `0x8212C820` : fermée ;
- nouvelle frontière : `0x822CFCF4 -> 0x822CFCE0` ;
- entrée configurée à auditer : `0x822CFCE8` dans `sub_822CFCA8` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
