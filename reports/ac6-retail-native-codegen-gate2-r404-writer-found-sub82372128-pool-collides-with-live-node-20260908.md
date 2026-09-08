# AC6 retail NTSC-U/J — r404 — une invocation de l'allocateur général insère en direct un nœud "libre" dans le bucket 17 depuis l'INTÉRIEUR d'un bloc de 525 Ko qu'il a lui-même déjà alloué et jamais libéré — ni codegen, ni taille mal demandée ; la branche précise reste à identifier (r405)

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r403 : nommé pour r404, armer un point d'observation matériel
sur l'en-tête du bucket 17 (`0x10000208`) et sur les deux champs de
chaînage retour du nœud (`0x10082aa8`/`0x10082aac`) depuis le tout début
du processus, pour capturer l'écrivain exact de l'insertion à moitié
faite identifiée par r403.

**Ce rapport a été réécrit deux fois avant publication, à chaque fois
sur une preuve directe qui réfutait la version précédente — les deux
versions abandonnées sont résumées en fin de document (§Fausses pistes
écartées avant publication) parce que le lecteur qui reviendra sur ce
fil doit savoir pourquoi.**

## Méthode (quatre captures en direct, même harnais r371/r398 réutilisé sans modification)

1. **Point d'observation matériel** (technique r371/r389) sur les trois
   adresses ci-dessus, depuis l'entrée du constructeur
   `ac6::native::GuestAddressSpace::GuestAddressSpace()`.
2. **Capture de registre ciblée** (technique r398) : `ctx.r3` juste
   avant l'appel indirect à l'offset hôte `+147` de
   `__imp__sub_8236E868`, et juste avant son appel à `sub_823721E8` à
   `+168`.
3. **Capture générale filtrée** sur l'entrée de `sub_821F9E10`
   (`r4==0 && r5>1000`), pour situer la requête réelle dans la séquence
   complète des appels (587 invocations totales dans ce run).
4. **Capture sur l'entrée de `sub_821FA6F8`** (la fonction `free`,
   identifiée par r356), filtrée sur `ctx.r5==0x10011c60` (l'adresse en
   question), sur l'intégralité du run. **Le filtre a été refait une
   fois avant publication** : une première capture avait filtré sur
   `ctx.r4`, une lecture du prologue de `sub_821FA6F8`
   (`ppc_recomp.27.cpp:18028`) montre `r30=r3` (le tas), `r28=r4`, mais
   c'est `r29=r5` qui est décrémenté de 16 et déréférencé directement
   comme pointeur de bloc (`lbz r11,5(r29)` etc.) — le pointeur libéré
   est le troisième argument, pas le second. Voir "Correction" pour le
   résultat de la capture refaite sur `r5`.

## Établi

### 1. L'insertion et l'écrasement du chaînage (point d'observation matériel, six déclenchements)

| # | adresse | ancien -> nouveau (vue PPC big-endian) | site | pile d'appel |
|---|---|---|---|---|
| 1 | `0x10000208` (en-tête bucket 17) | `0 -> 0x10000208` (auto-référence, vide) | `sub_821F9860` | `sub_821F7D50 <- sub_821F7E28 <- __xstart` |
| 2 | `0x10082aa8` (retour nœud, lo) | `0 -> 0x10000208` | `sub_821F9E10` | `sub_821F7A88 <- sub_821F59E0 <- sub_821D74A8 <- sub_823B86D0 <- sub_823B8770 <- sub_823B81F0 <- sub_823ADBD8 <- sub_823ADF78 <- sub_823A5BA0` |
| 3 | `0x10082aac` (retour nœud, hi) | `0 -> 0x10000208` | `sub_821F9E10` | idem |
| 4 | `0x10000208` (en-tête bucket 17) | `0x10000208 -> 0x10082aa8` | `sub_821F9E10` | idem |
| 5 | `0x10082aa8` (retour nœud, lo) | `0x10000208 -> 0` | `sub_82372128` | `sub_82372198 <- sub_823721E8 <- sub_8236E868 <- sub_8236B3F8 <- sub_82121308 <- sub_821D5F48 <- sub_821D7DE0 <- __xstart` |
| 6 | `0x10082aac` (retour nœud, hi) | `0x10000208 -> 0` | `sub_82372128` | idem |

`sub_821F9E10` (#2-#4) insère correctement et complètement un nœud
`0x10082aa0` en tête du bucket 17 — confirme une fois de plus
r401/r402/r403 : `sub_821F9E10` fait ce qu'il faut ICI. `sub_82372128`
(#5-#6, relu en entier, `ppc_recomp.61.cpp:3113` : boucle générique de
formatage de 8200 octets, base et index reçus en paramètre, aucune
adresse codée en dur) écrase ensuite les deux champs de chaînage retour
du nœud sans toucher l'en-tête du bucket.

Hit #1 (`sub_821F9860`) écrit uniquement l'en-tête du bucket 17
(auto-référence, bucket vide) à l'initialisation du tas — c'est
l'initialiseur de l'objet tas lui-même (chaîne
`sub_821F7D50 <- sub_821F7E28 <- __xstart`, première fois que cette
fonction est nommée dans ce fil de r399). Une version antérieure de
cette table (abandonnée avant publication) groupait par erreur ce hit
avec des écritures sur `0x10082aa8`/`0x10082aac` ; les six lignes
ci-dessus, relues directement depuis les bannières par point
d'observation de `gdb-stdout.log`, sont la version correcte.

### 2. La provenance de la mémoire que `sub_82372128` formate (capture de registre)

```
pre-call+147(sub_8236E868->indirect)   ctx.r3 = 0x80310   (la TAILLE demandée)
first-alloc-return(+150)               ctx.r3 = 0x10011c60 (le POINTEUR retourné)
call-sub_823721E8(+168)                ctx.r3 = 0x10011c60 ctx.r4 = 0x100b0000
```

`sub_8236E868` demande **`0x80310` octets (525 072), pas 784** — la
requête est correctement dimensionnée pour le pool de 64×8200+264+8
octets qu'elle va formater (`64*8200+264+8 = 0x80310`, exact). Le
pointeur retourné (`0x10011c60`) est passé tel quel, sans nouvelle
allocation, à `sub_823721E8 -> sub_82372198 -> sub_82372128`, qui
formate le pool à l'intérieur de ce bloc — un usage LÉGITIME de sa
propre mémoire.

**Correction à r398** : sa table donnait `+150 = 784` comme taille
demandée ; ce chiffre ne correspond à aucune capture (`0x80310 &
0xFFFF = 0x0310 = 784` — une troncature 16 bits de la vraie valeur,
probablement lue depuis le mauvais champ du C++ généré plutôt que
capturée en direct). La table de r398 est corrigée : `+150` demande
`0x80310`, `+225` reste `55944` (vérifié : la capture #3 filtrée montre
`r5=0xda88=55944` à sa place attendue dans la séquence).

### 3. Le nœud du bucket 17 tombe À L'INTÉRIEUR de ce même bloc

`0x10011c60 <= 0x10082aa0 < 0x10011c60+0x80320` — le nœud que
`sub_821F9E10` insère dans le bucket 17 est entièrement contenu dans le
bloc de 525 Ko déjà remis à `sub_8236E868`. L'en-tête de ce bloc
(`0x10011c50`, lu dans l'instantané `heap2_call1_size1.bin` déjà
capturé par r401) porte encore le champ de taille `0x8032` (= `0x80320`
octets, taille arrondie de la requête) à ce moment ultérieur — cohérent
avec un bloc correctement dimensionné et toujours marqué occupé,
**pas** la preuve de son état au moment de la découpe (voir "Non
établi").

### 4. Aucune libération de ce bloc avant la découpe (capture sur `sub_821FA6F8`, registre corrigé)

`sub_821FA6F8` (fonction `free`, r356) est appelée 47 fois avec des
pointeurs distincts sur l'intégralité du run précédant les événements
#2-#6 du point d'observation matériel. **Aucun des 47 pointeurs libérés
n'est `0x10011c60`** (ensemble complet capturé :
`0x0, 0x10000770, 0x10002470, 0x100024a0, 0x10002510, 0x10002530,
0x10002590, 0x10080010, 0x10080040, 0x100800b0` … jusqu'à `0x10080610`,
voir `r405_free_check_r5.log`) — le pointeur du pool n'a jamais été
libéré avant que `sub_821F9E10` n'insère le nœud `0x10082aa0` dans le
bucket 17. (Une première capture, filtrée par erreur sur `ctx.r4`, avait
donné ~280 appels tous à `ctx.r3=0x10000000` — c'est-à-dire l'objet tas
constant passé en premier argument, pas une liste de pointeurs libérés ;
ce chiffre est un artefact du mauvais registre et n'apparaît plus dans
la conclusion.)

## Ce que ceci établit pour r399

**Une invocation de `sub_821F9E10` (chaîne d'appel
`sub_821F7A88 <- sub_821F59E0 <- sub_821D74A8 <- sub_823B86D0 <- sub_823B8770
<- sub_823B81F0 <- sub_823ADBD8 <- sub_823ADF78 <- sub_823A5BA0`) insère
un nœud "libre" (`0x10082aa0`) dans le bucket 17 depuis l'intérieur d'un
bloc de 525 Ko qu'une invocation antérieure de la même fonction, dans
une chaîne d'appel totalement différente, avait déjà remis à
`sub_8236E868` — et ce bloc n'a jamais été libéré (47/47 pointeurs
libérés capturés, aucun n'est le pool).** Ce n'est pas un bug de codegen
(réfuté quatre fois maintenant, r401-r404), pas une taille mal demandée
par `sub_8236E868` (réfuté ici — la demande, `0x80310` octets, est
exacte), et pas un "chevauchement entre deux sous-systèmes indépendants"
comme l'hypothèse (a) de r380 le suggérait à l'origine — c'est une seule
et même fonction, l'allocateur général, qui distribue deux fois la même
mémoire dans deux invocations distinctes.

**Non établi ici, malgré la tentation de conclure** : laquelle des
branches internes de `sub_821F9E10` a fait l'insertion litigieuse. Deux
candidats restent non lus en entier (voir "Non établi" ci-dessous) — le
défaut n'est pas encore localisé à une ligne, seulement à la fonction et
au mécanisme (double-remise du même bloc).

## Non établi

- **La ligne exacte où le chemin de découpe considère cette mémoire
  comme disponible.** Deux candidats déjà partiellement lus, ni l'un ni
  l'autre en entier : `loc_821FA1D4` (repris du bloc pas encore lu au-delà
  de `bne cr6,loc_821FA2F4`) et `loc_821FA0BC` (le chemin grand-bloc de
  `sub_821F9E10`, jamais lu, atteint quand `r29>=128` — pertinent
  puisque la requête de `0x80310` octets tombe forcément dans ce
  chemin).
- L'état du champ de taille de l'en-tête (`0x10011c50`) AU MOMENT de la
  découpe (capturé seulement plus tard, hit ~#586) — ne prouve pas ce
  que l'allocateur croyait au moment précis de l'insertion litigieuse.
- Aucun correctif proposé ni appliqué.

## Fausses pistes écartées avant publication

1. **Version 1** : "le déchaînement à triple vérification de
   `sub_821F9E10` est lui-même le défaut, même famille que r356/r382."
   Réfutée par relecture des avertissements `Uninitialized memory read`
   du rejeu microexec de r402 (voir r403) : les champs retour du nœud
   sont NULS, la vérification échoue à raison. Devenue r403.
2. **Version 2** : "`sub_8236E868` demande 784 octets mais en formate
   525 072 — un facteur ~670, la faute est du côté de l'appelant."
   Réfutée par la capture de la Partie 2 ci-dessus : la requête réelle
   EST `0x80310`, exactement la bonne taille. `784` n'était qu'une
   troncature de lecture héritée de r398.

## Décisions prises

- Deux réécritures avant publication plutôt qu'une publication suivie
  d'une correction — chaque fois qu'une capture supplémentaire peu
  coûteuse (même harnais, techniques déjà établies) permettait de
  trancher en direct plutôt que de laisser une hypothèse non vérifiée
  entrer dans `NEXT.md`.
- Ne pas appliquer de correctif tant que la ligne précise du chemin de
  découpe n'est pas lue et vérifiée en direct — précédent r1111/r1113.

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF -> exit 0
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> exit 0
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> exit 0
```
`ctest` (relancé en entier ce cycle, pas seulement les trois audits) :
99% (87/88) passent. Le seul échec, `ac6-cpp-complexity`, est préexistant
et sans rapport avec ce cycle : `native_xenos_tests.cpp` est à 432 lignes
au dernier commit (`4bf6942f`) mais à 4241 lignes non committées dans
l'arbre de travail (`> 1000` lignes, budget de test) — un vestige de
l'arriéré non committé antérieur à r400 (Étape 0 du plan), jamais touché
par r401-r404. Ni r401 ni r402 ni r403 n'ont relancé `ctest` en entier
(chacun l'a noté « inchangé »), donc cet écart n'a jamais été surfacé
avant ce cycle. Nommé pour un cycle dédié plutôt que traité ici (scinder
le fichier de test ou ajuster le budget) : hors du fil r399, et éditer un
fichier de test à 4241 lignes sans lecture complète serait exactement le
type de correctif non vérifié que ce fil évite par ailleurs.

## Named for r405

Lire en entier `loc_821FA1D4` (suite, à partir de
`bne cr6,loc_821FA2F4`) et `loc_821FA0BC` (jamais lu, le chemin
grand-bloc atteint pour toute requête `r29>=128`, pertinent pour la
requête de `0x80310` octets elle-même) dans `sub_821F9E10`
(`ppc_recomp.27.cpp`), pour trouver la branche précise où une portion
de mémoire déjà attribuée au pool de `sub_8236E868` est traitée comme
disponible et réinsérée dans un bucket. Vérifier en direct (point
d'arrêt conditionnel sur cette branche précise) avant tout correctif.
Une fois confirmé et corrigé, revérifier le test ultime :
`sub_821D5F48` revient-il enfin, et la boucle par image s'exécute-t-elle ?

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/`
(`r404_watch.gdb/.log`, `gdb-stdout.log`, `r405_pool_base.gdb/.log`,
`r405_size_check.gdb/.log`, `r405_free_check.gdb/.log` (capture initiale,
filtrée par erreur sur `ctx.r4` — superseded par la suivante),
`r405_free_check_r5.gdb/.log` (capture corrigée, filtrée sur `ctx.r5`),
`gdb-stdout-r405*.log`, `gdb-stdout-r405-r5.log`).
