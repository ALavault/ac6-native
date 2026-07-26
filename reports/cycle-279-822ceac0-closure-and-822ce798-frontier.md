# AC6 cycle 279 — fermeture `0x822CEAC0` et front `0x822CE798`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x822CEAC0` est-elle indépendante ou une
  instruction interne de la branche alternative de `sub_822CE6C8` ?

## Audit prudent de l'instrumentation

La configuration contenait deux usages distincts de cette adresse :

- une pseudo-entrée `rex_sub_822CEAC0` dans la table des fonctions ;
- un hook `midasm` `ac6PacDynamicHeaderProbe` attaché à l'instruction.

Le corpus généré avant correction ne contenait aucun appel à la pseudo-fonction,
seulement sa déclaration, son implémentation et son dispatch. Le hook est un
comportement d'instrumentation existant et reste attaché à l'instruction dans
la fonction englobante ; il n'a pas été supprimé ou déplacé.

## Preuve headless

Le vérificateur en lecture seule passe **37/37** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify822CEAC0Boundary.java
```

Il établit :

- le prologue sauvegardé et le frame `0x100` à `0x822CE6C8` ;
- l'initialisation par cette entrée de `r31/r29/r30/r27/r26` ;
- les états de tables et de boucle qui conduisent à la branche alternative ;
- la production de `r7`, `r3`, `r4` et `r8` avant la cible ;
- `r11=r7+r3` à `0x822CEAB8`, immédiatement consommé par
  `addi r11,r11,0xA92` à `0x822CEAC0` ;
- aucune référence entrante vers `0x822CEAC0` ;
- la boucle de copie sur le scratch de pile, puis la réunion avec la boucle
  externe et le restore du frame d'origine.

Verdict : `0x822CEAC0` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x822CEAC0 = { name = "rex_sub_822CEAC0" }
```

Le bloc `[[midasm_hook]]` à `0x822CEAC0` est conservé.

- hash TOML avant :
  `ef782e9c38c5766460dddc7224b33793083a08a0cc7b844aef50c44e656dd158` ;
- hash TOML après :
  `4e14d867072db77f39cb1eec8b9810d32537d0d56907758867fd3ad18ba9079f`.

Le codegen passe avec **23 343** fonctions en 14,955 s. L'unité `.30.cpp`
a le SHA-256
`2b7b608095f61c99f9860920b60050dc07bfb279408a31303a23b89d248eefd9`
et le runtime lié
`90838a482b9b1835b06b853669cd09a1b32bb92dd4d1ecb76b2d666221f17ad1`.
Le pseudo-symbole a disparu, tandis que l'appel du probe reste généré dans
`sub_822CE6C8`. Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal précédent `0x822CEB18 -> 0x822CE980` disparaît. Le smoke borné
avance vers :

```text
__imp__sub_822CE6C8
  -> Unresolved branch from 0x822CE7D4 to 0x822CE798
  -> __imp__sub_822CEDB8
  -> __imp__sub_822CF618
```

L'entrée configurée `0x822CE7B0` se situe dans cette boucle et devient le
prochain candidat exact à qualifier ; elle reste intacte dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 30,78 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 343 ;
- frontière `0x822CEB18 -> 0x822CE980` : fermée ;
- nouvelle frontière : `0x822CE7D4 -> 0x822CE798` ;
- entrée configurée suivante : `0x822CE7B0` dans `sub_822CE6C8` ;
- hook `ac6PacDynamicHeaderProbe` : conservé ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
