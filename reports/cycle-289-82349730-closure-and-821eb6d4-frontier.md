# AC6 cycle 289 — fermeture `0x82349730` et front `0x821EB6D4`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x82349730` est-elle indépendante ou le
  backedge interne de la boucle de `sub_823496D0` ?

## Preuve headless

Le vérificateur en lecture seule passe **40/40** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify82349730Boundary.java
```

Il établit :

- la fin explicite de la feuille précédente à `0x823496CC` ;
- l'entrée réelle à `0x823496D0`, ses validations et retours anticipés ;
- l'initialisation du pointeur `r10=r11+0x20`, du zéro `r8` et du compteur
  `r6=1` avant le vrai loop head `0x8234971C` ;
- le corps continu jusqu'à l'avance `r10 += 0x130` à `0x8234972C` ;
- `bge 0x8234971C` à `0x82349730`, sans référence entrante ;
- la continuation de la même feuille jusqu'au `blr` à `0x82349760` ;
- la prochaine fonction cadrée à `0x82349768`.

Verdict : `0x82349730` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x82349730 = { name = "rex_sub_82349730" }
```

- hash TOML avant :
  `6563202da7307618679e4cd416857570fb19897b8d3390bc25a8169e5b84b475` ;
- hash TOML après :
  `b73f56d22ada0646469166d71d9b9d81f7a11cfc942e34e5037829ed05164293`.

Le codegen passe avec **23 333** fonctions en 12,806 s. Le runtime lié a le
SHA-256
`aa928072e1771f94ac9317f16522f09f3740064620ed0328a85f68ce92598b10`.
Le pseudo-symbole `rex_sub_82349730` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal `0x82349730 -> 0x8234971C` disparaît. Le smoke borné avance vers :

```text
__imp__sub_821EB6A0
  -> Unresolved branch from 0x821EB71C to 0x821EB6D4
```

Cette fois, l'adresse source n'est pas une entrée configurée. La pseudo-entrée
configurée voisine `0x821EB6E0`, située à l'intérieur de la région séparant le
loop head `0x821EB6D4` du backedge `0x821EB71C`, devient le prochain candidat
à qualifier. Elle reste intacte dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 29,23 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 333 ;
- frontière `0x82349730 -> 0x8234971C` : fermée ;
- nouvelle frontière : `0x821EB71C -> 0x821EB6D4` ;
- entrée configurée suivante à auditer : `0x821EB6E0` dans la région de
  `sub_821EB6A0` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
