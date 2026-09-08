# AC6 retail NTSC-U/J — r405 — `loc_821FA1D4` (reliquat) et `loc_821FA0BC` (grand bloc) lus en entier dans `sub_821F9E10` : ni l'un ni l'autre n'est manifestement fautif à la lecture seule ; la branche précise reste à tracer en direct

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r404 : lire en entier les deux chemins jamais lus de
`sub_821F9E10` (`ppc_recomp.27.cpp:16342`, la copie de labels à
16733-17334 — le fichier contient une seconde copie des mêmes labels à
15542-16143 appartenant à une AUTRE fonction, confirmé par
`grep -n "^PPC_FUNC_IMPL"` : seule l'occurrence à 16342 tombe dans la
plage 16733-17334).

## Établi (lecture de source uniquement, aucune capture en direct ce cycle)

1. **`loc_821FA1D4` (16881-17032) est une insertion de reliquat
   ordinaire, triée par taille — rien d'évidemment fautif à la
   lecture.** `r30 = ((r29<<4)&~0xF) + r3` construit l'adresse du
   reliquat (`r3` = base du bloc trouvé, `r29` = taille demandée
   arrondie en unités de 16). Le code écrit l'en-tête du reliquat
   (`+5`, `+2`, `+4`, `+0`), recalcule sa PROPRE classe de taille depuis
   la taille restante, puis :
   - si la classe < 128 : insertion directe dans le bucket exact
     (`r27+(classe+48)*8`), avec la même vérification `lwz+cmplw` d'auto-
     référence déjà vue par r403 (chemin `loc_821FA288`/`loc_821FA310`
     selon un bit de flag à `r10&0x10`) ;
   - sinon : parcours d'un arbre trié par taille (boucle
     `loc_821FA264`/`loc_821FA324`, comparaison sur `-8(r11)`) pour
     trouver le point d'insertion, puis liaison doublement chaînée
     classique à `loc_821FA2F4`/`loc_821FA348`.
   Ce mécanisme est celui d'un allocateur à reliquat correctement
   dimensionné — **s'il calcule `r29` (la taille demandée) correctement
   en amont**, ce chemin ne peut pas, à lui seul, faire pointer un
   reliquat à l'intérieur d'un bloc encore vivant. La question se
   déplace donc vers CE QUI APPELLE ce chemin avec quel `r3`/`r29`.
2. **`loc_821FA0BC` (16733-16825 pour son prologue jusqu'à
   `loc_821FA168`) est le chemin "grand bloc"** (`r29>=128`, atteint
   quand le bucket exact était vide et qu'aucun bucket bitmap plus
   grand n'a été trouvé par `loc_821F9F80`, cf. r402/r403). Il compare
   `r29` à `r30+28` (une borne stockée dans l'objet tas) et saute à
   `loc_821FA564` (chemin `NtAllocateVirtualMemory`, jamais lu) si la
   requête dépasse cette borne. Sinon, il parcourt un arbre trié par
   taille enraciné à `r30+384` (boucle `loc_821FA0C8`/`loc_821FA0F8`,
   même motif de comparaison sur `-8(r11)` que dans `loc_821FA1D4`) ;
   s'il épuise l'arbre sans trouver de bloc assez grand
   (`loc_821FA124`), il appelle **`sub_821F92B8`** (jamais nommée dans
   ce fil avant ce cycle) — puis, que `sub_821F92B8` ait réussi ou que
   le parcours ait trouvé un bloc existant, la suite (`loc_821FA138`)
   déchaîne le bloc trouvé de l'arbre par la même triple vérification
   que r403 a déjà validée comme correcte, avant de tomber dans
   `loc_821FA168` puis, potentiellement, `loc_821FA1D4`.

## Non établi

- **`sub_821F92B8` n'est pas encore lue.** C'est la seule fonction
  nouvellement nommée ce cycle qui pourrait introduire ou retourner un
  bloc déjà attribué ailleurs (candidate la plus probable pour un
  chevauchement, puisqu'elle seule "fabrique" de la mémoire neuve dans
  ce chemin plutôt que de réarranger de la mémoire déjà connue de
  l'allocateur).
- **`loc_821FA564` (chemin `NtAllocateVirtualMemory` direct) n'est pas
  lue.** r389 a déjà montré en direct, dans un cycle antérieur, que
  `MmAllocatePhysicalMemoryEx` (un stub différent, pas
  `NtAllocateVirtualMemory`) peut retourner une plage chevauchant la
  freelist vivante — un candidat structurellement similaire, jamais
  réconcilié avec ce chemin précis.
- **Aucune capture en direct ce cycle.** La lecture de source seule ne
  permet pas de dire QUELLE invocation de `sub_821F9E10` (parmi les 587
  du run) a emprunté `loc_821FA1D4` avec un `r3`/`r29` qui produit
  `0x10082aa0`, ni si c'est ce chemin ou `sub_821F92B8` qui est en
  cause. Conformément à la discipline du projet ("un contrôle direct
  avant conclusion"), aucune hypothèse n'est retenue comme probable
  plutôt qu'une autre à ce stade.

## Décisions prises

- Ne pas publier de conclusion sur la ligne fautive sans capture en
  direct de l'invocation précise (chaîne
  `sub_821F7A88 <- ... <- sub_823A5BA0`, déjà identifiée par r404) —
  précédent r1111/r1113. Ce rapport documente une lecture de source
  complète des deux chemins nommés par r404, pas une conclusion.
- Nommer `sub_821F92B8` pour lecture avant toute capture, plutôt que
  déjà supposer qu'elle est en cause — c'est la fonction la plus
  probable mais ce n'est qu'une hypothèse non vérifiée à ce stade.

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF -> exit 0
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> exit 0
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> exit 0
```
`ctest` : même échec préexistant et sans rapport documenté par r404
(`ac6-cpp-complexity`, `native_xenos_tests.cpp` non committé à 4241
lignes) — pas revérifié en entier une seconde fois ce cycle, aucune
source de production éditée.

## Named for r406

1. Lire `sub_821F92B8` en entier (`ppc_recomp.27.cpp`, appelée depuis
   `loc_821FA124`) — la fonction la plus probable pour introduire un
   chevauchement, puisqu'elle seule fabrique potentiellement de la
   mémoire neuve dans ce chemin.
2. ~~Capturer en direct l'invocation précise de `sub_821F9E10`
   identifiée par r404, tracer ses branches internes.~~ **Remplacé
   avant que r406 ne l'exécute** : une capture d'ordre (compteur de
   séquence partagé sur les entrées de `sub_821F9E10`/`sub_821FA6F8`,
   sans filtre par pile d'appel) s'est avérée trancher directement la
   question sans qu'un traçage de branches internes soit nécessaire —
   voir `reports/ac6-retail-native-codegen-gate2-r406-*.md`.

## Files

Aucun nouvel artefact capturé ce cycle (lecture de source uniquement).
