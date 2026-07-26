# AC6 cycle 285 — fermeture `0x822EF758` et front `0x82265D1C`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x822EF758` est-elle indépendante ou le
  test du compteur dans la boucle de `sub_822EF6D8` ?

## Preuve headless

Le vérificateur en lecture seule passe **40/40** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify822EF758Boundary.java
```

Il établit :

- le prologue et le frame `0x70` à `0x822EF6D8` ;
- l'initialisation de l'objet `r31` et du setup indirect par cette entrée ;
- l'initialisation du compteur `r30=3`, du pas `r29=0xA7E0` et du curseur
  `r31` avant le vrai loop head `0x822EF73C` ;
- l'appel virtuel, le décrément de `r30` et l'avance de `r31` dans le corps ;
- `cmplwi cr6,r30,0` à `0x822EF758`, sans référence entrante ;
- le backedge immédiatement suivant `0x822EF75C -> 0x822EF73C` ;
- le restore du frame, puis un nouveau prologue indépendant à `0x822EF768`.

Verdict : `0x822EF758` est une instruction interne **confirmed**, pas une
entrée PPC indépendante. L'entrée distincte `0x822EF708` reste intacte.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x822EF758 = { name = "rex_sub_822EF758" }
```

- hash TOML avant :
  `3ce60b664ef6086dc38a017325df12fdaac9878809d8000c4874bb0e6f5b8fc7` ;
- hash TOML après :
  `3373fa3421cabe275c591ab1b4ca7ebe19cd90cfe40d98ba909165f6f347a934`.

Le codegen passe avec **23 337** fonctions en 12,969 s. Le runtime lié a le
SHA-256
`fc9ce29dfa26e5c5aa715e36c24763cd3b8e2c8b661b08bd35e7dc4ef900ef4f`.
Le pseudo-symbole `rex_sub_822EF758` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal `0x822EF75C -> 0x822EF73C` disparaît. Le smoke borné avance vers :

```text
__imp__sub_82265CF8
  -> Unresolved branch from 0x82265D2C to 0x82265D1C
  -> __imp__sub_8226EFC8
  -> __imp__sub_82213758
```

L'entrée configurée `0x82265D20`, située au milieu du corps de cette boucle,
devient le prochain candidat exact ; elle reste intacte dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 33,84 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 337 ;
- frontière `0x822EF75C -> 0x822EF73C` : fermée ;
- nouvelle frontière : `0x82265D2C -> 0x82265D1C` ;
- entrée configurée suivante : `0x82265D20` dans `sub_82265CF8` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
