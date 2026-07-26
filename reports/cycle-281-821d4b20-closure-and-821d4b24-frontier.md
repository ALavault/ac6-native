# AC6 cycle 281 — fermeture `0x821D4B20` et front `0x821D4B24`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821D4B20`, immédiatement avant la cible
  arrière `0x821D4B24`, est-elle une fonction ou la fin du setup de boucle de
  `sub_821D4AE0` ?

## Preuve headless

Le vérificateur en lecture seule passe **32/32** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821D4B20Boundary.java
```

Il établit :

- le prologue sauvegardé et le frame `0x90` à `0x821D4AE0` ;
- l'initialisation du global `r28` et de ses champs par cette entrée ;
- `r27=0` et `r31=0` dans le setup ;
- le `lis r11,-0x7DFB` à `0x821D4B14`, consommé par
  `addi r26,r11,0x42A8` à `0x821D4B20` ;
- aucune référence entrante vers `0x821D4B20` ;
- la consommation de `r26/r27/r31` dans la boucle de quatre éléments ;
- l'avancement des curseurs, la branche `0x821D4BB8 -> 0x821D4B24` et le
  restore du frame réel.

Verdict : `0x821D4B20` est une instruction de setup interne **confirmed**, pas
une entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821D4B20 = { name = "rex_sub_821D4B20" }
```

L'entrée `0x821D4B30`, dans le corps de la boucle, reste intacte.

- hash TOML avant :
  `1871630cca779f01193c11c0c5aaa0e0514fdbcdd4861017ee7890d1ca91a1b4` ;
- hash TOML après :
  `8a3617b1d5cbd3a91b0504b1748f22e265a40df32bb7d0924265b61876a6e9b0`.

Le codegen passe avec **23 341** fonctions en 12,845 s. L'unité `.15.cpp`
a le SHA-256
`965e909edad5653cb51d29748cb01c82614602a618720b9342695bacb7d4916b`
et le runtime lié
`ec0a3ac359792b7f39cdc11f7cea19f08a448e56a776a97c9ecb3d60d33a7a54`.
Le pseudo-symbole `rex_sub_821D4B20` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke atteint encore :

```text
__imp__sub_821D4AE0
  -> Unresolved branch from 0x821D4BB8 to 0x821D4B24
  -> __imp__sub_823D2830
  -> __imp__sub_821F7AE8
```

La fausse entrée est fermée, mais la frontière ne l'est pas encore : le split
configuré `0x821D4B30` demeure dans le corps de cette même boucle. Il devient
le prochain candidat exact et reste intact dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 31,28 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 341 ;
- fausse entrée `0x821D4B20` : fermée ;
- frontière active inchangée : `0x821D4BB8 -> 0x821D4B24` ;
- entrée configurée suivante : `0x821D4B30` dans `sub_821D4AE0` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
