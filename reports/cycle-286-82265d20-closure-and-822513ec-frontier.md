# AC6 cycle 286 — fermeture `0x82265D20` et front `0x822513EC`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x82265D20` est-elle indépendante ou le
  store du corps de boucle de `sub_82265CF8` ?

## Preuve headless

Le vérificateur en lecture seule passe **25/25** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify82265D20Boundary.java
```

Il établit :

- la fin explicite de la fonction précédente à `0x82265CF4` ;
- l'entrée feuille réelle à `0x82265CF8` ;
- l'initialisation de la vtable, du pointeur `r10`, de la valeur zéro `r9`,
  de la constante flottante et du compteur `r11=0x271` par cette entrée ;
- le vrai loop head à `0x82265D1C` ;
- `stw r9,0(r10)` à `0x82265D20`, sans référence entrante ;
- l'avance de huit octets, le test et le backedge
  `0x82265D2C -> 0x82265D1C` ;
- le `blr` réel à `0x82265D30`, puis une autre fonction feuille alignée à
  `0x82265D38`.

Verdict : `0x82265D20` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x82265D20 = { name = "rex_sub_82265D20" }
```

- hash TOML avant :
  `3373fa3421cabe275c591ab1b4ca7ebe19cd90cfe40d98ba909165f6f347a934` ;
- hash TOML après :
  `dfdfd02cbee88861514def09e97003570e809ad970f6ed392d9c7fd505c265e5`.

Le codegen passe avec **23 336** fonctions en 12,912 s. Le runtime lié a le
SHA-256
`37c194692cff1c55eb06ff43a316c01a04f6f84e3009c4b6ae15bbb4ecc3190c`.
Le pseudo-symbole `rex_sub_82265D20` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal `0x82265D2C -> 0x82265D1C` disparaît. Le smoke borné avance vers :

```text
__imp__sub_82251388
  -> Unresolved branch from 0x82251440 to 0x822513EC
  -> __imp__sub_82213758
```

L'entrée configurée `0x82251440`, placée sur l'instruction de branche
elle-même, devient le prochain candidat exact ; elle reste intacte dans ce
cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 29,23 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 336 ;
- frontière `0x82265D2C -> 0x82265D1C` : fermée ;
- nouvelle frontière : `0x82251440 -> 0x822513EC` ;
- entrée configurée suivante : `0x82251440` dans `sub_82251388` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
