# AC6 cycle 293 — fermeture `0x821F0AD0` et front `0x823480F0`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821F0AD0` est-elle indépendante ou une
  instruction interne de la boucle `0x821F0AB8..0x821F0B00` ?

## Preuve headless

Le vérificateur en lecture seule passe **46/46** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821F0AD0Boundary.java
```

Il établit :

- la fin de la fonction précédente à `0x821F0A8C` ;
- l'unique prologue à `0x821F0A90`, sa frame de `0xB0` octets et les deux
  curseurs préparés avant le loop head `0x821F0AB8` ;
- `rlwinm r11,r11,2,0,29` à `0x821F0AD0`, sans référence entrante ;
- la dépendance directe au calcul `0x821F0AC8..0x821F0ACC`, puis les écritures
  indexées, l'appel indirect et le backedge `0x821F0B00 -> 0x821F0AB8` ;
- aucune entrée de fonction Ghidra au loop head, au candidat ou au backedge ;
- la continuation dans la même frame jusqu'aux boucles suivantes.

Verdict : `0x821F0AD0` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule ligne supplémentaire a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821F0AD0 = { name = "rex_sub_821F0AD0" }
```

- hash TOML avant :
  `23557c0c4a3b7ab27d8f198baa0b2ad4b9f084a91e6c7d3f1dd5eecb74f3a7ea` ;
- hash TOML après :
  `214985b4cf7c5f985d30a1d5e2158d4eeedaf7419ce7d6d7ecddd3c25a71a7ba`.

Le codegen passe avec **23 329** fonctions en 12,915 s. Le runtime lié a le
SHA-256
`2d535d98d4127f889274cdb007fe841cd9c97afe8299b007fdd7c98d13444eae`.
Le pseudo-symbole `rex_sub_821F0AD0` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal précédent disparaît entièrement. Le smoke borné franchit
`sub_821F0A90`, puis atteint :

```text
__imp__sub_82348098
  -> Unresolved branch from 0x82348158 to 0x823480F0
```

La sortie générée montre que `0x823480F0` est déjà un label interne alimenté
par le résultat de `sub_82347F50`, mais l'entrée configurée voisine
`0x82348100 = rex_sub_82348100` coupe encore ce corps. Elle devient le prochain
candidat exact. Elle reste intacte dans ce cycle : cette observation runtime
ne remplace pas son audit headless propre.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- corpus natif GCC : **44/44 PASS** en 52,29 s ;
- corpus complet Clang/probes : **48/48 PASS** en 52,45 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 329 ;
- pseudo-entrée `0x821F0AD0` : fermée ;
- frontière `0x821F0B00 -> 0x821F0AB8` : fermée ;
- nouvelle frontière runtime : `0x82348158 -> 0x823480F0` ;
- entrée configurée suivante à auditer : `0x82348100` dans
  `sub_82348098` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
