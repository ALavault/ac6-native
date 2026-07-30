# Cycle 327 — recensement des attentes invitées : 17 threads garés, 2 vivants

## Cible

- Produit : AC6 Xbox 360 PAL, Xenon PPC big-endian, Xenos
- Module : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Route : `dynamic`

## 0. Objet

P0.2 du `PLAYABLE_PLAN.md`, avec la prémisse corrigée au cycle 326 :
l'invité **attend** au lieu de boucler, donc la question est *sur quoi, et depuis
quand*.

## 1. Instrument

Recensement des attentes invitées, dans l'arbre, dans
`rex::kernel::xboxkrnl::wait_census` : une entrée vivante par thread invité,
table fixe de 64 entrées sans allocation ni verrou, sondée par la ligne de
télémétrie existante. Un `Enter`/`Exit` encadre les trois exports d'attente
invités — `NtWaitForSingleObjectEx` (`wait1`),
`NtWaitForMultipleObjectsEx` (`waitN`),
`NtSignalAndWaitForSingleObjectEx` (`sigwait`). Les threads hôtes en sont exclus
par construction : `XThread::GetCurrentThread()` rend `nullptr` pour eux.

Chaque ligne déclare le handle, la durée d'attente courante et le **nombre total
d'attentes entrées** par ce thread — c'est ce dernier chiffre qui distingue
« garé depuis le démarrage » de « cycle activement ».

## 2. Mesure, 70 s

```text
tid=0006 wait1   handle=F8000030 held=70.0s waits=1
tid=0007 wait1   handle=F800003C held=70.0s waits=1
tid=0008 wait1   handle=F8000048 held=70.0s waits=1
tid=0009 wait1   handle=F8000054 held=70.0s waits=1
tid=000A wait1   handle=F800005C held=70.0s waits=1
tid=000C wait1   handle=F8000074 held=70.0s waits=1
tid=0013 wait1   handle=F80000B0 held=70.0s waits=1
tid=0016 wait1   handle=F80000D4 held=69.6s waits=9
tid=001C wait1   handle=F8000140 held=68.8s waits=1
tid=0011 wait1   handle=F80000B0 held=68.8s waits=3
tid=0010 sigwait handle=F80000A8 held=68.6s waits=26     <-- le ping-pong
tid=0012 wait1   handle=F80000B0 held=68.6s waits=3
tid=000D wait1   handle=F8000080 held=68.6s waits=2
tid=000F wait1   handle=F800009C held=68.6s waits=5
tid=000B wait1   handle=F8000068 held=68.5s waits=3
tid=000E wait1   handle=F8000088 held= 0.0s waits=8399   <-- vivant
tid=001B wait1   handle=F8000134 held= 0.0s waits=6922   <-- vivant
```

## 3. Lecture

**Deux threads invités sont vivants**, et seulement deux : `000E` sur
`F8000088`, 8 399 attentes en 70 s (~120/s), et `001B` sur `F8000134`,
6 922 attentes (~99/s). Ils entrent et sortent normalement. Ce sont eux, et non
une boucle chaude, qui expliquent les ~9 % de CPU invité du cycle 326.

**Quinze threads sont garés**, dont **sept avec `waits=1`** : ils sont entrés une
seule fois dans leur attente, au démarrage, et n'en sont **jamais revenus**. Six
d'entre eux occupent des handles distincts espacés régulièrement
(`F8000030`, `3C`, `48`, `54`, `5C`, `74`) — la signature d'un pool de threads de
travail dont chacun attend son propre objet, jamais signalé.

**Trois threads partagent le handle `F80000B0`** (`0011`, `0012`, `0013`), avec
1 à 3 attentes chacun : un point de rendez-vous commun, lui aussi sans signal.

**`tid=0010` est le ping-pong des cycles 320 à 323.** C'est le seul `sigwait`, et
son handle `F80000A8` est exactement l'événement identifié au cycle 320. Deux
faits neufs :

1. `waits=26` — il a **cyclé 26 fois** avant de se garer. Le cycle 320 le décrivait
   endormi ; il tourne, puis s'arrête. Cohérent avec l'A/B du cycle 326, où la
   correction du cycle 323 triple la progression de l'invité.
2. **Un seul thread est garé sur `F80000A8`**, alors que le cycle 320 en observait
   **deux**. Avec la valeur partagée mesurée à `0` et le worker attendant `1`, un
   unique dormeur — le worker — est le comportement **correct** du protocole. La
   paire bloquée du cycle 320 n'existe plus.

**Le thread principal n'apparaît pas dans le recensement.** Il n'est donc dans
aucun des trois exports instrumentés. C'est le fait le plus important de ce
cycle : ce qui retient le jeu n'est pas une des attentes recensées ici.

## 4. Front P0.2, resserré

La question devient : **où est le thread principal ?** Trois candidats, tous
non instrumentés :

1. `KeWaitForSingleObject` / `KeWaitForMultipleObjects` — chemin noyau, distinct
   des exports `Nt*` déjà couverts ;
2. `KeDelayExecutionThread` — une temporisation, qui n'apparaîtrait pas comme
   attente d'objet ;
3. `RtlEnterCriticalSection` — un verrou détenu par un des quinze threads garés,
   ce qui refermerait l'explication : un thread de travail jamais signalé qui
   retient une section critique dont le thread principal a besoin.

Le candidat 3 est le plus économique en hypothèses et se teste par le même
instrument : étendre le recensement à ces trois familles. Coût : une
reconstruction SDK.

Question adjacente, à ne pas confondre : **qui devait signaler les sept objets
`waits=1`** ? Ce sont eux qui immobilisent le pool de travail, et l'un d'eux est
probablement en amont du thread principal.

## 5. Modifications

- `thirdparty/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_threading.cpp` :
  `wait_census`, plus `Enter`/`Exit` sur les trois exports d'attente.
- `thirdparty/rexglue-sdk/src/graphics/graphics_system.cpp` : émission du
  recensement dans la ligne de télémétrie, plafond 24 lignes.
- Aucun changement de comportement : deux stockages atomiques relâchés par
  attente, sur des chemins qui font déjà une résolution de handle et une attente
  noyau.
- Aucune sortie générée éditée.

## 6. Validation exécutée

```text
build, 3 itérations (dont une erreur de namespace corrigée)   rc=0
sonde 70 s                                       15 lignes de recensement
threads invités recensés                         17 garés, 2 vivants
tid=0010 handle F80000A8                         = l'événement du cycle 320
compteurs d'anneau                               inchangés, cohérents avec le cycle 325
thread principal                                 absent du recensement
```

Ni preuve de jouabilité, ni preuve de parité retail.

## 7. Porte P0

**Non franchie**, inchangée : `wptr` gelé, `eop` 12, `host_swap_presents` 3.

Mais le champ des causes est passé de « quelque part dans l'invité » à
« le thread principal est bloqué hors des trois exports `Nt*`, et sept objets de
travail n'ont jamais été signalés ». Deux mesures, une reconstruction SDK.
