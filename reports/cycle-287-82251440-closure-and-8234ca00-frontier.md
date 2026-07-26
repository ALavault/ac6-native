# AC6 cycle 287 — fermeture `0x82251440` et front `0x8234CA00`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x82251440` est-elle indépendante ou le
  backedge interne de la boucle de `sub_82251388` ?

## Preuve headless

Le vérificateur en lecture seule passe **35/35** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify82251440Boundary.java
```

Il établit :

- les retours explicites précédents à `0x82251374..0x82251384` ;
- l'entrée feuille réelle à `0x82251388` et son initialisation du compteur,
  des pointeurs et constantes ;
- le vrai loop head à `0x822513EC` ;
- le corps continu de la boucle jusqu'à `0x8225143C` ;
- `bge cr6,0x822513EC` à `0x82251440`, sans référence entrante ;
- la continuation directe à `0x82251444`, toujours dans le même initialiseur,
  jusqu'au `blr` réel à `0x822514F8` ;
- la prochaine fonction cadrée à `0x82251500`.

Verdict : `0x82251440` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x82251440 = { name = "rex_sub_82251440" }
```

- hash TOML avant :
  `dfdfd02cbee88861514def09e97003570e809ad970f6ed392d9c7fd505c265e5` ;
- hash TOML après :
  `5023673df12d9f0d4303c8079219cd180e0999b41afa872d9f1fe03cd1dc3a36`.

Le codegen passe avec **23 335** fonctions en 13,699 s. Le runtime lié a le
SHA-256
`61554153c2441d66321a90e2cc35262bc25b5c1ed4ece8ab4bf8b1462bd10cf1`.
Le pseudo-symbole `rex_sub_82251440` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal `0x82251440 -> 0x822513EC` disparaît. Le smoke borné avance vers :

```text
__imp__sub_8234C9D8
  -> Unresolved branch from 0x8234CA20 to 0x8234CA00
```

L'entrée configurée `0x8234CA20`, placée sur l'instruction de branche,
devient le prochain candidat exact ; elle reste intacte dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 42,22 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 335 ;
- frontière `0x82251440 -> 0x822513EC` : fermée ;
- nouvelle frontière : `0x8234CA20 -> 0x8234CA00` ;
- entrée configurée suivante : `0x8234CA20` dans `sub_8234C9D8` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
