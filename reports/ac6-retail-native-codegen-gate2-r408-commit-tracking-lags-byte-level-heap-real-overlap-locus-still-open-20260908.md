# AC6 retail NTSC-U/J — r408 — le tas octet-par-octet écrit dans une page réservée-mais-non-commise dès `seq=348` : sur du matériel réel ceci lèverait une faute d'accès, donc la divergence recomp/retail précède TOUT ce que r358-r407 ont examiné ; origine non établie

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

**Ce rapport a été réécrit une fois avant publication** — une première
version, construite sur la seule capture des résultats
`NtAllocateVirtualMemory` (`r408b`), concluait que la commission à
`seq=877` était la première à couvrir `0x10082aa0`/`0x10080000`. Un
reviewer a relevé que cette lecture ne vérifiait pas si le tas
octet-par-octet utilisait DÉJÀ cette plage AVANT `seq=877` — une
capture séparée (`r408c`, déjà en cours d'exécution en parallèle de la
rédaction) montre que oui, dès `seq=348`, ce qui inverse et déplace
la conclusion.

## Contexte

Suite de r407 : trancher lequel des deux chemins de `sub_821F92B8`
(segment existant / `NtAllocateVirtualMemory`) l'appel `seq=877`
emprunte, et capturer la plage exacte retournée.

## Établi

**Deux runs distincts, résultats combinés par corrélation de `seq`
(déterministe d'un run à l'autre — confirmé par r406/r407/r408b/r408c
qui retrouvent tous `seq=874`/`876`/`877` aux mêmes événements) :**

1. **(run `r408b`, TOUS les appels `NtAllocateVirtualMemory` du run
   loggés sans filtre)** : `seq=0` (deux appels, pile
   `sub_821F9860 <- sub_821F7D50 <- sub_821F7E28 <- __xstart` —
   l'initialiseur du tas déjà identifié par r404, hit #1) réserve
   `[0x10000000,0x10100000)` (`flags=0x60002000`) puis commet
   `[0x10000000,0x10010000)` (`flags=0x60001000`) ; `seq=839` : plage
   sans rapport (`0x16540000`) ; `seq=876` :
   `[0x10010000,0x10020000)` ; `seq=877` : `[0x10020000,0x100b0000)`
   — **aucune commission capturée entre `seq=0` et `seq=877` ne couvre
   `0x10080000`.**
2. **(run `r408c`, points d'observation matériel sur `0x10080000`
   ET `0x10082aa0`, armés dès l'entrée du constructeur
   `GuestAddressSpace`, plafonné à 14 déclenchements)** : les 14
   déclenchements portent TOUS sur `0x10080000` (point d'observation
   matériel #6, confirmé par `watch-armed`/`Hardware watchpoint`
   croisés) — **`0x10082aa0` (point d'observation matériel #7) ne
   s'est jamais déclenché dans ce run**, qui s'est arrêté à `seq=485`,
   bien avant `seq=874` (insertion du nœud, établie par r406/r407).
   Ce rapport ne dit donc rien sur `0x10082aa0` lui-même — seulement
   sur `0x10080000`. Premier déclenchement : `seq=348`, pile
   `sub_821F94F8 <- sub_821FA6F8 <- sub_821F7AD0 <- sub_821D7568 <-
   sub_82396DC0 <- sub_82397E38` — **c'est un `free()`
   (`sub_821FA6F8`), pas un `alloc()`.** Suivent des déclenchements
   alternant `alloc` (`sub_821F9E10`) et coalescence post-free
   (`sub_821F85F8`) jusqu'à `seq=485` (plafond du run).
3. **Constantes vérifiées** (`xtypes.h`) : `flags=0x60002000` =
   `X_MEM_HEAP(0x40000000) | X_MEM_LARGE_PAGES(0x20000000) |
   X_MEM_RESERVE(0x2000)` ; `flags=0x60001000` = `X_MEM_HEAP |
   X_MEM_LARGE_PAGES | X_MEM_COMMIT(0x1000)`. `X_MEM_HEAP` n'est PAS
   un flag de commission automatique dans cette énumération —
   `RESERVE` et `COMMIT` restent deux appels distincts, la lecture de
   r407 tient.

## Ce que ceci établit pour r399 — LA vraie divergence est plus en amont que r358-r407

**Sur du matériel Xbox 360 réel, toucher une page réservée-mais-non-commise
lève une violation d'accès.** Le titre retail s'exécute correctement sur
console. Or ici, le tas octet-par-octet du jeu écrit dans
`0x10080000` (`seq=348`) alors qu'AUCUNE commission capturée ne couvre
cette adresse avant `seq=877` — 529 appels plus tard. **Le code invité
est déterministe étant donné les valeurs retournées par les stubs
hôtes qu'il appelle : si le comportement diverge du matériel réel à
`seq=348`, une valeur retournée par un stub hôte, quelque part ENTRE
`seq=0` et `seq=348`, diffère déjà de ce que retournerait la console.**
Le double-octroi bucket-17 documenté par r404-r407 (`seq=874`/`877`)
n'est alors probablement qu'un SYMPTÔME tardif d'un état de tas déjà
faux depuis `seq≈348` ou avant — pas la cause elle-même. Ceci
recontextualise toute la chaîne r358-r407 : chacun de ces cycles a
examiné un mécanisme d'allocation correct EN LUI-MÊME, opérant sur un
état de tas déjà corrompu par quelque chose de bien plus tôt.

**Anomalie non expliquée, notée sans interprétation** : le PREMIER
accès capturé à `0x10080000` est un `free()`, pas un `alloc()` — aucune
allocation observée n'a jamais écrit l'en-tête de ce bloc pour la
première fois dans ce run. Soit l'allocation initiale qui l'a créé a
eu lieu AVANT que le point d'observation matériel ne soit armé (avant
l'entrée du constructeur `GuestAddressSpace` — improbable, c'est très
tôt), soit le pointeur libéré à `seq=348` (via
`sub_821F94F8 <- sub_821FA6F8`) n'a jamais été légitimement alloué par
CE tas.

## Non établi

- **Ce qui, entre `seq=0` et `seq=348`, fait diverger l'état du tas de
  ce qu'il serait sur matériel réel.** Piste directement suggérée :
  l'implémentation hôte de `NtAllocateVirtualMemory`/`BaseHeap` sur ce
  portage Linux natif pourrait mapper la RÉSERVATION initiale
  entière (`[0x10000000,0x10100000)`, `seq=0`) comme accessible en
  lecture/écriture côté HÔTE dès la réservation (indépendamment des
  commissions "logiques" ultérieures) — expliquerait pourquoi le
  processus ne plante PAS ici alors qu'il le ferait sur console, mais
  n'explique PAS pourquoi le code invité DÉCIDE d'utiliser
  `0x10080000` si tôt. Hypothèse non vérifiée (nécessiterait de lire
  `BaseHeap::Alloc`/`AllocFixed`).
- **L'origine du pointeur libéré à `seq=348`** (anomalie free-avant-alloc
  ci-dessus).
- **Si `0x10082aa0` (le nœud litigieux de r404-r407) est concerné par
  la même divergence précoce** — non capturé, `r408c` s'est arrêté à
  `seq=485`.
- `sub_821F8A00` (insertion/coalescence post-croissance) et le
  watermark utilisé par `sub_821F92B8` (`heap+32`/`+36`) — toujours pas
  lus.

## Décisions prises

- Réécrire ce rapport avant publication dès qu'une capture parallèle
  (`r408c`) a révélé une preuve qui inverse et déplace la conclusion de
  la première version — discipline déjà appliquée par r403/r404/r406
  sur eux-mêmes.
- Ne pas deviner l'origine de la divergence `seq<348` sans capture
  supplémentaire — nommée pour r409, pas conclue ici (précédent
  r1111/r1113).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF -> exit 0
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> exit 0
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> exit 0
```
`ctest` relancé en entier : même échec préexistant et sans rapport que
r404 (`ac6-cpp-complexity`), 87/88 (99%).

## Named for r409

Dans cet ordre (le moins cher d'abord) :
1. Lire `sub_821F9860` (l'initialiseur du tas, déjà identifié comme
   émetteur des deux appels `seq=0` par `NTALLOC-BT`) pour voir sa
   logique de dimensionnement — la commission initiale de 64 Ko
   (`seq=0`, `[0x10000000,0x10010000)`) est-elle vraiment tout ce que
   le tas croit posséder au démarrage, ou une structure interne
   (tree/segment) est-elle initialisée avec une taille plus grande (la
   réservation entière, 1 Mo) ?
2. Capturer un instantané du tas tôt (`seq≈5`) et lire `heap+384`
   (racine de l'arbre des grands blocs, déjà repérée par r402/r403) et
   le descripteur de segment à `0x10000630` (`+0x30`, déjà lu par r407)
   pour voir quelle taille ils encodent à ce moment.
3. Étendre `r406_ordering.gdb` avec une capture de RETOUR
   (`FinishBreakpoint` sur `sub_821F9E10`, loggant `r5_in`/`r3_out`) et
   chercher le premier retour `0x1008xxxx` avant `seq=348` — s'il
   n'existe pas, le pointeur libéré à `seq=348` n'a jamais été émis par
   CE tas.
4. Mettre `MmAllocatePhysicalMemoryEx` (suspect nommé par r389, jamais
   placé sur cette même chronologie) sur le même compteur de séquence
   partagé — candidat direct pour "qui a distribué `0x10080xxx` avant
   `seq=348`".

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r408_branch.gdb/.log`,
`gdb-stdout-r408.log`, `r408b_ntalloc_result.gdb/.log`,
`gdb-stdout-r408b.log`, `r408c_discriminator.gdb/.log`,
`gdb-stdout-r408c.log`.
