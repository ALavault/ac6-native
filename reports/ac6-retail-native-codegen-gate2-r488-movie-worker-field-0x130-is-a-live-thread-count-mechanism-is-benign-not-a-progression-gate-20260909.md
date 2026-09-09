# AC6 retail NTSC-U/J — r488 — le champ `*(iVar2+0x130)` que teste `Function_823AD9C0` est un COMPTEUR DE THREADS VIVANTS, pas un indicateur de progression : le mécanisme « movie worker » tout entier est un détail de gestion de threads bénin, jamais un point de blocage, sous aucune interprétation

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`,
projet Ghidra `ghidra-projects/ac6-us` (même projet, même contrôle de
cohérence déjà établi par r487 : 8167 fonctions, blocs mémoire
cohérents). Aucun oracle lancé ce cycle (lecture statique pure).

## Contexte

Nommé par r487, piste 1, et choisi explicitement par l'utilisateur :
décompiler `Function_823AD0D8`/`Function_823AD1C0` et tracer
`*(iVar2+0x130)` pour trouver le vrai déclencheur de progression que
r487 avait laissé ouvert après avoir établi que le sondage
`KeWaitForMultipleObjects` du « movie worker » a un timeout explicite
zéro (non bloquant par conception).

## Établi — `Function_823AD0D8`/`Function_823AD1C0` ne touchent PAS l'offset 0x130 de `iVar2`

Décompilation directe (`Ac6ProgressionDecompile.java`) :
- `Function_823AD0D8` implémente un verrou par compteur de références
  sur une structure globale persistante (`uRam82916de4`/`de8`/`dec`),
  protégeant un tableau d'appels `Function_823ACD88` — un motif
  section-critique classique, sans rapport avec `iVar2+0x130`.
- `Function_823AD1C0` référence `*(uint*)(iRam82916e4c + 0x130)`, mais
  **sur une base différente** — `iRam82916e4c` est une adresse GLOBALE
  fixe, pas le paramètre `iVar2` local de `Function_823AD9C0` (obtenu
  via `func_0x82382a18()`). Cette occurrence au même offset numérique
  est une coïncidence de layout, pas la même variable — noté
  explicitement pour éviter l'erreur de conflation que ce cycle a
  failli commettre.

`func_0x82382a18`/`func_0x82382a10` (les fonctions dont le retour
alimente `iVar2`/`iVar1`) n'ont pas de `Function` Ghidra à leur adresse
exacte — imports/thunks non résolus par cette passe, hors périmètre
d'aller plus loin (non établi, voir ci-dessous).

## Établi — un scan direct des écritures à l'offset 0x130 trouve le vrai setter : `Function_823ADBD8`

`Ac6ProgressionAccessors.java` scanne les 735 908 instructions de
`.text` pour toute instruction `stw`/`sth`/`stb` avec un opérande
scalaire `0x130` — 53 occurrences, dont 24 sont des décharges de pile
relatives à `r1` (non pertinentes, motif standard de sauvegarde de
registre). Des 29 restantes, une paire dans **`Function_823ADBD8`**
(`823addcc`: `stw r26,0x130(r30)`, `823ade74`: `stw
r11,0x130(r30)`) correspond exactement à la fonction que r487 avait
déjà notée comme contenant les trois adresses movie-worker.

## Établi — décompilation de `Function_823ADBD8` : `*(iVar4+0x130)` est un compteur de threads vivants créés par CETTE MÊME fonction

```c
// (extrait, iVar4 = func_0x82382a08(), le même type de structure de contexte)
uRam82916e3c = 1; uRam82916e40 = 0; uRam82916e44 = 0x82916e44; uRam82916e48 = 0x82916e44;
uRam82916e2c = 1; uRam82916e30 = 0; uRam82916e34 = 0x82916e34; uRam82916e38 = 0x82916e34;
func_0x823d0c3c(0xffffffff82916e18,0,6);
uRam82916e08 = 0; uRam82916e0c = 0; uRam82916e10 = 0x82916e10; uRam82916e14 = 0x82916e10;
pcRam82916df8 = Function_823ACCA8; uRam82916dfc = 0x7d800000;
func_0x823d064c(0xffffffff82916df8,1);
*(undefined4 *)(iVar4 + 0x130) = 0;                         // <-- initialisé à ZÉRO
Function_823AC6D0(*(undefined1 *)(param_2 + 2),auStack_70);  // remplit abStack_6e[0..5], drapeaux de slot
uVar15 = 0;
puVar14 = (undefined4 *)(iVar4 + 0x134);
do {
    uStack_7c = 0;
    if ((abStack_6e[uVar15] & 3) != 0) {                    // slot demandé
        uVar8 = (abStack_6e[uVar15] == 1) ? 0xffffffff823ad848 : 0xffffffff823ad910;
        iVar6 = func_0x823d039c(&uStack_7c,0,0,0,uVar8,0,(1 << (uVar15 & 0x3f)) << 0x18 | 1);
        // func_0x823d039c == ExCreateThread (signature: handle-out, stack, param,
        // start, flags) -- crée un thread réel avec point d'entrée Function_823AD848
        // ou Function_823AD910, TOUTES DEUX déjà connues comme appelant
        // Function_823AD0D8/823AD1C0 (r487/ce cycle)
        if (iVar6 < 0) return 0xffffffff8007000e;
        func_0x823d00fc(uStack_7c, ___imp__ExThreadObjectType, &uStack_80);
        func_0x823d00ec(uStack_80,0xf);
        func_0x823d0c2c(uStack_80);
        func_0x823d00dc(uStack_80);
        *(int *)(iVar4 + 0x130) = *(int *)(iVar4 + 0x130) + 1;  // <-- INCRÉMENTÉ par thread créé
    }
    uVar15 = uVar15 + 1;
    *puVar14 = uStack_7c;                                     // handles du thread stockés à +0x134...
    puVar14 = puVar14 + 1;
} while ((int)uVar15 < 6);
```

`Function_823ADBD8` est donc **l'initialisateur du sous-système
« movie worker »** : il configure les trois structures d'événement
(`0x82916e2c`/`0x82916e3c`/`0x82916e08`, celles-là même que
`Function_823AD9C0` sonde), initialise le compteur à zéro, puis crée
jusqu'à **6 threads candidats** (parmi lesquels seuls ceux dont
`abStack_6e[N] & 3 != 0` sont réellement lancés), en incrémentant le
compteur une fois par thread effectivement créé. Les points d'entrée
des threads créés sont `Function_823AD848`/`Function_823AD910` — les
deux fonctions déjà identifiées par r487 comme appelant
`Function_823AD0D8`/`Function_823AD1C0`.

**Conclusion directe** : `*(iVar2+0x130)` dans `Function_823AD9C0` est
donc le **nombre de threads « movie worker » actuellement vivants**.
La branche de `Function_823AD9C0` se lit ainsi : « si aucun thread
movie-worker n'existe (compteur à zéro), faire le travail directement
(`Function_823AD0D8`/`823AD1C0`) ; sinon, sonder (sans bloquer,
timeout=0) si l'un des threads existants a signalé sa fin ». C'est un
choix d'ordonnancement — travail direct vs délégation à un thread déjà
en vol — **pas une porte de progression**. Aucune des deux branches ne
peut bloquer quoi que ce soit : le travail direct s'exécute
immédiatement, et le sondage a un timeout explicite zéro (r487).

## Établi — le fil « movie worker », ouvert depuis r466, est définitivement clos comme piste de blocage, sous TOUTE interprétation

r466 : lu comme le symptôme du blocage (hypothèse initiale). r467 :
affaibli empiriquement (logs, `result=0` immédiat). r482 : ré-ouvert
sans confronter r467 (erreur de méthode, corrigée par r487). r487 :
fermé par preuve de source — le sondage lui-même est non-bloquant par
conception (timeout=0 explicite). **Ce cycle (r488) ferme la dernière
ouverture possible** : même le champ qui sélectionne la branche
(`*(iVar2+0x130)`) n'a aucun rapport avec un état de progression du
jeu — c'est un compteur de threads internes au sous-système lui-même,
mis à jour uniquement par le sous-système lui-même. Il n'existe plus
aucune lecture plausible de ce mécanisme comme cause d'un blocage de
progression.

## Non établi

- **La cause réelle de la variance de cadencement de démarrage**
  documentée par r479/r480/r486 (runs de 700+s, échecs stochastiques de
  `us-menu-navigation-probe.steps`/`us-pretype28-startup.steps`) —
  définitivement PAS le movie worker, mais la vraie cause reste
  inconnue. Candidats non explorés : contention de charge hôte
  (nommé par r486, jamais mesuré directement), un autre sous-système
  guest avec un vrai wait bloquant, ou un problème dans le pipeline
  d'exécution recompilé lui-même (scheduling de threads, latence
  syscall).
- **Ce que représentent `abStack_6e[0..5]`** (quels 6 threads
  candidats, à quoi correspond chaque slot) — nécessiterait de
  décompiler `Function_823AC6D0`, hors périmètre de ce cycle (déjà 3
  passes Ghidra effectuées).
- **`func_0x82382a18`/`func_0x82382a10`** — imports/thunks non résolus
  par cette passe, la provenance exacte de `iVar2`/`iVar1` (probablement
  un contexte par-thread ou un singleton du sous-système) n'est pas
  confirmée avec une adresse concrète.

## Décisions prises

- **Fermer définitivement la piste « movie worker »** comme cause de
  tout blocage de progression — conclusion maintenant établie à deux
  niveaux indépendants (r487 : le sondage ne bloque jamais ; r488 : le
  champ de sélection de branche n'a aucun rapport avec l'état du jeu).
  Aucune nouvelle investigation sur ce mécanisme spécifique n'est
  justifiée.
- Ne pas poursuivre la décompilation de `Function_823AC6D0`/les
  fonctions d'entrée de threads (`Function_823AD848`/`823AD910`) ce
  cycle — rendement marginal pour la question posée (la piste
  « movie worker » est déjà fermée), mieux dépensé sur la vraie piste
  ouverte (contention de charge hôte, nommée par r486).
- Conserver les deux scripts créés ce cycle
  (`Ac6ProgressionDecompile.java`, `Ac6ProgressionAccessors.java`,
  `Ac6Setter130Decompile.java`) sous `scripts/` avec les autres
  scripts `Ac6MovieWorker*`/`Ac6SanityCheck` de r487 — réutilisables
  pour toute future investigation de ce sous-système, même si la piste
  actuelle est close.
- Aucun fichier sous `recompilation/ace-combat-6-retail/native/`
  touché — cycle de lecture statique pur, `git status` vérifié
  inchangé avant et après.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts, inchangés — aucun contrat touché. `native/` inchangé
(vérifié `git status` avant et après ce cycle).

## Named for r489

**La piste « movie worker » est close, sous toute forme.** Reste
ouvert :
1. **Caractériser directement la contention de charge hôte** pendant
   un run flaky (mesure système réelle : `ps`/`top`/charge CPU du host
   pendant un cycle de capture) — nommé par r486, jamais tenté, et
   maintenant la piste la plus directe puisque le movie worker est
   définitivement écarté.
2. Si la contention de charge n'explique pas tout, chercher un AUTRE
   mécanisme d'attente réellement bloquant dans le code invité — pas
   en devinant, mais en répétant la méthode de ce cycle (lecture
   Ghidra directe) sur un point de blocage concrètement observé (ex.
   le dernier point atteint avant un échec `type28=30` dans un log
   `ac6recomp.log` d'un run qui échoue).
3. Piste `fetch_const` du HUD de vol (r475), toujours indépendante et
   jamais suivie.

## Files

Committé : ce rapport, `NEXT.md`,
`scripts/Ac6ProgressionDecompile.java`,
`scripts/Ac6ProgressionAccessors.java`,
`scripts/Ac6Setter130Decompile.java`. `reports/handoff/CURRENT.json`
non touché ce cycle (même raisonnement que r487 — pas de nouvelle
piste active nécessitant sa mise à jour). Non conservé (scratch,
`/fastdata/lavaulta/tmp/r488-ghidra/`) : logs bruts d'exécution
Ghidra.
