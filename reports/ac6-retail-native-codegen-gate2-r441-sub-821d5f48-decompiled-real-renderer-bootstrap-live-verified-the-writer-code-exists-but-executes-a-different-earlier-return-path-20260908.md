# AC6 retail NTSC-U/J — r441 — `sub_821D5F48` décompilée : bootstrap Xenos/renderer très clair (résolution 1280×720, tailles de tampon en dur), avec un écrivain réel de `*0x82935D98` identifié dans le code source généré — vérifié EN DIRECT : ce code existe mais l'exécution observée emprunte un chemin de retour ANTÉRIEUR qui ne l'atteint jamais, réconciliant pleinement avec la preuve de r437

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r440 : décompiler `sub_821D5F48` (l'étape d'initialisation
« doit réussir » juste avant `sub_821D6C20`) et les fonctions
d'assistance de lecture de réglages, pour tester l'hypothèse
« manager de rendu/GPU ».

## Outil ajouté

`scripts/DecompileD5F48SettingsHelpers.java` — même méthode que
`DecompileD6C20Dispatcher.java` (r440) : désassemble et décompile
`sub_821D5F48` et cinq fonctions d'assistance
(`sub_82234D40`, `sub_821CC008`, `sub_821CC508`, `sub_82222D80`,
`sub_82222E08`), bornées par les points de coupure du codegen retail
qualifié, verrouillé au bon SHA. Committé pour réutilisation.

## Établi — décompilation, confirme fortement l'hypothèse « bootstrap Xenos/renderer »

Le début de `sub_821D5F48` contient une longue séquence d'appels avec
des arguments qui ne laissent guère de doute :
```c
Function_82331C60(0x1800000,0x3d7ac00);
Function_82331CA8(0x500,0x2d0,4,0,1);   // 0x500=1280, 0x2d0=720 — LA résolution du jeu
Function_82331ED0(0,0);
func_0x82331d18(0xb00000);
Function_823359F0(0,0x500);
Function_823376F8(0x100000,0x400);
Function_82337F68(0x800000,0x780);
...
```
Des tailles de tampon en Mio et la résolution d'affichage réelle du
jeu (`1280×720`) codées en dur dans les arguments — **ceci est, sans
ambiguïté raisonnable, le bootstrap du sous-système Xenos/renderer du
jeu**, pas une routine générique. Confirme et dépasse l'hypothèse de
r440.

`sub_82234D40` (l'assistant de lecture appelé ~35 fois par
`sub_821D6C20`) retourne un **pointeur** (`base + offset[index]`),
pas un scalaire — cohérent avec une table de ressources/shaders
indexée. `sub_821CC008` énumère des **fichiers réels par chemin**
(traduction `/`→`\`, ouverture, taille, fermeture) — cohérent avec une
énumération d'assets réels précédant l'allocation d'un pool de rendu.

## Établi — un écrivain réel de `*0x82935D98` existe dans le source généré

Une relecture plus attentive de la même zone déjà examinée par r436
(le fichier contenant `23960`) montre une occurrence manquée
précédemment : à `ppc_recomp.23.cpp:20257`, un `addi r11,r11,23960`
(PAS un `lwz`) suivi de `stw r3,0(r11)` :
```c
// lis r11,-32109
r11.s64 = -2104295424;
// addi r11,r11,23960
r11.s64 = r11.s64 + 23960;
// stw r3,0(r11)
PPC_STORE_U32(r11.u32 + 0, ctx.r3.u32);
```
`r11 = 0x82930000 + 23960 = 0x82935D98`. **Ceci écrit bien
`ctx.r3` à l'adresse exacte que `sub_821D6C20` lit.** Cette
occurrence était présente dans les résultats du `grep "23960"` de
r436/r439 depuis le début (26 occurrences au total dans ce fichier) —
elle a été mal classée comme un `lwz` de plus par une relecture trop
rapide de la liste. **Auto-correction** : r436/r439 avaient raison sur
la conclusion (« aucun écrivain trouvé par cette méthode dans le code
atteignable ») pour de mauvaises raisons partielles — l'écrivain
existait bien dans les résultats bruts, simplement mal lu.

## Vérifié EN DIRECT — le code existe mais N'EST PAS exécuté dans ce run

Point d'arrêt gdb posé exactement sur cette instruction (adresse hôte
confirmée en direct : `0x555555724df8`, dans le corps compilé de
`__imp__sub_821D5F48`, avant le début de `__imp__sub_821D6C1C` à
`0x555555724ef0` — bien à l'intérieur des limites réelles de la
fonction). Lancement complet (`--probe-entry`,
`AC6_NATIVE_ALLOW_ENTRY_PROBE=1`) :

- Le point d'arrêt sur l'ENTRÉE de `sub_821D5F48` se déclenche
  **une seule fois** (fonction appelée une seule fois avant le crash,
  cohérent avec le flux déjà établi par r440).
- Le point d'arrêt temporaire sur l'instruction d'écriture
  (`0x555555724df8`) **ne se déclenche JAMAIS** — le processus
  progresse directement de `sub_821D5F48` jusqu'au crash déjà connu
  dans `sub_821D6C20` (`0x555555724f58`) sans jamais atteindre ce
  point.

**Ceci réconcilie pleinement avec r437** : le code d'écriture existe
réellement dans `sub_821D5F48` (retail réel, pas une invention), mais
`sub_821D5F48`, dans ce run, retourne via un **chemin de retour
antérieur** (parmi les nombreux `return 0`/`return 1` imbriqués vus
dans la décompilation) qui ne l'atteint jamais — tout en retournant
une valeur suffisante pour que `sub_821D7DE0` ne déclenche pas son
échec « doit réussir ». Le point d'arrêt matériel de r437 (couvrant
tout le run, du tout premier octet invité jusqu'au crash) avait
raison : zéro écriture, parce que le chemin qui écrit n'est
simplement jamais emprunté ici.

## Ce que ceci établit

**La contradiction apparente entre décompilation et preuve en direct
est résolue, pas par le rejet d'une des deux, mais par une troisième
explication correcte** : le code source est fidèlement lu, la preuve
en direct de r437 est fidèlement mesurée — les deux sont vrais
simultanément parce que le code écrivain est réel mais conditionnel,
et la condition qui y mène n'est pas satisfaite dans cet
environnement. C'est exactement le type de situation où sauter à une
conclusion à partir d'une seule source de preuve (texte OU direct
seul) aurait été trompeur.

**Reste à établir** : QUEL retour antérieur est pris, et quelle
condition en amont détermine ce choix — c'est là, pas dans le bloc
d'écriture lui-même, que se trouve la vraie divergence entre cet
environnement offline et le matériel réel.

## Non établi

- **Le retour antérieur exact emprunté** dans `sub_821D5F48` — la
  fonction contient de nombreux `if`/`return` imbriqués (vus dans la
  décompilation r440/r441) ; lequel est pris n'a pas été tracé en
  direct ce cycle.
- **La condition qui détermine ce choix** — probablement un test sur
  un service/état que cet environnement offline ne satisfait pas de
  la même façon qu'une vraie console (à déterminer).

## Décisions prises

- Auto-corriger r436/r439 dans ce rapport plutôt que de laisser
  l'erreur silencieuse — la conclusion finale de ces cycles restait
  correcte par accident, pas par une lecture complète et exacte des
  résultats de recherche.
- Vérifier EN DIRECT avant de conclure quoi que ce soit à partir de
  la décompilation seule — exactement la discipline que ce rapport
  avait initialement prévu de nommer pour un cycle séparé (r442),
  faite dans ce même cycle une fois l'ambiguïté identifiée comme
  résoluble à faible coût.
- Committer l'outil (`scripts/DecompileD5F48SettingsHelpers.java`) —
  utile indépendamment, a permis de trouver l'écrivain réel.

## Gate

**Source committée ce cycle** :
`scripts/DecompileD5F48SettingsHelpers.java` (nouvel outil, aucune
source de production touchée).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé depuis r440 (aucune source de
production modifiée).

## Named for r442

Tracer EN DIRECT le chemin de retour réellement emprunté par
`sub_821D5F48` (points d'arrêt sur les différents `return`/`b
0x82382a48` imbriqués vus dans la décompilation, ou une capture pas à
pas depuis l'entrée) pour identifier la condition précise qui évite
le bloc d'écriture — c'est la vraie divergence avec le matériel réel,
pas le bloc d'écriture lui-même. Reste ouvert sinon : la décision de
committage de l'arriéré `native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Nouveau : `scripts/DecompileD5F48SettingsHelpers.java` (committé).
Aucun artefact gitignoré nouveau sous `reports/`.
