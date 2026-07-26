# AC6 cycle 297 — fermeture `0x821DE7D0` et front `0x821DE7E8`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821DE7D0` est-elle une fonction PPC
  indépendante ou le premier chargement de la boucle de construction de
  déclaration appartenant au helper `0x821DE7A8` ?

## Preuve headless

Le vérificateur en lecture seule passe **51/51** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821DE7D0Boundary.java
```

Il établit :

- l'unique entrée ABI `0x821DE7A8`, son appel au helper de sauvegarde des GPR,
  sa frame `0x90` et la conservation des arguments source/destination dans
  `r29/r31` ;
- la dépendance directe de `0x821DE7D0` (`lhz r11,0(r29)`) envers ce prologue,
  sans référence entrante ni entrée de fonction propre ;
- la boucle de scan des records source de 12 octets, terminée par le sentinel
  `0xff`, avec branche arrière `0x821DE804 -> 0x821DE7DC` ;
- la branche `0x821DE7E0 -> 0x821DE7E8`, enregistrée sans qualifier ni retirer
  l'entrée voisine `0x821DE7E8` ;
- l'allocation et la copie des records vers `destination+0x34`, puis l'unique
  épilogue à `0x821DE88C..0x821DE890`.

Verdict : `0x821DE7D0` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch, rollback et régénération

Une seule pseudo-entrée supplémentaire a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821DE7D0 = { name = "rex_sub_821DE7D0" }
```

Le wrapper transparent correspondant dans `src/d3d_hooks.cpp`, qui ne
capturait plus aucun état depuis le cycle 250, a également été retiré afin de
ne pas référencer un symbole généré inexistant. `0x821DE7E8` est restée
strictement configurée.

- hash TOML avant :
  `e069d124bc5212d16527f1e91395373dfd9c4b00d4e27f0eee025aeb5d9bf515` ;
- hash TOML après :
  `5e763c69f768efbc71a39e5fa41e85f70f60a0a206396378f8e768706234e0dc`.

La réinsertion locale de la seule ligne TOML reproduit exactement le hash
avant. ReXGlue traite **23 325** fonctions en 13,188 s. Le runtime lié a le
SHA-256
`ccf75ad50a33b39c21b759375edf696a7acf380331aeb64d4f613c10b3bf9969`.
Le symbole `rex_sub_821DE7D0` a disparu ; aucun fichier généré n'a été modifié
manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke instrumenté borné atteint encore `SIGABRT` dans
`__imp__sub_821DE7A8` sur :

```text
Unresolved branch from 0x821DE804 to 0x821DE7DC
```

Cette absence d'avancement est informative : la suppression de `0x821DE7D0`
réunit bien le prologue et le corps dans `sub_821DE7A8`, mais l'entrée
configurée préservée `0x821DE7E8` coupe encore le scan. Le généré appelle
`rex_sub_821DE7E8` depuis la branche `0x821DE7E0`, puis duplique la suite avec
le même fatal arrière. Le prochain audit exact est donc `0x821DE7E8`, contre
`0x821DE7E0 -> 0x821DE7E8` et `0x821DE804 -> 0x821DE7DC`. Cette observation
runtime ne suffit pas à elle seule à retirer cette entrée.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif GCC `-j16` : PASS ;
- corpus GCC : **44/44 PASS** en 50,02 s ;
- build AC6 Clang/probes `-j16` : PASS ;
- corpus complet Clang/probes : **48/48 PASS** en 46,39 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB/Xvfb : 0 ;
- gardes de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 325 ;
- pseudo-entrée `0x821DE7D0` : fermée ;
- wrapper transparent `rex_sub_821DE7D0` : retiré ;
- frontière runtime : `0x821DE804 -> 0x821DE7DC`, encore ouverte ;
- prochaine entrée configurée à auditer : `0x821DE7E8` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
