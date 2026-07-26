# AC6 cycle 288 — fermeture `0x8234CA20` et front `0x8234971C`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x8234CA20` est-elle indépendante ou le
  backedge interne de la boucle de `sub_8234C9D8` ?

## Preuve headless

Le vérificateur en lecture seule passe **30/30** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify8234CA20Boundary.java
```

Il établit :

- la fin explicite de la feuille précédente à `0x8234C9D4` ;
- l'entrée feuille réelle à `0x8234C9D8`, qui initialise `r10`, les champs
  `r3+8/+12`, le compteur `r11` et ses deux retours anticipés ;
- le vrai loop head à `0x8234CA00` ;
- le corps lié continu jusqu'à la comparaison à `0x8234CA1C` ;
- `blt cr6,0x8234CA00` à `0x8234CA20`, sans référence entrante ;
- le `blr` réel à `0x8234CA24` ;
- la prochaine fonction cadrée à `0x8234CA28`.

Verdict : `0x8234CA20` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x8234CA20 = { name = "rex_sub_8234CA20" }
```

- hash TOML avant :
  `5023673df12d9f0d4303c8079219cd180e0999b41afa872d9f1fe03cd1dc3a36` ;
- hash TOML après :
  `6563202da7307618679e4cd416857570fb19897b8d3390bc25a8169e5b84b475`.

Le codegen passe avec **23 334** fonctions en 13,585 s. Le runtime lié a le
SHA-256
`f38a0a061e282ba3b78b227ec0515a425fcdf2397ef2b162bd7b6cc02387e956`.
Le pseudo-symbole `rex_sub_8234CA20` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal `0x8234CA20 -> 0x8234CA00` disparaît. Le smoke borné avance vers :

```text
__imp__sub_823496D0
  -> Unresolved branch from 0x82349730 to 0x8234971C
```

L'entrée configurée `0x82349730`, placée sur l'instruction de branche,
devient le prochain candidat exact ; elle reste intacte dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 33,69 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 334 ;
- frontière `0x8234CA20 -> 0x8234CA00` : fermée ;
- nouvelle frontière : `0x82349730 -> 0x8234971C` ;
- entrée configurée suivante : `0x82349730` dans `sub_823496D0` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
