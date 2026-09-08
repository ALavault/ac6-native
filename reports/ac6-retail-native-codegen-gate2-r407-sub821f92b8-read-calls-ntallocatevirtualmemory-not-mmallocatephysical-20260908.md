# AC6 retail NTSC-U/J — r407 — `sub_821F92B8` lue en entier : deux chemins possibles (réutiliser un segment existant / réserver-puis-committer via `NtAllocateVirtualMemory`) ; un seul segment existe dans l'instantané déjà capturé et il semble épuisé — lequel des deux chemins l'appel `seq=877` a réellement pris n'est PAS établi ce cycle

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

**Ce rapport a été corrigé avant publication.** Une première version
affirmait que l'appel `seq=877` avait pris le chemin noyau
(`NtAllocateVirtualMemory`) et décodait `0x60002000` comme
`RESERVE|COMMIT` combinés. Un reviewer a relevé, à raison, que (a) le
journal `r407_merged.log` ne capture que l'ENTRÉE de `sub_821F92B8`,
pas la branche interne empruntée — la lecture de source établit qu'IL
EXISTE un chemin de retour anticipé (table de 64 segments) qui
n'appelle jamais le noyau ; et (b) `0x60002000` n'a jamais été comparé
aux constantes réelles (`X_MEM_RESERVE=0x2000`, `X_MEM_COMMIT=0x1000`,
vérifiées dans `xtypes.h`) — c'est `RESERVE` seul (les deux premiers
appels, boucle de repli) suivi d'un appel SÉPARÉ à `0x60001000`
(`COMMIT` seul, `loc_821F9460`) sur l'adresse déjà réservée, pas un
seul appel combiné.

## Contexte

Suite de r406 : lire `sub_821F92B8` (`ppc_recomp.27.cpp:12734`, ~317
lignes) en entier pour identifier le stub hôte qu'elle appelle et
comment elle calcule la plage retournée.

## Établi

1. **`sub_821F92B8` commence par une recherche dans une table de 64
   pointeurs** (`heap+96` à `heap+352`, `4` octets chacun). Dans
   l'instantané déjà capturé par r401
   (`heap2_call1_size1.bin`, proche de `seq≈879`), **un seul slot est
   non nul : slot 0, pointeur `0x10000630`.** Le descripteur qu'il
   pointe (relu directement dans le même instantané) contient, entre
   autres, `+0x18=0x10000000` et `+0x2c=0x10100000` (cohérent avec UN
   segment couvrant `[0x10000000, 0x10100000)`, 1 Mo) et `+0x30=0xc`
   (12) — le champ comparé à la taille demandée par la boucle de
   recherche (`lwz r11,48(r4)` = offset `+0x30`, confirmé : `48
   décimal = 0x30`). **12 est très inférieur à toute taille demandée
   dans ce fil** (8888 octets pour `seq=874`, `0x80310` pour `seq=877`)
   — si ce champ est bien exprimé en unités utilisables directement,
   ce segment unique semble incapable de satisfaire l'une ou l'autre
   requête au moment de cet instantané.
2. **Si aucune entrée ne convient**, la fonction appelle
   `NtAllocateVirtualMemory` (`0x823D037C`) DEUX fois avec `RESERVE`
   seul (`0x60002000`, boucle de repli qui divise la taille par deux à
   chaque échec), puis une TROISIÈME fois avec `COMMIT` seul
   (`0x60001000`, `loc_821F9460`) sur l'adresse retournée par la
   réservation — un motif classique réserve-puis-commit en deux étapes
   séparées, pas un seul appel combiné (corrige la première version de
   ce rapport).
3. **Le stub hôte `NtAllocateVirtualMemory_entry`
   (`.../thirdparty/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_memory.cpp:57`)
   délègue, pour une base nulle, à `BaseHeap::Alloc` via
   `LookupHeapByType`** — sous-système générique du même fichier que
   `MmAllocatePhysicalMemoryEx_entry` (ligne 343, même fichier), déjà
   mis en cause par r389 pour un chevauchement avec la freelist
   vivante. **Les deux fonctions appartiennent à la même famille de
   stubs hôtes tiers (`upstream/AC6_recomp/thirdparty/rexglue-sdk/`) —
   le rapprochement avec r389 est donc plus direct qu'une première
   lecture ne le suggérait, pas moins.**

## Non établi

- **Quel chemin l'appel `seq=877` a réellement pris.** Le journal
  `r407_merged.log` capture seulement l'ENTRÉE de `sub_821F92B8`, pas
  ses branches internes. Le point 1 ci-dessus (un seul segment,
  apparemment épuisé) rend le chemin noyau PLAUSIBLE mais ne le
  confirme pas — le champ `+0x30=12` n'est pas confirmé comme étant la
  même unité que la taille demandée, et l'instantané date d'APRÈS les
  événements, pas d'avant.
- **`BaseHeap::Alloc`/`LookupHeapByType`** (`xmemory.cpp:1031`/`353`,
  2070 lignes) ne sont pas lues.
- Si le chemin noyau est confirmé, la plage exacte retournée par le
  premier `NtAllocateVirtualMemory` (réservation) pour `seq=877` n'est
  pas capturée.

## Décisions prises

- Corriger la première version de ce rapport avant publication (chemin
  supposé, constante mal décodée) plutôt que publier une conclusion
  non établie — un reviewer a détecté les deux erreurs avant qu'elles
  n'entrent dans `NEXT.md`.
- Ne pas lire `BaseHeap::Alloc` en entier tant que la branche prise à
  `seq=877` n'est pas confirmée — lire 2070 lignes de code tiers avant
  de savoir si elles sont pertinentes serait disproportionné. Nommé
  pour r408 uniquement si la capture confirme le chemin noyau.

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF -> exit 0
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> exit 0
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> exit 0
```
`ctest` relancé en entier : même échec préexistant et sans rapport que
r404 (`ac6-cpp-complexity`), 87/88 (99%).

## Named for r408

Étendre `r407_merged.gdb` (même run, même compteur de séquence) avec
un point d'arrêt sur `*__imp__sub_821F8368` (chemin "segment
existant") ET sur `*__imp__NtAllocateVirtualMemory` (chemin noyau),
tous deux loggant `seq`, pour déterminer directement lequel des deux
chemins l'appel `seq=877` (et `876`) emprunte. Si `NtAllocateVirtualMemory`
est atteint, capturer sa plage retournée (point d'arrêt sur son retour,
lecture de `*base_addr_ptr`/`*region_size_ptr` via `base_`) et la
comparer à `0x10082aa0`. Si `sub_821F8368` est atteint à la place,
réexaminer le champ `+0x30` du descripteur de segment (candidat pour la
désynchronisation, hypothèse non vérifiée) plutôt que le chemin noyau.

## Files

Aucun nouvel artefact capturé ce cycle (lecture de source et
relecture arithmétique d'un instantané déjà capturé par r401).
