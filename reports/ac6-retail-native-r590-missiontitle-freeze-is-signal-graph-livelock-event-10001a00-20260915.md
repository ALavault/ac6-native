# r590 — Le gel MissionTitle est un livelock du graphe de signaux : le game loop se bloque, un thread park 5,8 s sur l'event guest 0x10001a00 jamais signalé

## Qualification

- **Ghidra project** : `ghidra-projects/ace-combat-6`, `default.xex` (retail
  `ntsc-uj`). Résolution hôte→guest via `exec_base` + `addr2line`.
- **Oracle** : non. Aucun budget N3.
- **Preuve runtime** : run bootée jusqu'à MissionTitle avec
  `AC6_NATIVE_THREAD_SAMPLE=1` + compteurs bruts `mgr_raw`/`frameupd_raw` +
  hooks r497/r498/r530 existants + hooks r591 (caller-naming). GPU saturé.

## La chaîne du gel (résolue niveau par niveau)

`sub_821B9A00` (mode-manager) est dispatché **indirectement** ; en résolvant
l'adresse de retour hôte via `addr2line`, la chaîne d'appel est :

```
Function_821D7D90   <- MAIN GAME LOOP : do { ... } while(true)
   -> Function_821D7A90 -> sub_821D7AE0   (g_boot_updates)
      -> sub_822AAB78                      (g_frameupd_raw_calls)
         -> sub_821B9A00 (mode-manager)    (g_mgr_raw_calls)
```

Les trois compteurs suivent 1:1 pendant Opening et **gèlent ensemble à ~1689-1703**
à MissionTitle+2. La boucle `while(true)` ne peut pas « sortir » : elle se bloque
**dans un appel de l'itération** (le mode-manager, lui, RETOURNE — `r588 mgrstate`
est loggé post-update). Le gel est donc en aval du dispatch du mode, dans la
boucle de frame.

## Ce sur quoi le thread se bloque (preuve directe)

Le thread d'entrée échantillonné (sp=0x8feffbc0) est dans des primitives de
synchro **hôte** (libc `futex`, `pthread_mutex_lock/unlock`) en boucle — donc
dans une primitive d'attente guest implémentée par condvar/mutex.

Le tripwire `r530 longpark` (park > 2 s) capture le fait décisif :
```
r530 longpark gatewait tid=1857807 obj=0x10001a00 requested=6835 parked_ms=5834
```
→ **tid 1857807 est parké 5,8 s sur l'objet event guest `0x10001a00`** (attente
« gate », timeout infini `sub_821F4128`). C'est un **vrai blocage**, pas un spin.
Personne ne signale `0x10001a00` → l'attente ne se referme jamais.

## Autour : le graphe de signaux churne mais ne se referme pas (livelock, r508)

À l'état gelé, tout SPINNE (compteurs qui grimpent), rien ne progresse :
- `r498 signalwait` tid 1856974 : `NtSignalAndWaitForSingleObjectEx(signal=0x120,
  wait=0x121)`, 524 M appels.
- `r498 setevent` tid 1857805 : `KeSetEvent` sur `0x11a` (15,6 k) / `0x11c`
  (15,6 k), 624 M appels. `0x121` n'a été set **qu'une fois**.
- `r497 waitfn` (timeout infini) : waiters sur `0x119` (6,5 k) / `0x11b` (6,3 k)
  / `0x120`.
- workers : `Function_823D2020` (`0x823d2058`, division flottante) en spin.

Les waiters attendent `0x119`/`0x11b` tandis que le producteur set `0x11a`/`0x11c`
(décalage apparent), et l'event `0x10001a00` du blocage long n'est jamais signalé.
Confirme r497/r508 : **le graphe de signaux ne se referme pas** — c'est un
**livelock**, sur le thread du game loop, à MissionTitle.

## Ce que cela corrige (toute la piste précédente était en aval)

- « register the missing node » (demande) : le nœud `0xb362294c` **existe**
  (r589, lookup renvoie `0x18a30200`). Non pertinent pour le gel.
- Le désarme `f24` 2→0, le poll de chargement, `DAT_8293ba10=0` : tous **en
  aval** du livelock ; les trois fixes tentés (r589) n'ont rien avancé et ont été
  retirés.
- Le gel réel : le game loop se bloque dans une attente guest dont l'event
  (`0x10001a00`) n'est jamais signalé.

## Non établi (dit clairement)

- **Que tid 1857807 SOIT le thread du game loop** : c'est *un* thread bloqué
  (gatewait 5,8 s) ; que le game loop dépende directement de lui, ou attende
  lui-même `0x10001a00`, reste à confirmer (mapping tid→thread non fait).
- **Qui doit signaler `0x10001a00`** : non identifié. C'est la vraie cible du fix
  (la question « qui doit signaler l'event X », ici épinglée sur `0x10001a00` à
  MissionTitle).
- Le « décalage » `0x119`/`0x11b` (waiters) vs `0x11a`/`0x11c` (setter) : observé,
  pas expliqué (paires d'events ? mauvaise allocation de handle HLE ?).

## Prochaine barrière (précise)

Identifier le **producteur** de l'event guest `0x10001a00` (le `KeSetEvent` /
`set_guest_object` qui devrait le signaler à MissionTitle) et pourquoi il ne
s'exécute pas côté HLE. `0x10001a00` est un objet kernel guest dynamique
(plage `0x1000xxxx`) ; le tracer demande d'instrumenter `KeSetEvent`/create pour
corréler l'objet, puis un run (GPU-borné). C'est le même graphe que r497/r508,
désormais épinglé sur un event concret et un thread parké mesuré.

## Décisions prises

1. **Localiser le gel par preuve** (chaîne d'appel résolue + longpark) plutôt que
   par hypothèse ; corriger la piste « nœud manquant » (r589) définitivement.
2. **Ne pas conclure au-delà de la preuve** : l'event `0x10001a00` et le park de
   5,8 s sont mesurés ; son producteur manquant est nommé comme prochaine cible,
   pas deviné.
3. Aucun code de fix ajouté ce cycle ; hooks de diagnostic (caller-naming,
   compteurs bruts) conservés en scaffolding local.
