# AC6 cycle 282 — fermeture `0x821D4B30` et front `0x822CFBAC`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821D4B30` est-elle indépendante ou une
  instruction du corps de boucle de `sub_821D4AE0` ?

## Preuve headless

Le vérificateur en lecture seule passe **33/33** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821D4B30Boundary.java
```

Il établit :

- le prologue sauvegardé et le frame `0x90` à `0x821D4AE0` ;
- l'initialisation du global `r28`, des compteurs `r27/r31` et de la base
  `r26` par cette entrée ;
- le vrai loop head à `0x821D4B24` ;
- trois instructions de setup séquentielles avant
  `or r6,r27,r27` à `0x821D4B30` ;
- aucune référence entrante vers `0x821D4B30` ;
- la consommation du compteur copié et des états de l'entrée dans le corps ;
- l'avancement des curseurs, la branche `0x821D4BB8 -> 0x821D4B24` et le
  restore du frame réel.

Verdict : `0x821D4B30` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821D4B30 = { name = "rex_sub_821D4B30" }
```

- hash TOML avant :
  `8a3617b1d5cbd3a91b0504b1748f22e265a40df32bb7d0924265b61876a6e9b0` ;
- hash TOML après :
  `7e9a5df830fabd4791e2c05e28039d4c5befabb0a0bb24b2afb7894fb8a4e6b0`.

Le codegen passe avec **23 340** fonctions en 13,267 s. L'unité `.15.cpp`
a le SHA-256
`067ef43771c3e444e36a6ae6e15e301c6b1df57deb8275ca9d43ead4a96f8d38`
et le runtime lié
`cf3525f7248130eb8a4b568a7b6e71f430ab7ed6801a8f343a55d9e3e6bbf571`.
Le pseudo-symbole `rex_sub_821D4B30` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal `0x821D4BB8 -> 0x821D4B24` disparaît. Le smoke borné avance vers :

```text
__imp__sub_822CFB18
  -> Unresolved branch from 0x822CFC10 to 0x822CFBAC
  -> __imp__sub_822CC048
  -> __imp__sub_82213758
```

L'entrée configurée `0x822CFBA8`, immédiatement avant la cible de boucle,
devient le prochain candidat exact ; elle reste intacte dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 31,67 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 340 ;
- frontière `0x821D4BB8 -> 0x821D4B24` : fermée ;
- nouvelle frontière : `0x822CFC10 -> 0x822CFBAC` ;
- entrée configurée suivante : `0x822CFBA8` dans `sub_822CFB18` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
