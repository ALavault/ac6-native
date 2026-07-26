# AC6 cycle 295 — fermeture `0x821E6AC8` et front `0x821EDD60`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821E6AC8` est-elle indépendante ou une
  instruction interne de `sub_821E6A88`, entre la tête de boucle
  `0x821E6AA4` et le backedge `0x821E6AD8` ?

## Preuve headless

Le vérificateur en lecture seule passe **40/40** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821E6AC8Boundary.java
```

Il établit :

- l'unique prologue à `0x821E6A88`, sa frame de `0x80` octets, la capture de
  `r31` et le chargement de l'owner `r29` ;
- l'initialisation à 4 du compteur local `[r1+0x50]` ;
- huit instructions d'attente `or r31,r31,r31` à la tête interne
  `0x821E6AA4` ;
- le chargement du compteur à `0x821E6AC4`, sa décrémentation à
  `0x821E6AC8`, son stockage et son test avant le backedge
  `0x821E6AD8 -> 0x821E6AA4` ;
- aucune référence entrante et aucune entrée de fonction à `0x821E6AC8` ;
- la continuité de la même frame jusqu'à l'épilogue partagé
  `0x821E6B54..0x821E6B58`.

Verdict : `0x821E6AC8` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule pseudo-entrée supplémentaire a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821E6AC8 = { name = "rex_sub_821E6AC8" }
```

- hash TOML avant :
  `f0976f6c3733b014e55e1b1ad9b04ca25011c55487666e4673255704f45d5fbf` ;
- hash TOML après :
  `878ff2186126de614c97cb694bb9ab15b55e277632d7eb34425f48422046855f`.

La réinsertion en flux de cette ligne unique reproduit exactement le hash
avant. ReXGlue traite **23 327** fonctions en 14,855 s. Le runtime lié a le
SHA-256
`0762f859070d79d260d3a332b4de0d513843f67d82f21348df1e015db751c7b6`.
Le pseudo-symbole a disparu et le généré contient désormais le label interne
`loc_821E6AA4` dans `sub_821E6A88`. Aucun fichier généré n'a été modifié
manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke instrumenté borné ne contient plus le fatal
`0x821E6AD8 -> 0x821E6AA4`. Il avance et reçoit `SIGABRT` dans :

```text
__imp__sub_821EDD28
  -> Unresolved branch from 0x821EDDA4 to 0x821EDD60
```

Le généré localise le nouveau fatal à la boucle de cache-flush de
`sub_821EDD28`. La configuration contient l'entrée voisine
`0x821EDD68 = rex_sub_821EDD68`, au milieu de cette boucle. Elle reste intacte
et devient le prochain candidat exact ; cette observation runtime ne remplace
pas son audit headless propre.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif GCC `-j16` : PASS ;
- corpus GCC : **44/44 PASS** en 34,06 s ;
- build AC6 Clang/probes `-j16` : PASS ;
- corpus complet Clang/probes : **48/48 PASS** en 29,59 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 327 ;
- pseudo-entrée `0x821E6AC8` : fermée ;
- frontière `0x821E6AD8 -> 0x821E6AA4` : fermée ;
- nouvelle frontière runtime : `0x821EDDA4 -> 0x821EDD60` ;
- entrée configurée suivante à auditer : `0x821EDD68` dans
  `sub_821EDD28` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
