# AC6 cycle 276 — fermeture `0x822CFCE8` et front `0x821CA55C`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : `0x822CFCE8` est-elle une entrée réelle ou un départ interne à
  la boucle de construction de `sub_822CFCA8` ?

## Preuve headless

Le vérificateur en lecture seule passe **29/29** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify822CFCE8Boundary.java
```

Il établit :

- l'entrée réelle `0x822CFCA8`, son frame sauvegardé et `r31=this` ;
- l'initialisation des objets embarqués, de `r27/r29` et du compteur `r30=7` ;
- la tête `0x822CFCE0` et l'appel constructeur à `0x822CFCE4` ;
- à `0x822CFCE8`, la décrémentation du compteur, sans référence entrante ;
- l'avancement de `r29` par `0xC60` et la branche
  `0x822CFCF4 -> 0x822CFCE0` ;
- les stores post-boucle qui publient les objets à partir de `r27/r28/r31` ;
- le restore helper à `0x822CFD40` ;
- un nouveau frame indépendant à `0x822CFD48`.

Verdict : `0x822CFCE8` est un départ interne **confirmed**.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x822CFCE8 = { name = "rex_sub_822CFCE8" }
```

- hash TOML avant :
  `ba50e515b25a533b352af4755dfb8bb8d67d588a2080f3013f516c5f2f55c418` ;
- hash TOML après :
  `961cad973280f112fa4ab5f8c1b51c49c2e21073d17405ca7c2efe71401c78c4`.

Le codegen passe avec **23 346** fonctions en 17,077 s, puis le runtime se
lie avec `-j16`. L'unité `.30.cpp` a désormais le SHA-256
`4b337adb89f5d533684d286d4fc27193f5b3562a57432c5360de9a9800c6f880`.
Le symbole `rex_sub_822CFCE8` et le fatal vers `0x822CFCE0` ont disparu.
Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke borné atteint le code PPC puis reçoit `SIGABRT` :

```text
__imp__sub_821CA538
  -> Unresolved branch from 0x821CA570 to 0x821CA55C
  -> __imp__sub_823D2688
  -> __imp__sub_821F7AE8
```

La configuration contient `0x821CA570`, au milieu de cette famille. Cette
adresse est le prochain départ à qualifier ; elle reste intacte dans ce cycle.

## Validation native

- build AC6 `-j16` : PASS ;
- CTest : **48/48 PASS** en 31,83 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels AC6/GDB : 0 après clôture ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 346 ;
- frontière `0x822CFCE0` : fermée ;
- nouvelle frontière : `0x821CA570 -> 0x821CA55C` ;
- entrée configurée à auditer : `0x821CA570` dans `sub_821CA538` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
