# AC6 cycle 280 — fermeture `0x822CE7B0` et front `0x821D4B24`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x822CE7B0` est-elle indépendante ou le
  store interne de la boucle `0x822CE798..0x822CE7D4` de `sub_822CE6C8` ?

## Preuve headless

Le vérificateur en lecture seule passe **37/37** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify822CE7B0Boundary.java
```

Il établit :

- le prologue sauvegardé et le frame `0x100` à `0x822CE6C8` ;
- l'initialisation par cette entrée de `r31/r29/r30` et des tableaux `u16`
  de scratch aux offsets de pile `0x60` et `0x80` ;
- la dérivation du compteur `r5/r6` et du curseur objet `r7` ;
- le chargement de l'élément de scratch à `0x822CE7A8`, son incrément à
  `0x822CE7AC`, puis son store indexé à `0x822CE7B0` ;
- aucune référence entrante vers `0x822CE7B0` ;
- la mise à jour des extrema et curseurs, puis la branche arrière
  `0x822CE7D4 -> 0x822CE798` ;
- les sorties par le restore du frame réel.

Verdict : `0x822CE7B0` est un store interne **confirmed**, pas une entrée PPC
indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x822CE7B0 = { name = "rex_sub_822CE7B0" }
```

L'entrée `0x822CE718` et le hook `ac6PacDynamicHeaderProbe` à `0x822CEAC0`
restent intacts.

- hash TOML avant :
  `4e14d867072db77f39cb1eec8b9810d32537d0d56907758867fd3ad18ba9079f` ;
- hash TOML après :
  `1871630cca779f01193c11c0c5aaa0e0514fdbcdd4861017ee7890d1ca91a1b4`.

Le codegen passe avec **23 342** fonctions en 13,055 s. L'unité `.30.cpp`
a le SHA-256
`5790ea8f3d41cfa0f6f7711c485f91d42ecd7266b01d6783d6f010cdb3b19d20`
et le runtime lié
`bb3c7277f6094b8e186f28cd426fb89d2432b98228b7c405d1072c4698dd6cf4`.
Le pseudo-symbole `rex_sub_822CE7B0` a disparu et le probe dynamique reste
généré. Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal `0x822CE7D4 -> 0x822CE798` disparaît. Le smoke borné quitte cette
famille et avance vers :

```text
__imp__sub_821D4AE0
  -> Unresolved branch from 0x821D4BB8 to 0x821D4B24
  -> __imp__sub_823D2830
  -> __imp__sub_821F7AE8
```

L'entrée configurée `0x821D4B20`, immédiatement avant la cible de branche,
devient le prochain candidat exact ; elle reste intacte dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 28,90 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 342 ;
- frontière `0x822CE7D4 -> 0x822CE798` : fermée ;
- nouvelle frontière : `0x821D4BB8 -> 0x821D4B24` ;
- entrée configurée suivante : `0x821D4B20` dans `sub_821D4AE0` ;
- hook `ac6PacDynamicHeaderProbe` : conservé ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
