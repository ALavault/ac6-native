# AC6 cycle 283 — fermeture `0x822CFBA8` et front `0x822CFBAC`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x822CFBA8` est-elle indépendante ou la
  seconde instruction d'un chargement de constante interne à
  `sub_822CFB18` ?

## Preuve headless

Le vérificateur en lecture seule passe **39/39** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify822CFBA8Boundary.java
```

Il établit :

- le prologue et le frame `0xB0` à `0x822CFB18` ;
- l'initialisation du pointeur objet `r31`, des curseurs `r30/r29`, des
  compteurs `r21/r28` et des constantes `r23..r26` par cette entrée ;
- la paire contiguë `lis r11,-0x7DFF` à `0x822CFBA4`, puis
  `subi r23,r11,0x58B0` à `0x822CFBA8` ;
- aucune référence entrante vers `0x822CFBA8` ;
- la consommation immédiate de `r23` au vrai loop head `0x822CFBAC` ;
- les boucles imbriquées `0x822CFC00 -> 0x822CFBE4` et
  `0x822CFC10 -> 0x822CFBAC`, puis le restore du frame réel ;
- un nouveau prologue indépendant à `0x822CFC20`.

Verdict : `0x822CFBA8` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x822CFBA8 = { name = "rex_sub_822CFBA8" }
```

- hash TOML avant :
  `7e9a5df830fabd4791e2c05e28039d4c5befabb0a0bb24b2afb7894fb8a4e6b0` ;
- hash TOML après :
  `7b29449017cfa38e41015e9f715cb9548cad8b9ee901f963782049acd0d5972b`.

Le codegen passe avec **23 339** fonctions en 12,884 s. Le runtime lié a le
SHA-256
`442ff2b7cc3de7c8791a5ac0f6a20049e2c3320028136635d3d18516463f7060`.
Le pseudo-symbole `rex_sub_822CFBA8` a disparu ;
`rex_sub_822CFBF8` reste intact. Aucun fichier généré n'a été modifié
manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke borné atteint encore :

```text
__imp__sub_822CFB18
  -> Unresolved branch from 0x822CFC10 to 0x822CFBAC
  -> __imp__sub_822CC048
  -> __imp__sub_82213758
```

Le C++ régénéré contient les labels des deux boucles, mais la pseudo-entrée
restante `0x822CFBF8`, au milieu de la boucle interne, force encore
XenonRecomp à classer ses backedges comme externes. Elle devient le prochain
candidat exact et reste intacte dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 32,81 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 339 ;
- pseudo-entrée `0x822CFBA8` : fermée ;
- frontière runtime : `0x822CFC10 -> 0x822CFBAC` ;
- entrée configurée suivante : `0x822CFBF8` dans `sub_822CFB18` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
