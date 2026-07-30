# Cycle 328 — P0.2 bis : le thread principal était bloqué dans une réception réseau

## Cible

- Produit : AC6 Xbox 360 PAL, Xenon PPC big-endian, Xenos
- Module : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Binaire mesuré : `5fbe1dfbf56ee0ef354588957cf87ca4f7467af20e0d57c7b9b37c8e66bc77b4`
- Route : `dynamic`

## 0. Objet

P0.2 bis du `PLAYABLE_PLAN.md` : **où est le thread principal ?** Le cycle 327
avait clos P0.2 sur un résultat négatif — le thread principal n'apparaît dans
aucun des trois exports d'attente `Nt*` recensés — et proposait trois candidats,
tous internes au noyau invité : `KeWaitForSingleObject`,
`KeDelayExecutionThread`, `RtlEnterCriticalSection`.

**Les trois étaient faux.** Le thread principal n'attendait aucun objet invité.

## 1. Instrument : demander au noyau hôte, pas à l'invité

Le cycle 327 cherchait à l'intérieur du runtime. Or le noyau hôte sait déjà, pour
chaque thread, son état, sa fonction d'attente et son temps CPU. C'est gratuit,
immédiat, et sans reconstruction :

- `/proc/<pid>/task/<tid>/comm` — le nom, donc l'identité `XThread`
- `stat` champ 3 — `R` actif, `S` endormi
- `wchan` — la fonction noyau où le thread est bloqué
- `utime+stime`, échantillonné deux fois — le débit CPU réel

`tools/ac6-thread-state-census.sh`. Résultat au premier essai, sonde de 40 s :

```text
tid       st  cpu%    wchan                        comm
4002230   S   102.2%  0                            Audio Worker
4002222   S    36.7%  poll_schedule_timeout        ac6recomp
...
4002275   S     0.0%  __skb_wait_for_more_packets  Main XThread   <-- ici
```

`__skb_wait_for_more_packets` est la fonction d'attente d'un **datagramme**.
Le thread principal n'est ni dans un futex, ni dans une attente d'objet invité :
il est dans une **réception réseau bloquante**, à **0,0 % de CPU**.

C'est exactement pourquoi le recensement du cycle 327 ne le voyait pas : il
instrumente les attentes d'objet, et une réception de socket n'en est pas une.

**Correction du cycle 326.** Ce cycle attribuait 1,7 % du CPU à `Main XThread`
et en déduisait qu'il tournait un peu. Mesuré ici sur deux exécutions : **0,0 %**.
Le thread ne consomme rien ; les 1,7 % étaient du bruit d'attribution.

## 2. Identification de l'appel

`/proc/<pid>/task/<tid>/syscall`, cinq échantillons sur 10 s, **identiques** :

```text
syscall: 45 0x62 0x17014f750 0x501 0x0 0x7b2e2e426348 0x7b2e2e426344
         │   │                │
         │   │                └─ len = 0x501 = 1281 octets
         │   └─ fd = 0x62 = 98
         └─ 45 = recvfrom
```

Blocage dur, pas un sondage. Et le SDK ne contient **qu'un seul** `recvfrom` :
`XSocket::RecvFrom`, atteignable uniquement depuis l'export invité
`NetDll_recvfrom`. L'invité se met donc lui-même dans cet état, par la couche
réseau.

## 3. Ce qui a échoué, et ce qui a marché

**`strace` est inutilisable ici.** Lancé sur le processus complet, son coût est
tel que l'invité n'atteint jamais l'appel en 120 s : 905 298 lignes, 252 Mo,
dominées par 871 349 `recvmsg` d'infrastructure hôte, **aucune socket `AF_INET`**.
À ne pas réessayer.

**Les points de trace `perf`** (`syscalls:sys_enter_recvfrom`) ne sont pas
exposés à `perf_event_paranoid=1`.

**Ce qui a marché : un interposeur `LD_PRELOAD`** — `tools/ac6-net-interpose.c`,
16 Ko, une seconde de compilation, aucune reconstruction du binaire de 165 Mo.
Il journalise les appels socket et peut **tester un correctif avant de l'écrire**.

Sûreté : le processus détient de nombreuses sockets `AF_UNIX`/`AF_NETLINK` —
X11, dbus, PipeWire, NSS — et les casser casse tout. Chaque interception demande
donc d'abord le domaine au noyau (`SO_DOMAIN`) et ignore tout ce qui n'est pas
`AF_INET`. Les seules sockets `AF_INET` de ce processus sont celles de l'invité.

## 4. Mesure : la séquence réelle

```text
socket(domain=2 type=2 proto=17) = 99      AF_INET, SOCK_DGRAM, UDP
bind(fd=99 port=999) = -1                  ÉCHEC
recvfrom(fd=99 len=1281 flags=0)           bloque indéfiniment
```

Vérifié directement, en dehors du runtime :

```text
$ bind(('0.0.0.0', 999))  ->  EACCES
$ cat /proc/sys/net/ipv4/ip_unprivileged_port_start  ->  1024
```

**Le `bind` échoue parce que Linux réserve les ports sous 1024.** La Xbox 360
n'a aucune règle de ce genre : un `bind` qui réussit *toujours* sur console
échoue *toujours* ici. L'invité reçoit alors sur une socket non liée, où aucun
datagramme ne peut arriver — d'où l'attente infinie.

## 5. Une cascade, pas un défaut

L'expérience « faire réussir le `bind` » a révélé que le défaut en cachait deux
autres, que l'invité n'atteignait jamais :

```text
bind(fd=98 guest_port=999) remapped to host_port=40999 = 0
ioctl(fd=98, WINSOCK FIONBIO, arg=16777216)      <-- jamais atteint avant
recvfrom(fd=98) = -1 EAGAIN                      <-- ne bloque plus
```

Trois divergences hôte distinctes, empilées :

| # | divergence | conséquence |
|---|---|---|
| 1 | port privilégié 999 refusé par Linux (`EACCES`) | socket non liée, réception infinie |
| 2 | `FIONBIO` Winsock `0x8004667E` transmis tel quel à `ioctl()` (Linux : `0x5421`) | la demande de non-bloquant est sans effet |
| 3 | l'argument est de l'invité **big-endian** : son `1` arrive en `0x01000000` | toute comparaison à `1` prend la mauvaise branche |

Le point 2 avait été formulé **par lecture du source**, puis **réfuté par la
première mesure** — la trace ne montrait aucun `ioctlsocket`. Il s'est avéré vrai
seulement après correction du point 1, qui seul rendait l'appel atteignable. La
lecture du code avait la bonne hypothèse pour la mauvaise raison ; c'est la
mesure qui a donné l'ordre causal.

## 6. Effet mesuré

Deux exécutions de 60 s, **même binaire**, correctifs actifs via l'interposeur :

| observable | référence | correctif, ex. 1 | correctif, ex. 2 |
|---|---:|---:|---:|
| thread principal | 0,0 % CPU, bloqué | actif | **121,1 % CPU, actif** |
| `eop` | 12 | 25 | **34** |
| `guest_swap_requests` | 4 | 9 | **12** |
| `host_swap_presents` | 3 | 8 | **12** |
| `wptr_updates` | 20 | 38 | **50** |
| anneau `wptr` | `0x43` | `0x79` | **`0x9D`** |
| `primary_executions` | 11 | 14 | **17** |

Reproduit sur deux exécutions indépendantes. L'invité va deux à quatre fois plus
loin, et le thread principal passe de « garé pour toujours » à « exécute du code
invité ».

## 7. Correctif porté dans le SDK

L'interposeur est un instrument, pas un livrable. Le correctif est écrit dans
`XSocket`, donc dans l'arbre, et vaut pour tout titre :

- **`XSocket::Bind`** — essaie **d'abord** le port de l'invité, pour qu'un hôte
  permissif se comporte exactement comme la console ; **seulement** sur `EACCES`
  et pour un port privilégié, décale de `+40000`. `bound_port_` conserve le port
  **invité** : la correspondance est un détail hôte, rien de ce que le titre
  observe ne doit changer.
- **`XSocket::SendTo`** — applique le même décalage, et **uniquement** si un
  `bind` privilégié a réellement été refusé, sans quoi un titre qui se parle à
  lui-même en boucle locale émettrait vers le port non décalé.
- **`XSocket::IOControl`** — traduit `FIONBIO` Winsock en `fcntl(O_NONBLOCK)` et
  `FIONREAD`, en **échangeant les octets** de l'argument dans les deux sens.

Aucun privilège requis : le livrable est un exécutable qu'un utilisateur
ordinaire lance, donc `CAP_NET_BIND_SERVICE` et
`sysctl ip_unprivileged_port_start=0` sont écartés comme correctifs.

Patch : `patches/rexglue-guest-socket-privileged-port-and-fionbio-20260730.patch`.

## 8. Validation exécutée

```text
recensement d'état des threads, 2 exécutions   thread principal localisé, reproduit
échantillons syscall                            5/5 identiques, blocage dur
bind(999) hors runtime                          EACCES, cause confirmée
strace, 120 s                                   inutilisable, documenté
interposeur, mode observation                   séquence mesurée, sans effet de bord
interposeur, mode correctif                     2 exécutions, gains reproduits
compilation de xsocket.cpp modifié              rc=0, 0 erreur, 0 avertissement neuf
git apply --check sur l'arbre de référence      OK
```

Ni preuve de jouabilité, ni preuve de parité retail.
`recompiler-generated` n'est pas `verified`.

## 9. Porte P0

**Non franchie.** Exigé : `wptr` avançant, `eop` monotone au-delà de 100,
`host_swap_presents` au-delà de 600 sur 60 s. Mesuré, meilleur binaire :
`eop` 34, `host_swap_presents` 12, `wptr` figé à `0x9D`.

Le blocage réseau est levé et l'invité avance nettement, mais il se **fige de
nouveau**, plus loin. Ce n'était pas le dernier verrou.

## 10. Front suivant

Le thread principal est passé de « bloqué à 0 % » à « **actif à 121 %** ». Il ne
dort plus : il **tourne** sans soumettre de trame. La question change de nature —
ce n'est plus « sur quoi attend-il ? » mais « quelle boucle exécute-t-il ? ».

C'est le premier cycle où l'invité présente une boucle chaude réelle, et donc le
premier où un profil `perf` par thread avec unwinding DWARF, restreint au thread
principal, mesure quelque chose : il y a désormais des échantillons à prendre.
Le cycle 326 avait montré que cette mesure était vide tant que le thread dormait.

Question adjacente, toujours ouverte depuis le cycle 327 : **qui devait signaler
les sept objets `waits=1`** qui immobilisent le pool de travail.

## 11. Règles ajoutées

1. **Interroger le noyau hôte avant d'instrumenter l'invité.** Trois candidats du
   cycle 327 auraient coûté une reconstruction SDK chacun ; `/proc/<pid>/task`
   a répondu en une sonde, gratuitement, et a montré qu'ils étaient tous faux.
   Un thread invité bloqué dans un appel système hôte est invisible à tout
   recensement interne au noyau invité.
2. **Un défaut peut en cacher un autre en amont du même chemin.** Corriger le
   `bind` a fait apparaître deux divergences supplémentaires que l'invité
   n'atteignait jamais. Une trace « l'appel X n'a jamais lieu » ne prouve pas que
   X est correct — seulement qu'il est inatteignable dans l'état courant.
3. **Un interposeur `LD_PRELOAD` teste un correctif avant de l'écrire.** Sur un
   binaire LTO de 165 Mo où `strace` est inutilisable, `gdb` coûte des minutes et
   une reconstruction coûte une phase, c'est l'instrument le moins cher — à
   condition de le restreindre par un critère que le noyau confirme
   (`SO_DOMAIN`), jamais par un pari sur les descripteurs.
