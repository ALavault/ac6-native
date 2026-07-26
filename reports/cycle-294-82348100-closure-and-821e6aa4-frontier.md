# AC6 cycle 294 — fermeture `0x82348100` et front `0x821E6AA4`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x82348100` est-elle indépendante ou une
  instruction interne de `sub_82348098`, entre la cible de reprise
  `0x823480F0` et la branche arrière `0x82348158` ?

## Preuve headless

Le vérificateur en lecture seule passe **45/45** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify82348100Boundary.java
```

Il établit :

- l'unique prologue à `0x82348098`, sa frame de `0x80` octets et la capture
  de `r29`/`r30` avant le calcul ;
- la préparation de `r28`/`r27`, puis l'entrée dans la queue commune à
  `0x8234814C` ;
- la cible interne `0x823480F0`, le chargement `r11 = [r31+0x10]` et le
  contrôle de `r30` juste avant `divwu r10,r11,r30` à `0x82348100` ;
- aucune référence entrante et aucune entrée de fonction à `0x82348100` ;
- la consommation continue de `r31`, `r29`, `r30` et `r28`, l'appel à
  `sub_82347F50`, puis le backedge `0x82348158 -> 0x823480F0` ;
- la même frame jusqu'à l'épilogue partagé `0x82348160..0x82348164`.

Verdict : `0x82348100` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule pseudo-entrée supplémentaire a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x82348100 = { name = "rex_sub_82348100" }
```

- hash TOML avant :
  `214985b4cf7c5f985d30a1d5e2158d4eeedaf7419ce7d6d7ecddd3c25a71a7ba` ;
- hash TOML après :
  `f0976f6c3733b014e55e1b1ad9b04ca25011c55487666e4673255704f45d5fbf`.

Le codegen ReXGlue passe avec **23 328** fonctions en 14,734 s. Le runtime
lié a le SHA-256
`ad62dc4aef557f4b6c73d08090e95f8582372a1da0a89b81a3c8257c322f2f26`.
Le pseudo-symbole et le fatal `0x82348158 -> 0x823480F0` ont disparu. Aucun
fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le même smoke borné franchit désormais `sub_82348098`, puis atteint :

```text
__imp__sub_821E6A88
  -> Unresolved branch from 0x821E6AD8 to 0x821E6AA4
```

La sortie générée montre une boucle de temporisation dans la frame de
`sub_821E6A88`, tête `0x821E6AA4`, branche arrière `0x821E6AD8`. La
configuration contient l'entrée voisine `0x821E6AC8 = rex_sub_821E6AC8`.
Elle est le prochain candidat exact, mais reste intacte : cette observation
runtime ne remplace pas son audit headless propre.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif GCC `-j16` : PASS ;
- corpus GCC : **44/44 PASS** en 33,23 s ;
- build AC6 Clang/probes `-j16` : PASS ;
- corpus complet Clang/probes : **48/48 PASS** en 28,98 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 328 ;
- pseudo-entrée `0x82348100` : fermée ;
- frontière `0x82348158 -> 0x823480F0` : fermée ;
- nouvelle frontière runtime : `0x821E6AD8 -> 0x821E6AA4` ;
- entrée configurée suivante à auditer : `0x821E6AC8` dans
  `sub_821E6A88` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
