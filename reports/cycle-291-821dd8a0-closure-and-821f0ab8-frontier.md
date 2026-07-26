# AC6 cycle 291 — fermeture `0x821DD8A0` et front `0x821F0AB8`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821DD8A0` est-elle indépendante ou une
  instruction interne de la boucle `0x821DD898..0x821DD8B4` ?

## Preuve headless

Le vérificateur en lecture seule passe **47/47** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821DD8A0Boundary.java
```

Il établit :

- la fin cadrée de la fonction précédente à `0x821DD85C` ;
- le vrai prologue de `sub_821DD860`, sa frame de `0x670` octets et les
  pointeurs de boucle préparés à `0x821DD888..0x821DD894` ;
- le loop head `0x821DD898`, qui charge le premier octet comparé ;
- `subf. r8,r7,r8` à `0x821DD8A0`, sans référence entrante, suivi du branchement
  de divergence `0x821DD8A4` ;
- le backedge `0x821DD8B4 -> 0x821DD898` et la continuation dans la même frame ;
- la restauration de frame et le `blr` à `0x821DD920` ;
- le prochain prologue cadré à `0x821DD928`.

Verdict : `0x821DD8A0` est une instruction de comparaison interne
**confirmed**, pas une entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821DD8A0 = { name = "rex_sub_821DD8A0" }
```

- hash TOML avant :
  `16f78a04a5df914c0e0e0216ee0f3876e841f76fa98dbe5761b1019b085fe1a4` ;
- hash TOML après :
  `a069fc66d35b83e7033c34bead3c9181999cfffadebad7e5bddc1cb78fb13b35`.

Le codegen passe avec **23 331** fonctions en 13,237 s. Le runtime lié a le
SHA-256
`674fcf746187b5d4b45989c4ea8fc432e69771f5a4413dce5251f7cc61f1fb93`.
Le pseudo-symbole `rex_sub_821DD8A0` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal `0x821DD8B4 -> 0x821DD898` disparaît. Le smoke borné avance vers :

```text
__imp__sub_821F0A90
  -> Unresolved branch from 0x821F0B00 to 0x821F0AB8
```

L'adresse source n'est pas configurée. Les pseudo-entrées configurées
`0x821F0AC8` et `0x821F0AD0` se trouvent dans la région comprise entre la cible
et le backedge. `0x821F0AC8`, la première rencontrée, devient le prochain
candidat exact ; les deux restent intactes dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 29,29 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 331 ;
- frontière `0x821DD8B4 -> 0x821DD898` : fermée ;
- nouvelle frontière : `0x821F0B00 -> 0x821F0AB8` ;
- entrée configurée suivante à auditer : `0x821F0AC8` dans la région de
  `sub_821F0A90`, avec `0x821F0AD0` également préservée ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
