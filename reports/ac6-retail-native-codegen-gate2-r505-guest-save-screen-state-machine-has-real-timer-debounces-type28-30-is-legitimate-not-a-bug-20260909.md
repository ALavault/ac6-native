# AC6 retail NTSC-U/J — r505 — lecture Ghidra directe du côté invité pendant la fenêtre `sleep(4)` : `type28=30` provient d'une machine à états de dialogue de sauvegarde avec de vrais délais temporisés (0,2s/0,5s), imbriqués — comportement légitime, pas un bug

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle lancé ce cycle (lecture Ghidra directe uniquement,
conforme à la décision utilisateur reçue avant ce cycle).

## Contexte

Nommé par r504 : huit cycles de capture (r492-r504, ~19 tentatives)
bloquent de façon répétée au même point précis —
`tools/ac6-oracle-run.py::wait_log()`'s `sleep(4)` (délai de réglage
après une pulsation de touche), avant que la route n'atteigne son
objectif `wait-pulse type28=30`. r499 avait fermé l'angle du script
wrapper lui-même (juste l'endroit où l'horloge globale du `timeout
240` externe expire). CPU (r489), GPU (r497), RAM (r498) et VRAM
(r503/r504) ont tous été écartés comme cause de CE blocage précis.
Reste une question jamais posée : que fait le côté INVITÉ (code
PowerPC/Xenon) pendant cette fenêtre réelle de plusieurs secondes ?

## Établi — `type28` est le champ `screen+0x1c` (28 décimal) d'une structure « écran » partagée par trois machines à états imbriquées

`recompilation/ace-combat-6-retail/overlay/ac6_route_sync_ntsc_uj.cpp`
(lignes 36-51) : `ReadSaveSnapshot(base, screen)` lit
`GuestWord(base, screen+28)` comme champ `type`, journalisé
`type28=` dans les logs `[ac6-save-route]`/`[ac6-save-task]`/
`[ac6-save-state]` — c'est exactement la valeur que
`wait-pulse type28=30` de la route `us-pretype28-startup.steps`
attend.

Trois fonctions invitées hookées par ce même fichier
(`rex_sub_821C3800`/`821C5268`/`821C5708`, adresses
`0x821C3800`/`0x821C5268`/`0x821C5708`) ont été décompilées
directement via Ghidra (`ghidra-projects/ac6-us`, lecture seule,
`-noanalysis`). Chacune obtient le MÊME pointeur « screen » via un
accesseur propre (`Function_823829E0()` pour les deux premières,
`func_0x823829f4()` pour la troisième — cohérent avec un singleton
partagé), et pilote sa PROPRE machine à états sur un champ distinct
de la même structure :
- `Function_821C3800` : état sur `screen+0x28` (valeurs 0-11
  observées, jamais 30 directement).
- `Function_821C5268` : état sur `screen+0x44` (correspond à
  `selector44` du log — confirme que `screen` est bien la structure
  journalisée).
- `Function_821C5708` : état sur `screen+0x48`, appelle
  `Function_821C5268` en interne (case 1).

## Établi — `type28=30` (0x1e) est écrit par `Function_821C5268`, case 3, sous condition d'un drapeau et après un sondage temporisé

Ligne exacte (dans le corps décompilé de `Function_821C5268`, case
3) :
```c
*(undefined4 *)(iVar2 + 0x24) = 3;
if (*(int *)(iVar2 + 0x184) == 1) {
    *(undefined4 *)(iVar2 + 0x18) = 1;
    *(undefined4 *)(iVar2 + 0x20) = 1;
    *(undefined4 *)(iVar2 + 0x1c) = 0x1e;   // screen+28 = type = 30
    *(undefined4 *)(iVar2 + 0xc) = 0;
}
```
`0x1c` = 28 décimal = exactement le champ `type` de
`ReadSaveSnapshot`. `0x1e` = 30 décimal = exactement la valeur que
`wait-pulse type28=30` attend. Cette branche n'est atteinte
QU'APRÈS que `Function_821C59C0(iVar2)` (le sondage du `case 3`,
appelé en boucle, non décompilé ce cycle — nom générique, pas
d'adresse encore qualifiée) retourne succès, ET seulement si le
drapeau `screen+0x184 == 1` a déjà été positionné par ailleurs
(établi dans `Function_821C3800`, case 3 : `*(undefined4
*)(iVar3+0x184) = 1;`).

## Établi — trois délais temporisés réels et distincts, chacun 0,2s ou 0,5s, chaînés

Deux constantes globales lues directement en mémoire (valeurs
`float`, PAS `double` malgré la promotion implicite du décompilateur
— vérifié par lecture des 4 octets exacts, `0x3f000000` et
`0x3e4ccccd`, motifs IEEE-754 non ambigus) :
- `_UNK_8205474c` = **0,5 s** (`dVar8` dans `Function_821C5268`,
  aussi utilisée directement dans `Function_821C5708` case 4/5).
- `_UNK_82069c20` = **0,2 s** (`dVar7` dans `Function_821C5268`).

Ces constantes gardent plusieurs transitions d'état via une
comparaison horloge réelle (`(**(code**)(iRam823f6db8+4))(...)`, un
callback d'horloge invité) moins un horodatage de départ — un
« debounce » temporisé classique, PAS un blocage indéfini : chaque
état finit par retourner `1` une fois le seuil dépassé, la fonction
étant rappelée à chaque tick tant que la condition n'est pas
satisfaite (motif cohératif non bloquant, même style que le
mécanisme « movie worker » que r487/r488 avaient caractérisé).
`Function_821C5708` (case 4 et 5) réutilise le MÊME seuil de 0,5s
que `Function_821C5268`, et `Function_821C5708` appelle
`Function_821C5268` en interne (case 1) — les délais s'accumulent
séquentiellement, pas en parallèle.

## Non établi

- **La somme exacte de tous les délais sur le chemin complet vers
  `type28=30`** — non totalisée : nécessiterait de décompiler aussi
  `Function_821CFE50`, `Function_821CE6C0`, `Function_821C59C0`,
  `Function_821C5D18`, `Function_821C7910`, `Function_821C4FB0`,
  `Function_821C3C08` (appelées depuis les trois fonctions lues, non
  décompilées ce cycle, hors périmètre d'une seule session
  d'analyse statique). Plausible que 2-4+ secondes réelles
  s'accumulent à travers plusieurs de ces sondages temporisés
  imbriqués, cohérent avec le motif observé (`sleep(4)` de
  `wait_log()` insuffisant certains cycles), mais **non quantifié
  précisément**.
- **Si un ralentissement du cadencement invité sous contention hôte
  (CPU/GPU déjà mesurés par r489/r497, jamais parfaitement corrélés
  au blocage `sleep(4)` précis) peut retarder l'ATTEINTE de ces
  seuils temporisés** malgré leur base sur une horloge réelle plutôt
  que sur un compte de tick — l'horloge elle-même ne devrait pas
  ralentir, mais la FRÉQUENCE à laquelle la fonction est rappelée
  (et donc revérifie la condition) pourrait, sous contention sévère,
  retarder la DÉTECTION du seuil dépassé de façon significative si le
  moteur de jeu tourne à une fréquence de tick très réduite. Non
  mesuré directement.

## Décisions prises

- Ne PAS lancer de capture oracle ce cycle — conforme à la décision
  utilisateur.
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/` ni
  aucun sous-module — lecture statique pure, aucune correction
  identifiée avec suffisamment de confiance (le comportement lu est
  LÉGITIME, pas un bug à corriger).
- Documenter le mécanisme exact plutôt que de s'arrêter à « c'est
  juste l'endroit où l'horloge a expiré » (r499) — cette lecture
  montre PRÉCISÉMENT pourquoi cet endroit précis nécessite legitimement
  plusieurs secondes réelles, ce qui recontextualise (sans contredire)
  la conclusion de r499 : le blocage `sleep(4)` documenté par
  r498/r501/r503/r504 est cohérent avec une séquence d'initialisation
  d'écran invité légitimement lente, pas un signe de dysfonctionnement
  du wrapper ni du runtime natif.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché). `git
status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé.

## Named for r506

**Le blocage `sleep(4)` a maintenant une explication mécaniste
plausible et citée (séquence d'initialisation d'écran de sauvegarde
avec délais temporisés chaînés), mais non quantifiée précisément.**
Candidats pour la suite, aucun tenté ce cycle :
1. Décompiler les 7 fonctions callées non encore lues pour totaliser
   le délai minimal exact du chemin complet vers `type28=30` —
   confirmerait ou infirmerait si ce délai dépasse structurellement
   le budget de `wait_log()`/`sleep(4)` actuel.
2. Si le délai total legitimate dépasse le budget de route actuel,
   la piste la plus directe serait d'ALLONGER ce budget (route
   `us-pretype28-startup.steps` ou `wait_log()` lui-même) plutôt que
   de continuer à retenter — pas encore décidé, nécessite d'abord la
   quantification du point 1.
3. Les 3 états cibles de r478 restent non capturés après 8
   cycles/~19 tentatives — aucune reprise décidée ce cycle.

## Files

Committé : ce rapport, `NEXT.md`,
`scripts/Ac6SaveScreenDecompile.java`,
`scripts/Ac6SaveTimingConstants.java`. Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié. Scratch
(`/fastdata/lavaulta/tmp/r505-ghidra/`) non conservé.
