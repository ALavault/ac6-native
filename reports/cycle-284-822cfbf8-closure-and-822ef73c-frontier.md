# AC6 cycle 284 — fermeture `0x822CFBF8` et front `0x822EF73C`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x822CFBF8` est-elle indépendante ou une
  instruction du corps de la boucle interne de `sub_822CFB18` ?

## Preuve headless

Le vérificateur en lecture seule passe **38/38** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify822CFBF8Boundary.java
```

Il établit :

- le prologue et le frame `0xB0` à `0x822CFB18` ;
- l'initialisation des pointeurs, compteurs et constantes des deux boucles par
  cette entrée ;
- le vrai loop head interne à `0x822CFBE4` ;
- l'appel, le décrément `r28` et le store via `r29` avant l'adresse auditée ;
- `addi r29,r29,0x21` à `0x822CFBF8`, sans référence entrante ;
- le test immédiatement suivant et le backedge
  `0x822CFC00 -> 0x822CFBE4` ;
- le backedge externe `0x822CFC10 -> 0x822CFBAC`, puis le restore du frame ;
- un nouveau prologue indépendant à `0x822CFC20`.

Verdict : `0x822CFBF8` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x822CFBF8 = { name = "rex_sub_822CFBF8" }
```

- hash TOML avant :
  `7b29449017cfa38e41015e9f715cb9548cad8b9ee901f963782049acd0d5972b` ;
- hash TOML après :
  `3ce60b664ef6086dc38a017325df12fdaac9878809d8000c4874bb0e6f5b8fc7`.

Le codegen passe avec **23 338** fonctions en 13,116 s. Le runtime lié a le
SHA-256
`592098362b3f01b12394ed643900e1fb5272346ff055a1707bf0dc09421453e8`.
Le pseudo-symbole `rex_sub_822CFBF8` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Les deux fatals de boucle de `sub_822CFB18` disparaissent. Le smoke borné
avance vers :

```text
__imp__sub_822EF6D8
  -> Unresolved branch from 0x822EF75C to 0x822EF73C
  -> __imp__sub_822CB7F8
  -> __imp__sub_822CC048
```

L'entrée configurée `0x822EF758`, immédiatement avant le test/backedge,
devient le prochain candidat exact ; elle reste intacte dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 36,13 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 338 ;
- frontière `0x822CFC10 -> 0x822CFBAC` : fermée ;
- nouvelle frontière : `0x822EF75C -> 0x822EF73C` ;
- entrée configurée suivante : `0x822EF758` dans `sub_822EF6D8` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
