# AC6 cycle 299 — fermeture `0x82345190` et front `0x82345200`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x82345190` est-elle une fonction PPC
  indépendante ou le branchement conditionnel interne de `sub_82345100` ?

## Preuve headless

Le vérificateur en lecture seule passe **41/41** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify82345190Boundary.java
```

Il établit :

- l'unique entrée ABI `0x82345100`, sa frame `0x120` et l'initialisation des
  registres des trois boucles imbriquées ;
- l'arrivée à `0x82345190` par flot direct depuis `cmplwi r9,0` à
  `0x8234518C`, sans référence entrante, prologue ni entrée de fonction ;
- l'utilisation de la même zone temporaire `r1+0x50`, des mêmes sources,
  compteurs et curseur de sortie avant et après cette adresse ;
- le backedge `0x82345214 -> 0x82345144`, les deux boucles externes et l'unique
  épilogue `0x82345328..0x8234532C` de `sub_82345100`.

Verdict : `0x82345190` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch, rollback et régénération

Une seule pseudo-entrée a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x82345190 = { name = "rex_sub_82345190" }
```

Les entrées `0x82345200`, `0x82345228`, `0x82345250`, `0x82345260` et
`0x823452A8` restent configurées sans modification.

- hash TOML avant :
  `11ffc03fe1785a0ed31593d6e40274015f834b1bee78d8b061914efabd43675b` ;
- hash TOML après :
  `fd6d05bd12199d792d8f8b16e04cc9f0b19db373c04810864e6bad62922e047f`.

La réinsertion locale de cette seule ligne reproduit exactement le hash
avant. ReXGlue traite **23 323** fonctions en 14,737 s. Le runtime lié a le
SHA-256
`81347d3117a9427104d1b279e84c23da34078022064c3fd727622f70054aa758`.
Le symbole `rex_sub_82345190` a disparu. Aucun fichier généré n'a été modifié
manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke instrumenté borné atteint encore :

```text
__imp__sub_82345100
  -> Unresolved branch from 0x82345214 to 0x82345144
```

Ce résultat n'est pas une répétition sans changement : il discrimine la
pseudo-entrée retirée de la seconde coupure, préservée à `0x82345200`. Le
généré contient `0x82345214` dans `rex_sub_82345200`, qui ne peut donc toujours
pas rejoindre la tête `0x82345144`. Le prochain audit exact est
`0x82345200`; aucune autre entrée de la famille n'est retirée par inférence.

## Validation native

- codegen ReXGlue : PASS, **23 323** fonctions ;
- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif GCC `-j16` : PASS ;
- corpus GCC : **44/44 PASS** en 33,47 s ;
- build AC6 Clang/probes `-j16` : PASS ;
- corpus complet Clang/probes : **48/48 PASS** en 29,29 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels propres au smoke `ac6recomp`/GDB/Xvfb : 0 ;
- gardes de diff racine et sous-dépôt : PASS.

L'instance Xvfb encore active appartient au run AC5 cycle 216 et n'a pas été
interrompue.

## État

- fonctions générées : 23 323 ;
- pseudo-entrée `0x82345190` : fermée ;
- frontière runtime : `0x82345214 -> 0x82345144`, encore ouverte ;
- prochaine entrée configurée à auditer : `0x82345200` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
