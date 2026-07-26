# AC6 cycle 272 — fermeture `0x820FA690` et front `0x82106400`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : `0x820FA690` est-elle une entrée réelle ou un départ interne à
  la boucle de `sub_820FA5E8` ?

## Preuve headless

Le vérificateur en lecture seule passe **26/26** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify820FA690Boundary.java
```

Il établit :

- le prologue réel à `0x820FA5E8`, son frame de `0x80` octets et la sauvegarde
  de `f31` ;
- l'initialisation de `r29=2`, `r30` et `r31` avant la boucle ;
- la tête de boucle `0x820FA670`, qui consomme `r31/f31` ;
- à `0x820FA690`, un simple calcul d'argument `r3=r31+8` ;
- aucune référence entrante vers `0x820FA690` ;
- les deux appels, la décrémentation de `r29`, l'avancement de `r31` et la
  branche arrière `0x820FA6DC -> 0x820FA670` ;
- le restore tail apparié jusqu'à `0x820FA6EC` ;
- le prochain prologue indépendant à `0x820FA6F0`.

Verdict : `0x820FA690` est un départ interne **confirmed**.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x820FA690 = { name = "rex_sub_820FA690" }
```

- hash TOML avant :
  `81bf060550e75681b934b56d7f9ae7c84102b7ddabda62ffb627b6f9224d9f60` ;
- hash TOML après :
  `f448f47321d7aad1b6720f99990e94e9b3d3d9869c3a00b84bbe8b09720b9f1b`.

Le codegen passe avec **23 350** fonctions en 12,760 s, puis le runtime se
lie avec `-j16`. L'unité `.5.cpp` a désormais le SHA-256
`00c5239297cd02c6b0ab03f457e238d907f77e77b7eaab0de842f98ec3d4e3ef`.
Le symbole `rex_sub_820FA690` et le fatal vers `0x820FA670` ont disparu.
Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke borné atteint le code PPC puis reçoit `SIGABRT` :

```text
__imp__sub_82106358
  -> Unresolved branch from 0x82106578 to 0x82106400
  -> __imp__sub_820FA258
  -> __imp__sub_823D19D0
```

La configuration contient `0x82106520`, et le généré produit le duplicat
`rex_sub_82106520` au sein de la même famille. Cette adresse est le prochain
départ à qualifier; elle reste intacte dans ce cycle.

## Validation native

- build AC6 `-j16` : PASS ;
- CTest : **48/48 PASS** en 31,62 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels AC6/GDB : 0 après clôture.

## État

- fonctions générées : 23 350 ;
- frontière `0x820FA670` : fermée ;
- nouvelle frontière : `0x82106578 -> 0x82106400` ;
- entrée configurée à auditer : `0x82106520` dans `sub_82106358` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
