# AC6 retail NTSC-U/J — r410 — l'écrivain n'est PAS un sous-système sans rapport : c'est le tas général lui-même, qui libère (`seq=6`) le tampon d'un tableau croissant du C++ statique puis continue d'y écrire après l'avoir rendu au freelist

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

**Ce rapport a été réécrit deux fois avant publication.** Une première
version concluait que `loc_821FA288` (fusion de voisin de
`sub_821F9E10`) lisait mal un voisin encore vivant — infirmé par une
lecture directe de l'en-tête (§ ci-dessous). Une deuxième version
concluait que l'écrivain réel, `sub_8237FA50`, était un sous-système
« sans rapport avec l'allocateur ». **Cette deuxième conclusion était
elle-même fausse** : la lecture du code PPC de `sub_8237FA50` et de son
appelant `sub_821F7B28`, plus une capture en direct de la pile d'appel
exacte au moment du `free()` litigieux, montrent que l'écrivain est un
CLIENT ORDINAIRE de l'allocateur général — un tableau croissant utilisé
par la boucle d'initialisation statique du C++ (`sub_821F7B28`) — et
que le mot corrompu naît d'un use-after-free : ce tableau libère son
propre tampon via `sub_821F9E10`/`sub_821FA6F8` puis continue d'écrire
dedans. Voir "Correction 2" et "Établi".

## Contexte

Suite de r409 (nommé pour r410) : convertir les adresses hôte des
écritures capturées par r409 en labels PPC, et capturer en direct le
registre source de `0x823f`.

## Correction 1 (déjà publiée dans une version antérieure, résumée)

`r413_offsets.gdb` a identifié `loc_821FA288` (texte déjà cité par
r405) comme le code qui fusionne inconditionnellement un mot lu au
voisin physique du reliquat dans le calcul de taille, sans
revérification indépendante de sa fraîcheur. `r414_header_trace.gdb`
a montré que l'en-tête du voisin (`0x100007f0`) était CORRECT à
`seq=3`-post-alloc et `seq=6`-post-free (`{taille=0x59, flags+5=0x1}`,
un bloc vivant légitime), mais DÉJÀ corrompu à `seq=8`-pre
(`{0x823cf728, 0x823cf740, 0x823cf758, 0x823cf770}`). `sub_821F9E10`
ne fait donc que lire (et propager) une corruption déjà survenue avant
son exécution — ce tracé reste exact.

## Correction 2 (celle qui invalide la version précédente de CE rapport)

`r415b_seq6to8_writer.log` avait localisé l'écrivain : quatre écritures
consécutives à `0x100007f0`..`+0xc`, toutes pendant que `seq` vaut
encore `6`, pile d'appel `sub_8237FA50 <- sub_8237FB58 <-
sub_821F7B28 <- __xstart <- std::thread::_State_impl<...>::_M_run()`.
La version précédente de ce rapport s'arrêtait là et concluait « sans
rapport avec l'allocateur ». **Lire le code PPC de ces trois fonctions
réfute cette conclusion :**

- `sub_821F7B28` (`ppc_recomp.27.cpp:9177`) est un DISPATCHEUR de
  constructeurs statiques C++ : il itère une table de pointeurs de
  fonction entre deux bornes fixes de l'image (`0x8213xxxx`..) et
  appelle chaque entrée non nulle par `bctrl` — le motif standard
  `.init_array`/`_GLOBAL__sub_I_*`, exécuté depuis `__xstart` avant
  `main`. Ce n'est pas un sous-système isolé, c'est LA boucle
  d'initialisation statique du binaire tout entier.
- `sub_8237FA50` (`ppc_recomp.63.cpp:7717`) est un `push_back` sur un
  tableau croissant global : il lit un pointeur de début (`r28`,
  global à l'adresse invité `0x82a5eef0`) et un pointeur de fin
  (`r30`, global à `0x82a5eeec`), interroge la capacité du tampon
  courant via `sub_82385AF0(r28)`, et si elle est insuffisante,
  agrandit le tampon via `sub_823857E0(r28, taille_demandée)` — une
  fonction qui (§ ci-dessous, capture en direct) alloue un nouveau
  tampon **et libère l'ancien**. Sinon (capacité suffisante), il écrit
  simplement l'argument (`r23`, `ctx.r3` d'entrée) à `*r30`, avance
  `r30` de 4 octets, et persiste le nouveau `r30` comme fin globale.
  C'est un motif `std::vector<T*>::push_back` ordinaire.

**Capture décisive (`r417_free6_caller.gdb`, pile d'appel exacte au
`free()` de `seq=6`, pas une corrélation par compteur)** :

```
FREE seq=6 r5=0x10000770
BT: sub_821FA6F8 <- sub_821FA9E0 <- sub_823857E0 <- sub_8237FA50
    <- sub_8237FB58 <- sub_821F7B28 <- __xstart <- ...::_M_run()
```

Le pointeur libéré à `seq=6` (`0x10000770`) est EXACTEMENT le pointeur
retourné par `seq=2` (r409 : allocation de 128 octets, en-tête
`0x10000760`, retour `0x10000770`) — c'est le tampon du tableau
croissant lui-même, en train d'être agrandi.

**Capture confirmatoire (`r416_array_bounds.gdb`, lecture directe des
deux globales de contrôle `0x82a5eef0`/`0x82a5eeec` à chaque
déclenchement)** :

| moment | `begin` (`0x82a5eef0`) | `end` (`0x82a5eeec`) |
|---|---|---|
| juste après le retour du `free()` `seq=6` | `0x10000770` | `0x100007f0` |
| WATCH-STOP #1 (écrit `0x100007f0`) | `0x10000770` | `0x100007f0` |
| WATCH-STOP #2 (écrit `0x100007f4`) | `0x10000770` | `0x100007f4` |
| WATCH-STOP #3 (écrit `0x100007f8`) | `0x10000770` | `0x100007f8` |
| WATCH-STOP #4 (écrit `0x100007fc`) | `0x10000770` | `0x100007fc` |

`0x10000770 + 0x80 = 0x100007f0` : `end` est exactement à la borne de
capacité du bloc de 128 octets alloué par `seq=2` (9 unités × 16 −
16 octets d'en-tête = 128 octets utiles, arithmétique déjà validée par
r409). Les quatre écritures dépassent cette borne d'un mot à chaque
appel, et **`begin` ne change JAMAIS** — alors qu'un `free()` du tampon
qu'il désigne vient d'avoir lieu, dans le MÊME appel à `sub_8237FA50`,
via son propre chemin d'agrandissement.

## Établi

1. **L'écrivain de `0x100007f0..+0xc` est le tas général lui-même,
   agissant comme client de sa propre API** (`sub_8237FA50`, un
   `push_back` de tableau global, appelé depuis la boucle des
   constructeurs statiques C++) — pas un sous-système indépendant.
2. **Le tampon corrompu est un use-after-free interne au tas** : ce
   même appel à `sub_8237FA50` libère son ancien tampon
   (`free(0x10000770)`, capturé par pile d'appel exacte à `seq=6`) puis
   continue d'écrire à travers `r30` (le pointeur de fin) sans que ce
   dernier — ni le pointeur de début persistant, `begin=0x10000770`,
   confirmé inchangé sur les quatre écritures — n'ait été remis à jour
   vers un nouveau tampon.
3. **Ceci ferme la boucle avec r409** : le "nœud fantôme" et le bit de
   fraîcheur mal vérifié par `loc_821FA288` ne sont pas la cause — ils
   PROPAGENT une valeur déjà écrite par ce use-after-free, exactement
   comme établi par la Correction 1.

## Ce que ceci établit pour r399

**La chaîne r399-r409 documente les symptômes d'un bug d'ordre
d'exécution DANS l'allocateur général lui-même** (ou dans la fonction
qu'il appelle pour agrandir un tampon, `sub_823857E0`/`sub_821FA9E0`) :
un agrandissement libère l'ancien tampon avant — ou sans jamais —
faire pointer les écritures suivantes vers le nouveau. C'est un bug de
la famille r358-r409 (l'allocateur général), pas un sous-système
étranger qui empiète sur lui par accident. Ceci recentre
l'investigation exactement là où r399 l'avait placée à l'origine,
après un détour de deux versions de ce rapport.

## Non établi

- **Le mécanisme exact du use-after-free** : registre `r30`
  (pointeur de fin) resté périmé après l'appel à `sub_823857E0`
  (convention `PPC_CONFIG_NON_ARGUMENT_AS_LOCAL` mal traduite pour ce
  registre à travers l'appel ?), ou `sub_823857E0` lui-même retournant
  au tas général une taille demandée mais ne livrant pas réellement un
  tampon plus grand (le tas général réutilise le bloc tout juste
  libéré pour satisfaire la nouvelle requête, sans l'agrandir) — les
  deux hypothèses sont compatibles avec les captures ci-dessus, aucune
  n'est tranchée. Une capture supplémentaire (`r418_grow_call.gdb`) a
  tenté de lire `ctx.r3`/`ctx.r4` à l'entrée de `sub_823857E0` pour
  trancher, mais a produit une valeur incohérente
  (`r3=0x8feffc90`, hors de la plage du tas invité) malgré une pile
  d'appel confirmée correcte par `bt` — DÉCISION : ne pas publier cette
  valeur comme un fait, l'instrument (lecture par offset fixe `ctx+8`)
  n'est pas validé pour ce point d'arrêt précis, contrairement à
  `ctx+0x10`/`ctx+0x18` (`r4`/`r5`) déjà croisés avec les logs de
  tailles connues dans d'autres captures. Named for r411.
- **Le code PPC de `sub_823857E0` et `sub_821FA9E0`** — aucun des deux
  n'a été lu directement ; c'est la lecture qui tranchera entre les
  deux hypothèses ci-dessus.
- **Si ce même motif (use-after-free interne à un agrandissement)
  explique le reste de la chaîne r399-r409** (le double-octroi
  bucket-17 à `seq=874`/`877`) — probable vu que r409 a déjà établi
  que le nœud fantôme n'est qu'une propagation, mais non vérifié
  formellement.
- Aucun correctif proposé ni appliqué.

## Décisions prises

- Réécrire cette section une deuxième fois dès que la lecture directe
  du code PPC de `sub_8237FA50`/`sub_821F7B28` a contredit la
  conclusion « sans rapport avec l'allocateur » de la version
  précédente — même discipline que r403/r404/r406/r408/r409 sur
  eux-mêmes.
- Ne pas publier la valeur `r418` (`ctx.r3=0x8feffc90` à l'entrée de
  `sub_823857E0`) comme un fait établi : l'offset de lecture n'est pas
  validé pour ce point d'observation précis, malgré une pile d'appel
  confirmée correcte séparément. Une valeur non croisée avec un
  contrôle indépendant n'est pas une preuve (précédent r1111/r1113).
- Ne pas deviner le mécanisme exact du use-after-free sans lire
  `sub_823857E0`/`sub_821FA9E0` — nommé pour r411, pas conclu ici.

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r411

Lire directement le code PPC de `sub_823857E0` et `sub_821FA9E0`
(l'agrandisseur de tampon et son wrapper d'allocation/libération) pour
trancher entre les deux hypothèses ouvertes ci-dessus : registre
`r30`/`r28` non rechargé après l'appel côté `sub_8237FA50`, ou
`sub_823857E0` lui-même qui échoue à agrandir réellement le tampon
avant de libérer l'ancien. Si nécessaire, réparer l'instrument de
lecture `ctx+offset` pour ce point d'arrêt (comparer avec un
désassemblage x86 du prologue de `__imp__sub_823857E0` plutôt que de
réutiliser l'offset validé pour `sub_821F9E10`).

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r413_offsets.gdb/.log`,
`r414_header_trace.gdb/.log`, `r415_seq7_writer.gdb`,
`r415b_seq6to8_writer.log`, `r416_array_bounds.gdb/.log`,
`r417_free6_caller.gdb/.log`, `r418_grow_call.gdb/.log`.
