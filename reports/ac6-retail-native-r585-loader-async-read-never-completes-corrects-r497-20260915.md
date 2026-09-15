# r585 — Le stall MissionTitle n'est pas un compteur kick/wait (r497 corrigé) : une lecture asynchrone de chargement qui ne se termine jamais

## Qualification

- **Ghidra project** : `ghidra-projects/ace-combat-6`, `default.xex` (retail US/JP
  `ntsc-uj`). Décompilation lue dans `exports/*.json`.
- **Oracle** : non. Aucun budget N3. Analyse statique de la décompilation +
  échantillonnage OS du run 19 (r584).
- **Aucun run ce cycle** : le GPU reste saturé par `neural_amp` (r584). Ce cycle
  est une analyse statique qui corrige un modèle erroné ; il ne commite aucun
  patch runtime, faute de pouvoir le valider (une règle sans contrôle est
  refusée).

## Correction d'un prédécesseur (et de r584 lui-même)

**r497 a mal attribué le graphe.** r497 (et, en le citant, r584) décrivait le
stall comme un couple producteur/consommateur : `sub_82345C88` « kickfn »
signalerait un événement sur `0x82870f48`, et `sub_82345CE0` « waittarget » le
consommerait. **Les deux affirmations sont fausses**, vérifiées en lisant la
décompilation :

1. **`0x82345CE0` n'est pas une fonction.** C'est du code *intérieur* à
   `Function_82345C88` (le site de l'appel indirect de lecture, asm `82345ce0`).
   Le « 0 appel » que r497 avait relevé n'est pas un consommateur affamé : c'est
   qu'aucune fonction n'y entre, parce que ce n'est pas un point d'entrée.

2. **`Function_82345C88` ne signale rien sur `0x82870f48` — il le LIT.** Corps
   vérifié (`exports/82345c88.json`) :
   ```c
   iVar2 = *(param_1 + 0x18);                 // obj = 0x82870f48
   if ((iVar2 == 0) || (*(iVar2 + 4) != *(param_1 + 0x14)))  // obj+4 LU (garde de génération)
       lVar4 = -0x1000007;
   else if (*(iVar2 + 0x10) == 0) {           // device idle ?
       lVar4 = (**(code**)(**(int**)(iVar2 + 0x14) + 0x18))  // device->vtbl[+0x18](device, obj, pos, len)
                 (*(int**)(iVar2+0x14), iVar2, *(param_1+0x1c), *(param_1+0x20));
       ...
       if (0 < lVar3) return 1;               // reste > 0 → se ré-arme
   }
   ```
   `obj+4` n'est jamais écrit ici ; il est **comparé** à `param_1+0x14` (contrôle
   de génération/handle). Ce n'est pas un `KeSetEvent`. C'est une **pompe de
   lecture asynchrone par morceaux** : elle avance la position (`param_1+0x1c`),
   décrémente le reste (`param_1+0x20`), et **renvoie 1 pour être ré-invoquée
   tant qu'il reste des octets**. La « valeur toujours croissante » vue par le
   sampler (position ~419 M, ~100 k/s) est le compteur de position/itérations de
   cette pompe, pas un compteur de signal.

`num_callers=0` sur `Function_82345C88` : pas d'appelant statique, cohérent avec
une routine de worker dispatchée par pointeur de fonction (le `tid=2793790`
`state=R` de r584).

## Le vrai graphe de fin de chargement (établi)

Vérifié directement sur `exports/82345c88.json` et `exports/821b99b8.json` ;
le reste de la chaîne provient de l'analyse de `exports/821b8430.json`,
`821d2860.json`, `821d2fc0.json`, `821b8318.json`, `821ba588.json` (lus par
l'agent d'exploration, non re-vérifiés ligne à ligne ici — signalé comme tel).

1. **Démarrage** : `Function_821B8318` met l'état de chargement à 1, crée/retrouve
   un **nœud d'arbre de chargement** sous la racine `0x829e6218`, et lance la
   lecture asynchrone que la pompe `Function_82345C88` pilote sur `0x82870f48`.

2. **Poll de complétion** : `Function_821B8430` (l'entrée réelle de la « fonction
   de chargement » que r584 appelait `sub_821B8478` — `0x821b8478` est en
   milieu de corps) — état 1 → appelle `Function_821D2FC0(0x829e6218, handle)`
   (lookup du nœud) puis `Function_821D2860()` (**le vrai poll**). Celui-ci
   parcourt l'arbre des tâches ; **tout nœud dont la méthode `step` (vtable+0x14)
   renvoie 0 fait renvoyer 0 au poll entier**. Retour 0 = « pas prêt ».

3. **Gestionnaire de mode** `Function_821B99B8` (entrée réelle ;
   `0x821b9a00`/`0x821b8478` sont en milieu de corps). Vérifié
   (`exports/821b99b8.json`) : la machine d'état de chargement vit sur le
   `this` du gestionnaire à **`iVar2+0x2c`** (`=2`, puis `=3`, …), gardée par
   `iVar2+0x18==0` et `iVar2+0x2c==0`, et avançant selon les méthodes de vtable
   du nœud (`*piVar5+0x40`, `*piVar5+0x20`). L'avance de mode ne franchit que
   quand cette machine atteint son état terminal ; sinon le compteur du mode
   MissionTitle (`mode+0x48`, mesuré par r581/r584) reste figé à 3 et
   `Function_821BA588` (le dispatcher d'avance de mode) n'est jamais invoqué.

## Le maillon bloqué (établi)

L'étape 2 ne se produit jamais. **Rien ne délivre une complétion terminale** pour
la lecture asynchrone derrière `0x82870f48`. Sur matériel réel, l'I/O du
fichier/périphérique se termine : le noyau pose le `SignalState` du dispatcher
(`obj+4`) / marque le device inactif (`obj+0x10`), et les méthodes du nœud
rapportent alors « prêt ». Dans le portage natif, la méthode device
`device->vtbl[+0x18]` (la lecture asynchrone) ne conduit jamais la pompe à un
état terminal : `Function_82345C88` renvoie 1 sans fin (~100 k/s, les 419 M
appels de r584), la `step` du nœud (vtable+0x14) reste 0, `Function_821D2860`
reste 0, `Function_821B8430` reste état 1, `iVar2+0x2c` n'atteint jamais son
terminal, le compteur reste 3, et `Function_821BA588` n'est jamais appelé.

## Non établi (dit clairement)

- **Quel binding natif** sert `device->vtbl[+0x18]` et pourquoi il ne se termine
  pas (renvoie toujours un positif sans épuiser `param_1+0x20` ? renvoie 0 sur un
  device qui ne devrait pas être inactif ? handle de génération `obj+4` jamais
  posé ?). C'est le prochain point à instrumenter côté natif — non fait ce cycle.
- **Deux explications concurrentes du « load-poll appelé une fois »** (relevées
  par l'exploration, non tranchées) : soit `iVar2+0x24` est désarmé à 0 (chemin
  d'échec du lookup / `vtable+0x20`), soit le garde global `DAT_8293ba44` reste
  non nul. Les distinguer demande un log runtime de `iVar2+0x24` et
  `DAT_8293ba44` à la queue du gestionnaire de mode par frame.
- **La chaîne 821b8318/821d2860/821ba588** est reprise de l'exploration ; j'ai
  vérifié moi-même `82345c88` et `821b99b8` seulement.

## Décisions prises (en place d'une question)

1. **Corriger r497/r584 dans un rapport** plutôt que perpétuer le modèle
   kick/wait : l'évidence lue contredit l'attribution producteur/consommateur.
2. **Ne pas commiter de patch device natif ce cycle** : sans run de validation
   (GPU saturé), tout patch de complétion serait une règle sans contrôle. La
   cible est nommée précisément pour un cycle ultérieur hors contention.

## Prochaine barrière (concrète, GPU-libre pour l'analyse ; run requis pour valider)

1. **Instrumenter** la queue de `Function_821B99B8` pour logger `iVar2+0x24`,
   `iVar2+0x2c`, `iVar2+0x18` et `DAT_8293ba44` par frame au mode MissionTitle
   (tranche l'ambiguïté ci-dessus). Diagnostic pur, sûr à commiter.
2. **Localiser le binding natif** de `device->vtbl[+0x18]` (la lecture asynchrone
   du nœud de chargement) et déterminer pourquoi la pompe ne se termine pas ;
   faire poser au device une complétion terminale (avancer `obj+0x10`/poser
   `obj+4` selon la sémantique lue), puis valider qu'un run avance
   MissionTitle → Briefing (`0x8206360C`).
