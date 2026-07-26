# AC6 cycle 298 — fermeture `0x821DE7E8` et front `0x82345144`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821DE7E8` est-elle une fonction PPC
  indépendante ou la cible conditionnelle interne du scan de déclarations
  appartenant à `sub_821DE7A8` ?

## Preuve headless

Le vérificateur en lecture seule passe **43/43** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821DE7E8Boundary.java
```

Il établit :

- l'unique entrée ABI `0x821DE7A8`, sa frame `0x90` et l'initialisation des
  registres `r10`, `r28`, `r29`, `r30` et `r31` consommés après le split ;
- l'unique référence entrante de `0x821DE7E8`, la branche conditionnelle
  `0x821DE7E0 -> 0x821DE7E8` ;
- l'absence d'entrée de fonction à `0x821DE7E8` ;
- le backedge `0x821DE804 -> 0x821DE7DC`, qui réutilise la même tête de boucle,
  les mêmes compteurs et la même frame ;
- l'allocation, la copie de records et l'unique épilogue
  `0x821DE88C..0x821DE890` communs à `sub_821DE7A8` ;
- l'instruction à `0x821DE8D8` est seulement enregistrée et n'est ni classée
  ni modifiée dans ce cycle.

Verdict : `0x821DE7E8` est une cible interne **confirmed**, pas une entrée PPC
indépendante.

## Patch, rollback et régénération

Une seule pseudo-entrée a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821DE7E8 = { name = "rex_sub_821DE7E8" }
```

`0x821DE8D8` reste configurée sans modification.

- hash TOML avant :
  `5e763c69f768efbc71a39e5fa41e85f70f60a0a206396378f8e768706234e0dc` ;
- hash TOML après :
  `11ffc03fe1785a0ed31593d6e40274015f834b1bee78d8b061914efabd43675b`.

La réinsertion locale de la seule ligne TOML reproduit exactement le hash
avant. ReXGlue traite **23 324** fonctions en 14,092 s. Le runtime lié a le
SHA-256
`d3c3d0bf4b7b020e4cca4151a03adec408de1fd87bc767e706b6b1b10744ccbc`.
Le symbole `rex_sub_821DE7E8` a disparu et le généré contient maintenant un
`goto loc_821DE7DC` normal. Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke instrumenté borné ne contient plus le fatal
`0x821DE804 -> 0x821DE7DC`. Il avance jusqu'à :

```text
__imp__sub_82345100
  -> Unresolved branch from 0x82345214 to 0x82345144
```

Le généré montre une boucle interne dans `sub_82345100`, dont la tête est
`0x82345144` et le backedge `0x82345214`. La première entrée configurée qui
coupe cette fonction est `0x82345190`; elle devient le prochain audit exact.
Les entrées voisines `0x82345200`, `0x82345228`, `0x82345250`, `0x82345260` et
`0x823452A8` restent intactes jusqu'à leurs propres preuves.

## Validation native

- codegen ReXGlue : PASS, **23 324** fonctions ;
- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif GCC `-j16` : PASS ;
- corpus GCC : **44/44 PASS** en 33,27 s ;
- build AC6 Clang/probes `-j16` : PASS ;
- corpus complet Clang/probes : **48/48 PASS** en 29,64 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels propres au smoke `ac6recomp`/GDB/Xvfb : 0 ;
- gardes de diff racine et sous-dépôt : PASS.

Un Xvfb actif observé après le smoke appartient au run AC5 cycle 216, pas à
ce cycle, et n'a donc pas été interrompu.

## État

- fonctions générées : 23 324 ;
- pseudo-entrée `0x821DE7E8` : fermée ;
- frontière `0x821DE804 -> 0x821DE7DC` : fermée ;
- nouvelle frontière runtime : `0x82345214 -> 0x82345144` ;
- prochaine entrée configurée à auditer : `0x82345190` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
