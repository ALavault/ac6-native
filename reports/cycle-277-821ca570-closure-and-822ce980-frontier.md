# AC6 cycle 277 — fermeture `0x821CA570` et front `0x822CE980`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821CA570` est-elle une cible d'appel
  indépendante ou l'instruction de branche interne de `sub_821CA538` ?

## Audit prudent de la cible

`0x821CA570` est bien une adresse enregistrée dans la table de dispatch du
codegen, mais l'instruction retail située à cette adresse est elle-même :

```text
0x821CA56C  cmpwi cr6,r31,0
0x821CA570  bge   cr6,0x821CA55C
```

Le corpus généré avant correction ne contient aucun appel direct à
`rex_sub_821CA570`; ses seules occurrences hors fatal sont sa déclaration et
son enregistrement de dispatch. Le backtrace retail atteint la branche dans
`__imp__sub_821CA538`, pas via la pseudo-fonction configurée.

## Preuve headless

Le vérificateur en lecture seule passe **24/24** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821CA570Boundary.java
```

Il établit :

- l'entrée réelle `0x821CA538` et son frame sauvegardé ;
- `r29=this`, `r30=this+4` et le compteur `r31=3` avant la boucle ;
- l'appel répété à `0x821CA560`, puis la décrémentation de `r31` et
  l'avancement de `r30` ;
- la production de `cr6` à `0x821CA56C` immédiatement avant la branche ;
- aucune référence entrante vers `0x821CA570` ;
- le fallthrough qui consomme encore `r29` et le restore helper à
  `0x821CA584` ;
- un nouveau frame indépendant à `0x821CA588`.

Verdict : `0x821CA570` est l'instruction de branche interne **confirmed**, pas
une entrée PPC indépendante.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821CA570 = { name = "rex_sub_821CA570" }
```

- hash TOML avant :
  `961cad973280f112fa4ab5f8c1b51c49c2e21073d17405ca7c2efe71401c78c4` ;
- hash TOML après :
  `ee1827b7da22fc64f83fd24ce30de60de6f5d87b4a9233b2484bef907fc7142e`.

Le codegen passe avec **23 345** fonctions en 12,915 s, puis le runtime se
lie avec `-j16`. L'unité `.14.cpp` a désormais le SHA-256
`8157ee12360ae1b5643b331b8726653061853003c3551592d77c75928c7a0f88`.
Le symbole `rex_sub_821CA570` et le fatal vers `0x821CA55C` ont disparu.
Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke borné atteint le code PPC puis reçoit `SIGABRT` :

```text
__imp__sub_822CE6C8
  -> Unresolved branch from 0x822CEB18 to 0x822CE980
  -> __imp__sub_822CEDB8
  -> __imp__sub_822CF618
```

La configuration contient `0x822CE9A8`, au milieu de cette famille. Cette
adresse est le prochain départ à qualifier ; elle reste intacte dans ce cycle.

## Validation native

- build AC6 `-j16` : PASS ;
- CTest : **48/48 PASS** en 37,41 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels AC6/GDB : 0 après clôture ;
- `git diff --check` racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 345 ;
- frontière `0x821CA55C` : fermée ;
- nouvelle frontière : `0x822CEB18 -> 0x822CE980` ;
- entrée configurée à auditer : `0x822CE9A8` dans `sub_822CE6C8` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
