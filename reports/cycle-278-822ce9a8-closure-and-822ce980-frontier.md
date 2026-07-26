# AC6 cycle 278 — fermeture `0x822CE9A8` et front `0x822CE980`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x822CE9A8` est-elle une fonction PPC
  indépendante ou un store interne de la boucle de `sub_822CE6C8` ?

## Preuve headless

Le vérificateur en lecture seule passe **32/32** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify822CE9A8Boundary.java
```

Il établit :

- le vrai prologue sauvegardé à `0x822CE6C8` et son frame de `0x100` octets ;
- l'initialisation de `r31`, `r27` et `r26` par cette entrée ;
- la production du compteur `r27` dans la passe précédente ;
- les constantes et bases de tables préparées avant `0x822CE980` ;
- la charge indexée de `r10` à `0x822CE9A0`, immédiatement consommée par
  `sth r10,0x54(r1)` à `0x822CE9A8` ;
- aucune référence entrante vers `0x822CE9A8` ;
- l'avancement `r27/r26`, la branche arrière
  `0x822CEB18 -> 0x822CE980` et le restore du frame d'origine.

Verdict : `0x822CE9A8` est un store interne **confirmed**, dépendant du frame
et des registres de `sub_822CE6C8`, pas une entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x822CE9A8 = { name = "rex_sub_822CE9A8" }
```

- hash TOML avant :
  `ee1827b7da22fc64f83fd24ce30de60de6f5d87b4a9233b2484bef907fc7142e` ;
- hash TOML après :
  `ef782e9c38c5766460dddc7224b33793083a08a0cc7b844aef50c44e656dd158`.

Le codegen passe avec **23 344** fonctions en 13,267 s, puis le runtime se
lie avec `-j16`. L'unité générée `.30.cpp` a le SHA-256
`90f3118343a5e7df1af6659566ce977dcef52638c4078d28070818a51376c9b8` et
le runtime lié
`a2929e0bd31fc326dfa19a90e4486b8e9fccef8beb01ffe4bf03ab37a788414b`.
Le pseudo-symbole `rex_sub_822CE9A8` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke borné entre bien dans le PPC régénéré, puis reçoit `SIGABRT` :

```text
__imp__sub_822CE6C8
  -> Unresolved branch from 0x822CEB18 to 0x822CE980
  -> __imp__sub_822CEDB8
  -> __imp__sub_822CF618
```

La frontière n'avance donc pas encore : un second split configuré de la même
boucle demeure à `0x822CEAC0`. C'est le prochain candidat exact à qualifier ;
il reste intact dans ce cycle. Ce résultat n'est pas présenté comme une
fermeture du fatal, seulement comme la suppression prouvée d'une fausse entrée.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 30,89 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 après clôture ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 344 ;
- fausse entrée `0x822CE9A8` : fermée ;
- frontière active inchangée : `0x822CEB18 -> 0x822CE980` ;
- entrée configurée suivante à auditer : `0x822CEAC0` dans
  `sub_822CE6C8` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
