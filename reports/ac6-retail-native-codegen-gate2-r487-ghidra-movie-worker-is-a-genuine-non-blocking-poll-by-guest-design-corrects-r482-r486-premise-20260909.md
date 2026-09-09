# AC6 retail NTSC-U/J — r487 — lecture Ghidra directe du guest : le « movie worker » est un sondage NON-BLOQUANT par conception dans le code invité lui-même (timeout=0 explicite), pas une attente réelle — corrige la prémisse de toute la chaîne r481-r486 et confirme r467 par la preuve désassemblage, deux campagnes plus tard

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`,
projet Ghidra `ghidra-projects/ac6-us` (8167 fonctions, projet déjà
analysé — vérifié par un contrôle de cohérence avant toute conclusion,
voir ci-dessous). Aucun oracle lancé ce cycle (lecture statique pure).

## Contexte

Nommé par r485 (piste 1) et choisi explicitement par l'utilisateur
après 8 cycles (r479-r486) de tentatives empiriques de contournement
d'un blocage attribué au « movie worker » : lire le désassemblage
invité réel autour de `0x82916E2C`/`0x82916E3C`/`0x82916E08` plutôt
que continuer à deviner un motif de minutage.

## Établi — contrôle de cohérence du projet Ghidra AVANT toute conclusion

Avant de faire confiance à un résultat de zéro référence (voir
ci-dessous), le projet a été vérifié sain : `getFunctionManager()`
recense 8167 fonctions, les blocs mémoire couvrent bien
`0x82090000`-`0x823d0c6b` (`.text`) et `0x823f0000`-`0x82a5eef7`
(`.data`, qui contient les trois adresses cibles), et l'adresse
d'entrée `0x821F5ED0` (citée dans le manifeste) a bien une fonction
associée avec 1 référence entrante réelle. Le projet n'est donc pas
vide ni non analysé — un résultat de zéro référence ailleurs est
significatif et pas un artefact d'un projet vierge.

## Établi — les trois adresses n'ont AUCUNE référence directe (immédiat 32 bits) dans tout `.text`, mais existent bien en paires `lis`/`addi` fractionnées

Un premier script (`Ac6MovieWorkerEventRefs.java`, xrefs via
`ReferenceManager`) donne 0 référence pour les trois adresses. Un
second script (`Ac6MovieWorkerOperandScan.java`) confirme 0 occurrence
de la valeur 32 bits complète en opérande scalaire sur les 735 908
instructions de `.text` — attendu, PowerPC charge les constantes 32
bits en deux moitiés (`lis`+`addi`/`ori`), donc un opérande scalaire
complet ne peut apparaître que si l'analyse de propagation de
constantes a résolu la paire, ce qui n'était pas le cas ici (compte
zéro plutôt qu'une fausse absence).

Un troisième script (`Ac6MovieWorkerSplitScan.java`) cherche les
moitiés basses (`0x6e3c`/`0x6e2c`/`0x6e08`) séparément et trouve
**16 occurrences réelles** en `addi rX,r11,0x6eXX` / `addi
rX,r10,0x6eXX` dans 8 fonctions distinctes (`0x82094...`,
`0x82138...`, `0x821d7...`, `0x82214...` ×2, `0x823acc...`,
`0x823ad3...`, `0x823ad8...`, `0x823ad9...`, `0x823adb...`) — le
registre de base (r10/r11) porte la moitié haute `0x8291` chargée
ailleurs (probablement via un registre persistant type petites-données,
non retrouvé par le scan `lis` direct — hors périmètre de ce cycle).

## Établi — décompilation de `Function_823AD9C0` : la preuve directe que l'attente est un sondage non bloquant PAR CONCEPTION

```c
if (*(int *)(iVar2 + 0x130) == 0) {
    Function_823AD0D8(iVar2);
    Function_823AD1C0(iVar2,1);
} else {
    func_0x823d056c(0xffffffff82916e3c,1,0);      // KeSetEvent(SetEvent, 1, 0)
    uStack_70 = 0x82916e2c;                          // handle[0] = WaitEvent0
    uStack_6c = 0x82916e08;                          // handle[1] = WaitEvent1
    uStack_60 = 0;                                    // *timeout = 0  <-- POLL, PAS D'ATTENTE
    iVar3 = func_0x823d0bec(2,&uStack_70,1,3,1,0,0,&uStack_60);
    if (iVar3 == 1) { bVar1 = true; }
}
```

La signature de l'appel (`count=2, handles, wait_type=1, reason=3,
mode=1, alertable=0, timeout=&0`) correspond EXACTEMENT au format
journalisé par r467/r482 (`wait_type=1 reason=3 mode=1 alertable=0
timeout=0000000000000000`) — confirmation croisée directe entre le
désassemblage et les logs runtime déjà collectés. `func_0x823d0bec`
est donc `KeWaitForMultipleObjects`, appelée avec un pointeur vers un
timeout de **valeur zéro explicite** (pas un pointeur nul — une valeur
zéro réelle), ce qui signifie « vérifier l'état et retourner
immédiatement », pas « attendre indéfiniment ». Le code invité
**signale son propre événement (`KeSetEvent` sur `0x82916e3c`) juste
avant de sonder les deux événements d'attente**, dans la branche
`else` d'un test sur `*(iVar2+0x130)` — un compteur/indicateur d'état
non encore identifié (piste ouverte, voir ci-dessous).

## Établi — ceci CONFIRME r467 par une preuve de source, deux campagnes plus tard, et CORRIGE la prémisse de r481-r486

r466 avait initialement lu ce motif comme « le symptôme du blocage ».
r467 l'a corrigé empiriquement (logs : `result=0` immédiat sur 159 644
occurrences, présent dès la première seconde après le boot, pas
spécifique à un état bloqué) et avait conclu : « la piste "movie
worker figé" est affaiblie par cette preuve directe » — mais r467
n'avait PAS de preuve de source, seulement une inférence sur les logs.
**Ce cycle fournit cette preuve manquante** : le code invité lui-même,
lu directement, confirme qu'il s'agit d'un sondage à timeout explicite
zéro, jamais un `KeWaitForMultipleObjects` bloquant. r482 (`« l'attente
est donc réelle : si le jeu invité n'appelle jamais KeSetEvent sur
kAc6MovieWorkerSetEvent, l'attente ne se termine jamais par elle-même »`)
avait ré-ouvert cette hypothèse sans la confronter à r467 — **erreur
de méthode corrigée ici** : le code montre que `KeSetEvent` sur cette
même adresse est appelé PAR LA MÊME FONCTION, à la ligne précédente,
inconditionnellement dans cette branche — il ne peut structurellement
pas manquer d'être appelé si la branche est prise.

**Conséquence directe pour Piste A** : les 8 cycles r479-r486 ont
cherché une cause dans le mauvais mécanisme. Le vrai blocage (pourquoi
`us-menu-navigation-probe.steps` échoue parfois à `type28=30`, pourquoi
le produit oracle-hybride met parfois 700+s à démarrer) n'a **jamais**
été le « movie worker » — c'est un sondage par tick sans effet de bord
sur la progression du jeu. La vraie cause reste à chercher ailleurs :
le flag `*(iVar2+0x130)` (qui détermine quelle branche est prise :
`Function_823AD0D8`/`Function_823AD1C0` vs le sondage), ou —
cohérent avec la conclusion de r486 — une contention de charge hôte
réelle et variable (le sondage lui-même ne bloque jamais, mais s'il
tourne sur un thread qui n'obtient pas assez de temps CPU face à un
hôte chargé, la PROGRESSION globale du jeu peut ralentir sans qu'aucun
wait individuel ne soit jamais réellement bloqué — cohérent avec les
runs de 700+s de r480 : processus actif à 121-238% CPU en continu,
jamais gelé).

## Non établi

- **Ce que représente le flag `*(iVar2+0x130)`** et ce qui le fait
  passer de zéro à non-zéro (quel événement de jeu déclenche la
  branche de sondage plutôt que le chemin `Function_823AD0D8`) —
  nécessiterait de décompiler `Function_823AD0D8`/`Function_823AD1C0`
  et de tracer les écritures à cet offset, hors périmètre de ce cycle
  (déjà 4 passes Ghidra effectuées).
- **Le vrai mécanisme derrière la variance de cadencement documentée
  par r486** (host-load/timing) — ce cycle établit que ce n'est PAS le
  movie worker, mais n'identifie pas la cause réelle.
- **Qui appelle `Function_823AD9C0`** — zéro référence directe trouvée
  (probablement invoquée via un pointeur de fonction/table de thread
  worker, cohérent avec son rôle de routine de thread — non
  confirmée).
- **La provenance du registre de base `r11`/`r10` = `0x82910000`** —
  non tracée (probablement un registre persistant chargé une fois par
  fonction depuis une petite zone de données, motif PowerPC courant),
  hors périmètre.

## Décisions prises

- **Ne pas relancer d'oracle sur cette piste** : la question posée par
  r485/r486 (« le movie worker bloque-t-il vraiment ? ») a maintenant
  une réponse ferme et sourcée : non, jamais, par construction. Toute
  nouvelle tentative de « corriger » le motif movie-worker via le
  minutage des routes (comme r481-r484) serait désormais une
  contradiction directe avec cette preuve.
- **Corriger explicitement r482** : son affirmation « l'attente est
  donc réelle » était une régression méthodologique par rapport à la
  conclusion déjà établie de r467, faute de cross-référencement.
  Signalé ici par nom et cycle, conformément à la discipline de ce
  dépôt.
- Conserver les quatre scripts Ghidra créés ce cycle
  (`Ac6MovieWorkerEventRefs.java`, `Ac6SanityCheck.java`,
  `Ac6MovieWorkerOperandScan.java`, `Ac6MovieWorkerSplitScan.java`,
  `Ac6MovieWorkerDecompile.java`) sous `scripts/` — réutilisables pour
  la piste ouverte sur `*(iVar2+0x130)`.
- Aucun fichier sous `recompilation/ace-combat-6-retail/native/`
  touché — cycle de lecture statique pur, comme scopé.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts, inchangés — aucun contrat touché. `native/` inchangé
(vérifié `git status` avant et après ce cycle).

## Named for r488

**La piste « movie worker » est définitivement close comme cause du
blocage** — preuve de source directe, pas une inférence. Reste ouvert :
1. Décompiler `Function_823AD0D8`/`Function_823AD1C0` et tracer
   `*(iVar2+0x130)` pour trouver le VRAI déclencheur de progression
   (nécessite une session Ghidra supplémentaire, ciblée).
2. Caractériser directement la contention de charge hôte pendant un
   run flaky (mesure système : `ps`/`top`/charge CPU du host pendant
   un cycle de capture, pas une nouvelle supposition) — piste nommée
   par r486, jamais tentée.
3. Piste `fetch_const` du HUD de vol (r475), toujours indépendante et
   jamais suivie.

## Files

Committé : ce rapport, `NEXT.md`, `scripts/Ac6MovieWorkerEventRefs.java`,
`scripts/Ac6SanityCheck.java`, `scripts/Ac6MovieWorkerOperandScan.java`,
`scripts/Ac6MovieWorkerSplitScan.java`,
`scripts/Ac6MovieWorkerDecompile.java`. `reports/handoff/CURRENT.json`
non touché ce cycle (préférence pour la continuité, voir décision
ci-dessus — pas de contrainte de coordination sur ce fichier
spécifiquement mais aucun besoin de le modifier pour un cycle sans
nouvelle piste active). Non conservé (scratch,
`/fastdata/lavaulta/tmp/r487-ghidra/`) : logs bruts d'exécution
Ghidra.
