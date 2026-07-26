# AC6 cycle 296 — fermeture `0x821EDD68` et front `0x821DE7DC`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821EDD68` est-elle une fonction PPC
  indépendante ou la seconde opération `dcbf` de la boucle interne
  `0x821EDD60..0x821EDDA4` appartenant à `sub_821EDD28` ?

## Preuve headless

Le vérificateur en lecture seule passe **48/48** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821EDD68Boundary.java
```

Il établit :

- l'unique entrée de fonction à `0x821EDD28` et le calcul des compteurs de
  lignes de cache avant la boucle ;
- la tête de boucle `0x821EDD60`, le chargement `li r8,0x80` à
  `0x821EDD64`, puis `dcbf r8,r11` à `0x821EDD68` ;
- l'absence de référence entrante et d'entrée de fonction à `0x821EDD68` ;
- les six autres couples `li`/`dcbf`, le compteur à `0x821EDD9C`, puis le
  backedge `0x821EDDA4 -> 0x821EDD60` ;
- la boucle résiduelle `0x821EDDB0..0x821EDDBC`, le `sync` et le `blr`
  partagés par la même fonction feuille.

Verdict : `0x821EDD68` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch, rollback et régénération

Une seule pseudo-entrée supplémentaire a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821EDD68 = { name = "rex_sub_821EDD68" }
```

- hash TOML avant :
  `878ff2186126de614c97cb694bb9ab15b55e277632d7eb34425f48422046855f` ;
- hash TOML après :
  `e069d124bc5212d16527f1e91395373dfd9c4b00d4e27f0eee025aeb5d9bf515`.

La réinsertion locale de cette ligne reproduit exactement le hash avant.
ReXGlue traite **23 326** fonctions en 13,363 s. Le runtime lié a le SHA-256
`ba5ec7935f2fdc6c039701ef5f7b0282cc31b41df3597221b11654370c89cd29`.
Le pseudo-symbole a disparu ; `sub_821EDD28` contient désormais le label
interne `loc_821EDD60` et le backedge généré est un `goto` normal. Aucun
fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke instrumenté borné ne contient plus le fatal
`0x821EDDA4 -> 0x821EDD60`. Il avance et reçoit `SIGABRT` sur :

```text
Unresolved branch from 0x821DE804 to 0x821DE7DC
```

Le généré rattache ce front à `sub_821DE7A8`. La configuration contient deux
entrées voisines, `0x821DE7D0` et `0x821DE7E8`. Elles restent intactes. Le
prochain audit exact doit commencer par `0x821DE7D0` et qualifier ensuite
`0x821DE7E8` seulement si la première preuve ne suffit pas à expliquer le
split de la boucle. Cette observation runtime n'est pas une preuve de leur
statut.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif GCC `-j16` : PASS ;
- corpus GCC : **44/44 PASS** en 33,97 s ;
- build AC6 Clang/probes `-j16` : PASS ;
- corpus complet Clang/probes : **48/48 PASS** en 29,65 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB/Xvfb : 0 ;
- gardes de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 326 ;
- pseudo-entrée `0x821EDD68` : fermée ;
- frontière `0x821EDDA4 -> 0x821EDD60` : fermée ;
- nouvelle frontière runtime : `0x821DE804 -> 0x821DE7DC` ;
- entrée configurée suivante à auditer : `0x821DE7D0`, avec
  `0x821DE7E8` conservée ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
