# AC6 retail NTSC-U/J — r406 — ordre exact établi en direct : le nœud du bucket 17 (`0x10082aa0`) est un reliquat ordinaire créé par un split normal, TROIS appels avant que le chemin de croissance (`sub_821F92B8`, jamais lu avant ce cycle) ne rende au pool une mémoire qui le recouvre — sans libération entre les deux ; corrige r404, qui datait l'un des deux événements dans le mauvais sens

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

**Ce rapport a été réécrit une fois avant publication.** Une première
version, construite sur la seule capture d'ordre `r406_ordering.log`
(sans point d'observation matériel), plaçait à tort l'insertion du
nœud `0x10082aa0` à `call1` (`seq=879`, DEUX appels APRÈS le pool). Un
reviewer a relevé que ceci contredisait la propre table de r404 (les
points d'observation matériel #2-#4, l'insertion, précèdent
chronologiquement #5-#6, `sub_82372128`, qui s'exécute DANS l'appel du
pool). Une capture fusionnée (points d'observation matériel + compteur
de séquence partagé, même run) tranche : voir "Établi".

## Contexte

r404 a établi que le nœud `0x10082aa0` (bucket 17) tombe à l'intérieur
du pool de 525 Ko (`sub_8236E868`, base `0x10011c60`), et qu'aucun des
47 pointeurs libérés sur l'intégralité du run n'est le pool lui-même.
Une relecture de ce même journal (`r405_free_check_r5.log`) montre que
**40 des 47 pointeurs libérés tombent DANS la plage du pool**
(`[0x10011c60, 0x10091f80)`) — un fait présent dans les données de r404
mais non interprété. Ce cycle détermine l'ordre exact des événements
pour trancher le sens de la causalité.

## Méthode

Capture fusionnée, un seul run : les trois points d'observation
matériel de r404 (`0x10000208`, `0x10082aa8`, `0x10082aac`) PLUS un
compteur de séquence partagé, incrémenté à chaque entrée de
`sub_821F9E10` (loggant `r4`/`r5`) et de `sub_821FA6F8` (loggant `r5`),
PLUS un point d'arrêt sur l'entrée de `sub_821F92B8` (la fonction
"croissance" nommée par r405 depuis `loc_821FA124`, jamais tracée en
direct avant ce cycle), tous partageant le même compteur. Voir
`r407_merged.gdb`/`.log`.

## Établi

1. **Les 40 libérations dans la plage du pool précèdent toutes sa
   création, avec une marge confortable et aucune libération après.**
   Dernière libération dans cette plage : `seq=838`
   (`r5=0x10080010`). Aucun `FREE` entre `838` et la fin du run
   (`seq=883`). Réutilisation mémoire légitime du côté des petits blocs
   — n'explique pas le chevauchement (voir point 3).
2. **Le nœud `0x10082aa0` est inséré dans le bucket 17 à `seq=874`, par
   un split ORDINAIRE, PAS par le pool.** `ALLOC-ENTER seq=874
   r4=0x8 r5=0x22b8` (8888 octets) ; ses propres points d'observation
   matériel #2-#4 se déclenchent PENDANT cet appel (pile confirmée :
   `sub_821F9E10 <- sub_821F7A88 <- sub_821F59E0 <- sub_821D74A8 <-
   sub_823B86D0 <- sub_823B8770`, identique à la chaîne déjà relevée
   par r404). C'est une insertion de reliquat après découpe — le
   mécanisme lu par r405 dans `loc_821FA1D4`, en usage normal.
3. **Le pool (`seq=877`, `sub_8236E868`, `r4=0 r5=0x80310`) tombe TROIS
   appels plus tard, sans AUCUNE libération entre les deux, et son
   exécution appelle `sub_821F92B8` (le chemin "croissance").** Ordre
   exact : `874` (insère `0x10082aa0` dans bucket 17) → `875` (petit
   appel, `r5=8`) → `876` (`r5=0x1c54=7252`, appelle DÉJÀ
   `sub_821F92B8`) → `877` (pool, `r5=0x80310`, appelle AUSSI
   `sub_821F92B8`). Pendant `877`, les points d'observation matériel
   #5-#6 (`sub_82372128` formatant le pool) écrasent les champs de
   chaînage retour du nœud `0x10082aa0` — **c'est la même mémoire
   physique**, confirmé une seconde fois indépendamment de r404.
4. **Deux appels consécutifs immédiatement avant le pool
   (`seq=876` et `877`) tombent tous les deux dans `sub_821F92B8`** —
   cohérent avec un arbre de grands blocs épuisé à ce moment précis
   (le split de `874` a consommé le dernier grand bloc disponible,
   laissant un petit reliquat en bucket 17 plutôt qu'en réinsérant un
   nouveau grand nœud dans l'arbre).

## Ce que ceci établit pour r399 (corrige r404, PAS une simple confirmation)

**Le sens de causalité de r404 était inversé.** Le nœud du bucket 17
n'est pas "une insertion tardive dans un bloc déjà remis" — c'est un
reliquat ORDINAIRE et légitime au moment de sa création (`seq=874`).
Le vrai défaut est que **`sub_821F92B8`, trois appels plus tard, rend
au pool une plage mémoire (`0x10011c60`..`0x10091f80`) qui recouvre une
adresse (`0x10082aa0`) déjà distribuée par le tas au niveau octet**,
sans qu'aucune libération n'ait pu légitimement rendre cette adresse
disponible entre les deux événements. Ceci pointe vers un défaut de
synchronisation entre deux niveaux de gestion mémoire : le tas
octet-par-octet (buckets/arbre, déjà bien compris depuis r401) et le
mécanisme de croissance `sub_821F92B8` (jamais lu), qui semble opérer
sur une plage d'adresses sans consulter — ou sans consulter
correctement — ce que le tas octet-par-octet a déjà distribué. **C'est
structurellement le même type de défaut que celui documenté par r389
pour `MmAllocatePhysicalMemoryEx`** (une plage retournée par un stub
hôte qui chevauche la freelist vivante), mais sur une fonction et un
chemin différents, jamais rapprochés jusqu'ici.

## Non établi

- **Le contenu de `sub_821F92B8` : quel stub hôte elle appelle, et
  comment elle calcule la plage qu'elle retourne.** C'est la seule
  pièce manquante pour confirmer le mécanisme exact du chevauchement.
- **Si le chevauchement vient de `sub_821F92B8` elle-même (mauvais
  calcul de plage) ou du stub hôte qu'elle appelle** (candidat
  structurel : le même stub que r389 a déjà mis en cause, ou un stub
  frère non encore audité).
- Le rapport précédent (première version de ce cycle, non publiée)
  concluait à tort que `call1` (`seq=879`) était l'insertion — corrigé
  ici avant publication par la capture fusionnée, comme documenté en
  tête de ce rapport.

## Décisions prises

- Réécrire ce rapport avant publication plutôt que publier une
  conclusion dont la propre preuve tabulaire de r404 (ordre relatif
  des points d'observation matériel #2-4 vs #5-6) contredisait déjà le
  sens retenu — discipline déjà appliquée par r403/r404 sur eux-mêmes.
- Ne pas deviner le contenu de `sub_821F92B8` sans la lire — nommée
  pour r407, pas conclue ici (précédent r1111/r1113).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF -> exit 0
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> exit 0
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> exit 0
```
`ctest` relancé en entier : même échec préexistant et sans rapport que
r404/r405 (`ac6-cpp-complexity`, `native_xenos_tests.cpp` non committé
à 4241 lignes), 87/88 (99%).

## Named for r407

Lire `sub_821F92B8` en entier (`ppc_recomp.27.cpp`, appelée depuis
`loc_821FA124` de `sub_821F9E10`) pour identifier le stub hôte qu'elle
appelle et comment elle calcule la plage retournée. Si elle appelle un
stub déjà audité par une campagne antérieure (candidat nommé :
`MmAllocatePhysicalMemoryEx`, déjà mis en cause par r389 pour un
chevauchement structurellement identique), rapprocher explicitement
les deux fils plutôt que les traiter comme indépendants. Vérifier en
direct (capture de la plage exacte retournée par `sub_821F92B8` à
`seq=877`, comparée à `0x10082aa0`) avant tout correctif.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r406_ordering.gdb/.log`,
`gdb-stdout-r406.log`, `r407_merged.gdb/.log`, `gdb-stdout-r407.log`.
