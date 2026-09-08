# AC6 retail NTSC-U/J — r411 — le commit du redimensionnement ne s'exécute JAMAIS : le tableau croissant continue d'écrire indéfiniment dans son propre tampon déjà libéré, sans jamais re-déclencher de croissance

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r410 (nommé pour r411) : lire `sub_823857E0`/`sub_821FA9E0`
pour trancher entre un registre de fin périmé côté `sub_8237FA50`, ou
`sub_823857E0` échouant à agrandir réellement le tampon avant de
libérer l'ancien.

## Établi

### 1. `sub_821FA9E0` est un `realloc()` textuellement correct

Lecture complète du code PPC (`ppc_recomp.27.cpp:19452`) : alloue un
NOUVEAU tampon plus grand via `sub_821F9E10` (AVANT toute libération —
l'ancien tampon est donc toujours vivant à ce moment, pas de
chevauchement possible par construction), copie le contenu utile via
`sub_82383870` (memcpy borné au plus petit des deux tailles), libère
l'ANCIEN tampon via `sub_821FA6F8`, puis retourne le NOUVEAU pointeur
(`ctx.r3`, propagé jusqu'au retour de la fonction). Rien dans cette
fonction n'explique une réutilisation de l'ancienne adresse — la
lecture réfute directement l'hypothèse ouverte par r410 d'un bug dans
`sub_821FA9E0` lui-même.

### 2. La croissance a bien lieu, avec une nouvelle adresse distincte, capturée en direct

`r419_grow_alloc_ret.gdb` — pile d'appel filtrée à
`sub_8237FA50<-...<-sub_821FA9E0`, capture de l'entrée ET du retour de
l'allocation interne :

```
GROWALLOC-ENTER call#1 seq=5 r4(flags)=0x1 r5(size)=0x100
BT: sub_821F9E10 <- sub_821FA9E0 <- sub_823857E0 <- sub_8237FA50 <-
    sub_8237FB58 <- sub_821F7B28 <- __xstart
GROWALLOC-RET call#1 seq=5 ... -> r3=0x100015a0
FREE seq=6 r5=0x10000770
```

`0x100` = `128+128` (doublement de la capacité, `min(2048,r29)+r29`
dans `sub_8237FA50`, texte déjà cité par r410) — la croissance
concerne bien CE tampon. `0x100015a0` est une adresse NEUVE,
totalement distincte de `0x10000770` — l'allocation retourne un
tampon réellement plus grand, pas l'ancien recyclé.

### 3. Le pointeur `begin` du tableau (`0x82a5eef0`) n'est écrit qu'UNE SEULE FOIS dans tout le run — jamais après cette croissance

`r421_begin_full_history.gdb` — point d'observation matériel unique
sur `0x82a5eef0` (le pointeur "début" du tableau), armé dès l'entrée
du constructeur `GuestAddressSpace` (avant TOUTE activité applicative),
plafonné à 10 déclenchements, **run complet jusqu'à `total-seq=883`
(fin du run, la même borne que r406-r409)** :

```
WATCH-STOP #1 seq=0  pc=__xstart              new_be=0xffffffff  (initialisation sentinelle)
WATCH-STOP #2 seq=2  pc=sub_8237F9F0           new_be=0x10000770  (première allocation, seq=2)
run finished, watch-hits=2 total-seq=883
```

**Deux écritures pour tout le run, aucune après `seq=2`.** En
particulier, **aucune écriture ne correspond à la croissance de la
Section 2** (`seq=5`/`6`, bien après `seq=2`) : le nouveau pointeur
`0x100015a0` retourné par `sub_821FA9E0` n'est **jamais** stocké dans
`begin`. Le chemin `loc_8237FAF0: stw r3,-4368(r25)` du code PPC de
`sub_8237FA50` (texte déjà cité par r410, qui devrait s'exécuter
immédiatement après un retour non nul de `sub_823857E0`) ne s'exécute
donc pas pour cet appel, malgré un retour clairement non nul
(`0x100015a0`) et une pile d'appel confirmant que c'est bien CETTE
fonction qui a déclenché la croissance.

### 4. Après cette croissance manquée, le tableau continue d'écrire indéfiniment dans le tampon libéré, sans jamais re-tenter de croître

`r420_globals_watch.gdb` — points d'observation matériel sur `begin`
ET `end` (`0x82a5eeec`), armés juste après le retour du `free()` de
`seq=6` :

- Le point d'observation sur `begin` **ne se déclenche jamais** (0 sur
  17 déclenchements captés) — cohérent avec le § 3.
- Le point d'observation sur `end` se déclenche **17 fois de suite**,
  toutes au MÊME site (`sub_8237FA50`), avec **`seq` figé à `6`**
  pendant les 17 — aucun nouvel appel à `sub_821F9E10`/`sub_821FA6F8`
  ne se produit. `end` progresse par pas de 4 octets, continûment,
  de `0x100007f0` (la borne exacte des 128 octets utiles du tampon
  ORIGINAL, déjà libéré à `seq=6`) jusqu'à `0x10000834` au moins (68
  octets plus loin) :

  | # | `end` (BE) |
  |---|---|
  | 1 | `0x100007f0` |
  | 2 | `0x100007f4` |
  | 3 | `0x100007f8` |
  | 4 | `0x100007fc` |
  | 5 | `0x10000800` |
  | … | (+4 à chaque fois) |
  | 17 | `0x10000834` |

  **La capacité n'est jamais revérifiée avec succès** : la fonction de
  requête de capacité (`sub_82385AF0`, jamais lue) doit continuer de
  rapporter une capacité suffisante sur ce même tampon original —
  déjà libéré — indéfiniment, pour au moins 17 appels supplémentaires
  et vraisemblablement bien plus (le run entier compte `883`
  événements alloc/free, ce tableau n'en consomme plus AUCUN après
  `seq=6`).

## Ce que ceci établit pour r399

**Le tableau croissant global du C++ statique déborde silencieusement
et indéfiniment de son propre tampon, déjà rendu au tas général, sans
jamais re-déclencher de croissance.** Ce n'est pas un simple
use-after-free ponctuel (la lecture de r410 le suggérait) : c'est un
débordement de tas NON BORNÉ, un mot de 4 octets par objet statique
enregistré, marchant en continu à travers toute la mémoire qui suit
l'ancien tampon de 128 octets — exactement la région où r409/r410 ont
localisé la corruption qui alimente toute la chaîne r399-r409
(`0x100007f0`, le voisin physique du tampon, est écrasé dès la
première de ces 17+ écritures). Le nombre exact d'objets statiques
enregistrés après ce point (donc l'étendue exacte du débordement)
n'est pas mesuré, mais l'absence de toute nouvelle activité
alloc/free pour ce tableau jusqu'à la fin du run (`total-seq=883`)
indique qu'il continue d'écrire sans jamais s'arrêter ni re-grandir.

## Non établi

- **Pourquoi le commit (`stw r3,-4368(r25)`) ne s'exécute pas malgré
  un retour non nul confirmé** — deux hypothèses restent ouvertes,
  aucune vérifiée : (a) mauvaise traduction de codegen de la branche
  `cmplwi r3,0`/`bne` à cet endroit précis (improbable au vu du volume
  d'autres branches équivalentes correctement traduites dans ce même
  fichier), ou (b) le retour non nul observé par `r419` correspond en
  fait à un CHEMIN D'ÉCHEC de plus haut niveau qui, ailleurs dans
  `sub_8237FA50` (zone non relue ligne à ligne dans ce cycle après
  `loc_8237FAF0`), aboutit malgré tout à ne pas persister `r3` — par
  exemple si une instruction entre le retour de `sub_823857E0` et le
  `stw` a été mal transcrite dans une lecture précédente de ce rapport
  (r410) et mérite une seconde relecture attentive, instruction par
  instruction, au lieu d'une nouvelle capture.
- **Pourquoi `sub_82385AF0` continue de rapporter une capacité
  suffisante sur un tampon déjà libéré** — fonction jamais lue.
  Nommé pour r412.
- **L'étendue totale du débordement** (combien d'objets statiques sont
  enregistrés après `seq=6`, donc jusqu'où `end` marche réellement) —
  non mesurée au-delà des 17 déclenchements captés par construction du
  plafond de capture.
- Aucun correctif proposé ni appliqué.

## Décisions prises

- Armer le point d'observation sur `begin` dès l'entrée du
  constructeur (tout le run) plutôt que localement, dès qu'une
  première capture localisée (r420) n'a montré aucune écriture — pour
  distinguer "jamais écrit après ce point" de "jamais écrit du tout
  après la création" (l'ambiguïté aurait autrement subsisté).
  Confirmé : les deux cas coïncident, une seule écriture existe dans
  tout le run, à la création (`seq=2`).
- Ne pas deviner le mécanisme exact de la branche manquante sans
  relire `sub_8237FA50` instruction par instruction ou capturer le
  registre directement à cette branche précise — nommé pour r412, pas
  conclu ici (précédent r1111/r1113).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r412

Lire `sub_82385AF0` (requête de capacité) en entier pour comprendre
pourquoi elle continue de rapporter une capacité suffisante sur le
tampon original déjà libéré. En parallèle ou à défaut, isoler par
point d'arrêt x86 la branche exacte `cmplwi r3,0`/`bne` juste après le
premier `bl sub_823857E0` de `sub_8237FA50` (adresse PPC
`0x8237fac8`-`0x8237facc` environ, à confirmer par désassemblage) pour
capturer en direct la valeur de `cr0`/`r3` à cet instant précis lors
de l'épisode `seq=5`/`6`, et déterminer si le commit est sauté par une
branche mal prise ou simplement jamais atteint pour une autre raison
de flux de contrôle.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r418_grow_call.gdb/.log`,
`r419_grow_alloc_ret.gdb/.log`, `r420_globals_watch.gdb/.log`,
`r421_begin_full_history.gdb/.log`.
