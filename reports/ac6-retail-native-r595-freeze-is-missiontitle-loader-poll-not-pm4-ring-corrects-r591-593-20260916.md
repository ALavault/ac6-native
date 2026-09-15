# r595 — Le gel N'EST PAS le ring PM4 : le mode atteint MissionTitle (0x82065064), puis l'update spinne (livelock) ; corrige r591/r592/r593 (mes propres cycles)

## Qualification

- **Ghidra project** : `ghidra-projects/ace-combat-6`, `default.xex` (retail
  `ntsc-uj`). Oracle : non.
- **Preuve runtime** : run VD_TRACE r594 vivant (pid 3823195, log
  `/fastdata/lavaulta/tmp/ac6-r594-wptr.log`, 1,5 Gio) + inspection directe du
  process gelé via `/proc`. Analyse hors-ligne, aucun nouveau run.

## Correction décisive de r591/r592/r593 (par leurs propres données)

r591-r593 (mes cycles) ont conclu que la troncature d'IB PM4 (1,48 M rejets)
**bloque** le flip fence et donc le game loop. **C'est faux**, réfuté par
l'ordre causal du run r594 :

- Premier rejet de l'IB coinçant (ici `0x12b40f00`, count=11) à la ligne 340695,
  alors que `updates=1304`.
- `updates` **continue de monter** pendant les rejets : 1304 → 1655 → 1688 →
  **1701** — ~400 updates de game loop **pendant** que l'IB rejette 1,7 M de
  fois. Le wedge d'IB est donc un **symptôme de fond** (le VD poller tourne à
  vide sur un ring que le game loop ne surveille pas), **pas la cause**.
- `retries=0` sur 1 734 088 échantillons ⇒ l'IB est **stablement** incomplet
  (pas déchiré) ; aucune attente ne le réparerait (réfute aussi torn-window).
- r533 (nested-prefix commit) **fonctionne** 767 fois sur les IB count=254 : le
  mécanisme d'avance n'est pas cassé. L'IB count=11 est juste trop court pour
  toute barrière de prefix (`kMinPrefixWords=32`).

## Le vrai gel (mesuré) — un LIVELOCK, pas un blocage sur attente

Dernières lignes d'activité réelle avant le silence (puis uniquement du spam
`vd drain rejected`) :

```
r567 mode ordinal=8 frame=1702 vptr=82065064            <- MissionTitle ATTEINT
r581 mission-title-state countdown_before=3 countdown=3 still_title=1
r588 loadpoll upd=1702 ls=18a2a93c state=1 f54=fefefefe dat=0 realkey=0 res=0  <- poll pas prête
r588 treepoll ctx=18a30200 ret=0 flag24=0 list1c=0 f20=0
r588 mgrstate upd=1703 f24=00000000 (était 2)           <- DERNIÈRE ligne, puis silence
```

Le mode **transite bien** Opening (0x820661fc) → **MissionTitle (0x82065064)**
au frame 1702 (progrès réel vs les runs précédents restés à Opening).

### Preuve directe sur le process gelé vivant (pid 3823195, sans nouveau run)

`ptrace_scope=1` interdit gdb, mais `/proc/<pid>/task/*` suffit :

- Le thread du game loop est **tid 3823197** (toutes les lignes `r588
  loadpoll/treepoll` et `r590 lookup` le taguent).
- tid 3823197 : `state=R`, `wchan=0`, `syscall` **vide** (userspace), et
  **`utime` avance ~86 ticks/s**. Il **n'est PAS bloqué** sur un
  futex/mutex/condvar — il **spinne en userspace**. Plusieurs workers
  (3823250/52/57/58/98) spinnent aussi ~100 %/thread.
- Donc `updates` ne gèle pas parce que le thread attend : il gèle parce que
  l'`update()` de MissionTitle **est entré dans une boucle qui ne revient
  jamais** (le compteur de frame ne peut pas s'incrémenter). C'est un
  **livelock**, pas un deadlock — cohérent avec r508. Cela **réfute** à la fois
  « starvation sur le mutex VD » et r590 « parké sur l'event 0x10001a00 » (r590
  = un AUTRE run instrumenté THREAD_SAMPLE ; ici le thread tourne).

### Chaîne d'appel confirmée (addr2line, base ELF 0x589e42af7000)

```
0x589e430ae3e5 -> __imp__sub_822AAB78   (appelle le mode manager)
0x589e42d89fc5 -> __imp__sub_821D7AE0   (game loop)
0x589e42d8a48b -> __imp__sub_821D7DE0   (game loop)
```
soit `Function_821D7D90 (game loop) -> sub_821D7AE0/sub_821D7DE0 ->
sub_822AAB78 -> sub_821B9A00 (mode manager, dispatch MissionTitle)`.

### La cible du spin (par élimination)

Les derniers hooks émis par tid 3823197 sont `loadpoll`/`treepoll` renvoyant 0.
La readiness poll (`sub_821B8478` = `Function_821B8430`) renvoie **res=0** en
permanence : `state=1` (pas « done »), `f54=fefefefe` (motif **non initialisé**
0xFE), `realkey=0`, `dat=0`. La poll ne devenant jamais prête, l'update spinne.
(La prudence « res=0 = juste le frame 1 » est levée : le thread ne rend pas — il
boucle dessus, `utime` à l'appui.)

## Ce qui est établi / non établi

- **Établi** : le boot atteint MissionTitle ; le gel est un **livelock** dans
  l'`update()` de MissionTitle (tid 3823197 spinne en userspace, non bloqué) ;
  la readiness poll renvoie res=0 en boucle avec `f54=fefefefe`/`realkey=0` ;
  chaîne d'appel confirmée par addr2line.
- **Non établi** : (a) l'instruction exacte du spin (IP non épinglé —
  `ptrace_scope=1`, pas de sudo ; localisé par élimination sur `loadpoll`) ;
  (b) la condition précise qui met `loadpoll` res=1 (quel champ/état) ;
  (c) qui devrait initialiser `f54` (+0x54 sur `ls=18a2a93c`) — le 0xFE dit
  « jamais écrit ».

## Prochaine barrière (précise)

Lire `Function_821B8430` (load poll) : quels champs lit-elle et quelle
condition met `res=1` ; d'où vient `f54` (+0x54 sur le loader `ls`) et quelle
écriture guest le renseigne. C'est la couche 2 du plan (MissionTitle →
Briefing), un **livelock de logique guest**, pas le ring PM4. Un fix loader
sans contrôle est refusé (r584-r589 en ont tenté trois, tous retirés) : il faut
d'abord établir la condition de `res=1`, pas la deviner.

## Décisions prises

1. **Corriger r591/r592/r593 (mes cycles)** : le wedge d'IB PM4 est un symptôme,
   pas la cause — prouvé par `updates` 1304→1701 pendant le wedge et
   `retries=0`.
2. **Re-localiser le gel** sur preuve directe (/proc : tid 3823197 spinne en
   userspace, non bloqué ; addr2line sur la chaîne d'appel) : un livelock dans
   l'update MissionTitle, pas le flip fence, pas une attente guest.
3. Aucun code de fix ce cycle : la condition de `res=1` et la source de `f54`
   sont nommées comme prochaine cible, pas devinées (une règle sans contrôle est
   refusée — CLAUDE.md ; r584-589 ont déjà tenté-et-retiré trois fixes loader).
