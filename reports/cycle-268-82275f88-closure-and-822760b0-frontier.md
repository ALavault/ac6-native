# AC6 cycle 268 — fermeture `0x82275F88` et front `0x822760B0`

## Identité

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000`.

## Archive reçue et absence de double application

La nouvelle archive `ac6_82275f78_boundary_audit_v1.zip` a le SHA-256
`2d62c6886342ec8c1c46539507c609314eaa6a5cb205aa402ffd4f961ee0f21d`.
`unzip -t` passe, les 11 membres sont des fichiers relatifs ordinaires et les
10 charges utiles référencées par `SHA256SUMS` passent. Les neuf tests Python
rapportés par l'archive passent.

Le paquet qualifie exactement le même changement que l'audit local : retirer
uniquement l'entrée configurée
`0x82275F88 = { name = "rex_sub_82275F88" }`. Son hash d'entrée est
`a3682cd89d4ff582d3516c9d6ee23888e6b3d426c5f5d67d30166aaf438d6a1e`
et son hash de sortie attendu est
`79e90e105a13e71596538a506dd5e786700e0dd03dfed3d9561ec72b054e76f8`.
Le TOML local avait déjà exactement ce dernier hash lors de la qualification de
l'archive : le patch n'a donc pas été réappliqué.

## Preuve headless indépendante

Le script versionné `scripts/Verify82275F88Boundary.java`, exécuté en lecture
seule contre le projet Ghidra canonique, passe 19 assertions. Il confirme :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify82275F88Boundary.java
```

- prologue et frame autonomes à `0x82275F60` ;
- tête de boucle `0x82275F78` ;
- fallthrough `0x82275F84 -> 0x82275F88` sans référence entrante ;
- état vivant `r31`, `r30`, `r4`, `r5` et `r6` préparé avant `0x82275F88` ;
- branche arrière `0x82275FB4 -> 0x82275F78` ;
- restore tail apparié à `0x82275FC0` ;
- nouveau prologue autonome à `0x82275FC8`.

L'entrée `0x82275F88` est donc un départ interne confirmé. Aucun nom de jeu ni
aucune sémantique de gameplay n'est déduit de ce verdict de CFG.

## Régénération et build

```bash
cmake --build .tools/ac6-recomp-reference/out/build/linux-amd64-runtime-localdev \
  --target ac6recomp_codegen -j16
cmake --build .tools/ac6-recomp-reference/out/build/linux-amd64-runtime-localdev \
  --target ac6recomp -j16
```

Les deux commandes passent. Le codegen traite 23 354 fonctions en 15,025 s.
L'unité `.24.cpp` régénérée a le SHA-256
`76a408ac7d8d43b5388af6b1377d50d37e00f28692f2e016928ab60af2454ffa`.
Ni `rex_sub_82275F88`, ni le fatal
`0x82275FB4 -> 0x82275F78` ne subsistent. Aucun `generated/*.cpp` n'a été
modifié manuellement.

Le binaire Linux lié est un ELF x86-64 de 175 712 208 octets. Le build émet
des avertissements existants de dépendances et de généré, mais aucune erreur.

## Smoke retail borné et nouveau front

Un lancement direct sans serveur d'affichage échoue avant le jeu avec
`Failed to initialize GTK+`; ce résultat n'est pas retenu comme preuve PPC.
Sous Xvfb, le processus atteint `Ac6recompApp` puis termine par `SIGABRT 134`.
Une seule reprise GDB bornée identifie précisément la pile :

```text
__imp__sub_82276098
  -> Unresolved branch from 0x822760EC to 0x822760B0
  -> __imp__sub_821F7AE8
  -> __imp__xstart
```

Le généré montre un prologue à `0x82276098`, une tête de boucle à
`0x822760B0`, la branche arrière à `0x822760EC`, puis le restore tail. La
configuration contient `0x822760B8`, situé dans ce suffixe. Il s'agit de la
prochaine entrée suspecte, mais elle n'est pas retirée dans ce cycle : elle
doit recevoir le même audit indépendant de limites et de références.

Aucune interaction humaine, GUI Ghidra ou exécution Xenia n'a été nécessaire.

## Validation native élargie

```bash
cmake --build .build/ace-combat-6-clang-probes -j16
ctest --test-dir .build/ace-combat-6-clang-probes \
  --output-on-failure -j16
cmake --install .build/ace-combat-6-clang-probes --prefix "$PWD"
test ! -e bin/bin
```

- build : PASS ;
- CTest : **48/48 PASS** en 29,90 s ;
- probes XenonRecomp NDXR/LHBRX/DCBST/transformation : PASS ;
- catalogue shader-cache Xenia retail : PASS ;
- installation racine : PASS, sans `bin/bin`.

## Couverture et état

- fonction générée : 23 354 ;
- frontière runtime `0x82275F78` : fermée ;
- tests natifs exécutés : 48/48 ;
- produit AC6 complet/playable : non démontré ;
- statut : `candidate`, pas `verified` au niveau produit.

Le plan durable est `../DECOMPILATION_PLAN.md`. La prochaine tranche est
l'audit borné de la famille `0x82276098..restore`, avec question unique :
`0x822760B8` est-elle une entrée réelle ou un fallthrough interne ?
