# r588 — MissionTitle : le mode-manager frontend arme son load-poll une fois, le désarme, puis la boucle d'update s'arrête (preuve runtime, hooks 821B8430/821D2860)

## Qualification

- **Ghidra project** : `ghidra-projects/ace-combat-6`, `default.xex` (retail
  `ntsc-uj`). Décompilation : `exports/821b8430.json`, `821b99b8.json`,
  `821b9408.json`, `821d2860.json`.
- **Oracle** : non. Aucun budget N3.
- **Preuve runtime** : deux runs bootés jusqu'à MissionTitle
  (`AC6_NATIVE_BOOT_PHASE_TRACE=1` + navigation auto), hooks r588 ajoutés sur
  `sub_821B9A00` (mode-manager), `sub_821B8478` (load-poll = Ghidra
  `Function_821B8430`) et `sub_821D28C8` (Ghidra `Function_821D2860`, le walk
  d'arbre récursif). GPU saturé par `neural_amp` → ~1 h/run ; c'est le coût des
  runs, pas un blocage du diagnostic.

## Correction d'attribution (recompilateur vs Ghidra)

- Le load-poll appelle, dans le recompilé, `bl 0x821d3028` (lookup nœud, Ghidra
  `Function_821D2FC0`) puis **`bl 0x821d28c8`** (le walk, Ghidra
  `Function_821D2860`). La frontière recomp est `0x821D28C8`, pas `0x821D2860`
  (qui n'existe pas comme fonction) ni `0x821D2858` (essai initial erroné).
- `asset_request_word` ne borne pas (`memcpy(base+addr,4)`) : les lectures
  runtime ci-dessous sont réelles, pas des artefacts de garde.

## Établi (preuve runtime, à MissionTitle vptr=0x82065064)

1. **Le load-poll tourne exactement une fois.** `r588 loadpoll ls=18a2a93c
   state=1 res=0 ret=0` (une seule ligne ; `load_calls=1`). L'objet est le
   frontend startup-manager `0x188d0000` (`ls = manager + 0x15a93c`).

2. **Le manager arme puis désarme le poll (`f24` 2→0).** `r588 mgrstate` :
   - `upd=1688 f18=0 f1c=3 f24=2 f2c=0 ba44=0`  → poll **armé** (`f24==2`)
   - `upd=1689 f18=0 f1c=3 f24=0 f2c=0 ba44=0`  → poll **désarmé** (`f24==0`)

   Par `Function_821B99B8` : le poll n'est appelé que si `mgr+0x24==2` ; sinon
   il est sauté. Le désarme suit `Function_821D2FC0(0x829e6218, *(mgr+0x54)) ==
   null || node.vtable+0x20() == '\0'`. `DAT_8293ba44` reste 0 (ce n'est **pas**
   la branche « garde global bloqué » ; c'est bien la branche « désarme »).
   → **Voilà pourquoi `load_calls=1`** : armé une fois, sauté ensuite.

3. **`DAT_8293ba10 = 0` à MissionTitle, et c'est normal.** `*(0x8293BA10)=0`
   (lecture réelle). `Function_821B9408` — qui écrit `DAT_8293ba10 = puVar3`,
   l'objet dont `+0x15a93c` est le load-struct, donc le *game-manager* de
   mission — **n'a pas encore tourné** : il s'exécute à l'entrée en mission,
   *après* Briefing. MissionTitle est un mode **frontend** piloté par le
   startup-manager. Donc `DAT_8293ba10=0` n'est **pas** le bug (rectifie une
   piste que j'allais prendre).

4. **Aucune I/O fichier.** `[NtReadFile] invalid` = 0 sur les deux runs.
   Cohérent avec r587 : ce chemin n'atteint pas la lecture disque.

5. **Deuxième couche : la boucle d'update s'arrête.** `updates` gèle à ~1696 :
   `sub_821B9A00` cesse d'être compté après ~upd 1689. Le sampler r584 montrait
   le thread d'entrée vivant en `hrtimer_nanosleep`. Le gel d'écran est **cette
   couche (ii)**, distincte du désarme `f24` (couche (i)) : `f24=0` ne ferait
   que sauter le poll, la boucle continuerait. Les deux ne doivent pas être
   confondues.

## Ce que cela relocalise (le « consommateur manquant »)

Le vrai point de blocage n'est ni la pompe `0x82870f48` (red herring, r587), ni
le handle PAC (r586, faux), ni une lecture asynchrone (r585, faux). C'est :
**le lookup `Function_821D2FC0(0x829e6218, *(mgr+0x54))` échoue au 2ᵉ update**
(nœud absent ou `vtable+0x20()==0`), ce qui désarme le poll ; **et** la boucle
de mode s'arrête peu après. Le « consommateur manquant » est **l'enregistrement,
dans l'arbre de chargement racine `0x829e6218`, du nœud que le frontend attend
sous la clé `*(mgr+0x54)`** — enregistrement que la HLE ne crée pas.

## Non établi (dit clairement)

- **La valeur de la clé `*(mgr+0x54)`** et le résultat du lookup au moment du
  désarme : non capturés (mes hooks ont loggé `ls+0x54=0xfefefefe`, un *autre*
  `+0x54`, celui du load-struct, pas `mgr+0x54`).
- **Pourquoi la boucle d'update (couche ii) s'arrête** : non déterminé. Le
  thread d'entrée est vivant (`hrtimer_nanosleep`) mais ne redispatche plus la
  mise à jour de mode.
- Lecture live du process gelé **impossible** : `ptrace_scope=1` + process
  orphelin (setsid) → `/proc/pid/mem` et gdb refusés. D'où la nécessité d'un run
  instrumenté supplémentaire pour ces deux points.

## Prochaine instrumentation (précise)

Dans le hook `sub_821B9A00`, à MissionTitle : logguer `*(mgr+0x54)` (la clé de
lookup du désarme) et un **compteur d'appels brut** de `sub_821B9A00` (pour
distinguer couche ii : la boucle s'arrête-t-elle, ou le gate `trace` cesse-t-il
de matcher ?). Un run de plus (~1 h sous GPU saturé) tranche les deux points
« non établis ». Instrumentation r588 déjà en place (scaffolding local) : hooks
`mgrstate` / `loadpoll`+realkey / `treepoll` sur `sub_821D28C8`.

## Décisions prises

1. **Relocaliser le blocage** sur le désarme du poll frontend + l'arrêt de la
   boucle d'update, sur preuve runtime, plutôt que sur `DAT_8293ba10` (écarté :
   frontend, normal) ou la pompe/handle/async des cycles précédents (écartés).
2. **Ne pas conclure la cause du désarme ni de la couche (ii)** faute de la clé
   `*(mgr+0x54)` et du compteur brut ; nommés comme prochaine mesure, pas
   devinés (lecture live bloquée par ptrace).
