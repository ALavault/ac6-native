# AC6 cycle 290 — fermeture `0x821EB6E0` et front `0x821DD898`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821EB6E0` est-elle indépendante ou une
  instruction interne de la boucle `0x821EB6D4..0x821EB71C` ?

## Preuve headless

Le vérificateur en lecture seule passe **41/41** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821EB6E0Boundary.java
```

Il établit :

- la fin cadrée de la fonction précédente avant `0x821EB6A0` ;
- le vrai prologue à `0x821EB6A0` et l'initialisation de `r3`, `r8`, `r9`,
  `r10` et `r11` avant la boucle ;
- le loop head à `0x821EB6D4` et le compteur de `0x100` itérations ;
- `lhzx r7,r7,r3` à `0x821EB6E0`, sans référence entrante, après la rotation
  d'index à `0x821EB6DC` ;
- le corps continu jusqu'au backedge `0x821EB71C -> 0x821EB6D4` ;
- la restauration de frame et le `blr` à `0x821EB734` ;
- la prochaine fonction cadrée à `0x821EB738`.

Verdict : `0x821EB6E0` est une instruction interne **confirmed**, pas une
entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821EB6E0 = { name = "rex_sub_821EB6E0" }
```

- hash TOML avant :
  `b73f56d22ada0646469166d71d9b9d81f7a11cfc942e34e5037829ed05164293` ;
- hash TOML après :
  `16f78a04a5df914c0e0e0216ee0f3876e841f76fa98dbe5761b1019b085fe1a4`.

Le codegen passe avec **23 332** fonctions en 14,035 s. Le runtime lié a le
SHA-256
`220f6cd4d8e9df46d4a341469aaa40d2ca6d73642ed505243c241a1f45a69ebe`.
Le pseudo-symbole `rex_sub_821EB6E0` a disparu. Aucun fichier généré n'a été
modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le fatal `0x821EB71C -> 0x821EB6D4` disparaît. Le smoke borné avance vers :

```text
__imp__sub_821DD860
  -> Unresolved branch from 0x821DD8B4 to 0x821DD898
```

L'adresse source n'est pas configurée. La pseudo-entrée configurée
`0x821DD8A0`, située dans la région de boucle entre sa cible et son backedge,
devient le prochain candidat exact ; elle reste intacte dans ce cycle.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 29,47 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 332 ;
- frontière `0x821EB71C -> 0x821EB6D4` : fermée ;
- nouvelle frontière : `0x821DD8B4 -> 0x821DD898` ;
- entrée configurée suivante à auditer : `0x821DD8A0` dans la région de
  `sub_821DD860` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
