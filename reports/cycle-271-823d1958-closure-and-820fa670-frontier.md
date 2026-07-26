# AC6 cycle 271 — fermeture `0x823D1958` et front `0x820FA670`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : `0x823D1958` est-elle une entrée réelle ou une instruction interne
  aux boucles imbriquées de `sub_823D1938` ?

## Preuve headless

Le vérificateur en lecture seule passe **20/20** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify823D1958Boundary.java
```

Il établit :

- l'entrée réelle `0x823D1938`, qui initialise le zéro, la base de table et
  le curseur `r11` ;
- la tête externe `0x823D1948`, qui initialise `r10=3` ;
- la tête interne `0x823D194C`, puis les deux stores dépendant de `r11` ;
- à `0x823D1958`, un simple `addi r11,r11,8` qui consomme l'état préparé ;
- aucune référence entrante vers `0x823D1958` ;
- les branches arrière `0x823D1960 -> 0x823D194C` et
  `0x823D196C -> 0x823D1948` ;
- le tail-dispatch `0x823D1978 -> 0x82380040` ;
- le prochain thunk indépendant à `0x823D1980`.

Verdict : `0x823D1958` est un départ interne **confirmed**.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x823D1958 = { name = "rex_sub_823D1958" }
```

- hash TOML avant :
  `8f658a822164689d436e07c33aac92b667ac2e8ec796bbc69973e266c7bd7e9e` ;
- hash TOML après :
  `81bf060550e75681b934b56d7f9ae7c84102b7ddabda62ffb627b6f9224d9f60`.

Le codegen passe avec **23 351** fonctions en 14,008 s, puis le runtime se
lie avec `-j16`. L'unité `.49.cpp` a désormais le SHA-256
`2f1b733ece5448b73886fdc88f1d214d8e9919a83837a7da1c98f0d649ec5b3b`.
Le symbole `rex_sub_823D1958` et les deux fatals de la double boucle ont
disparu. Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke borné atteint le code PPC puis reçoit `SIGABRT` :

```text
__imp__sub_820FA5E8
  -> Unresolved branch from 0x820FA6DC to 0x820FA670
  -> __imp__sub_820FA258
  -> __imp__sub_823D19D0
```

La configuration contient `0x820FA690`, au sein de cette famille. C'est la
prochaine entrée à qualifier contre `sub_820FA5E8` et la boucle de tête
`0x820FA670`; elle reste intacte dans ce cycle.

## Validation native

- build AC6 `-j16` : PASS ;
- CTest : **48/48 PASS** en 32,68 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels AC6/GDB : 0 après clôture.

## État

- fonctions générées : 23 351 ;
- frontière `0x823D194C` : fermée ;
- nouvelle frontière : `0x820FA6DC -> 0x820FA670` ;
- entrée configurée à auditer : `0x820FA690` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
