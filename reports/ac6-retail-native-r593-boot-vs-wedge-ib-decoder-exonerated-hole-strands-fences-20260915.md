# r593 — Boot IB vs IB coinçant : le décodeur est innocenté, l'IB MissionTitle a un TROU qui isole les fence-writes

## Qualification

- Comparaison hors-ligne : 5 IBs de boot (dump `vd ib boot#`, hook r593 ajouté
  dans `expand_indirect`) vs l'IB coinçant `0x12943500` (dump `vd ib full`).
  Run court boot-only + le run VD_TRACE r591. Décodeur PM4 = `native_xenos.cpp`.
- Oracle : non.

## Comparaison décisive

| IB | count | décode | zero-run | fin |
|----|-------|--------|----------|-----|
| boot#1 | 11 | **propre** | 0 | exact (11) |
| boot#2 | 64 | **propre** | 0 | exact (64) |
| boot#3 | 74 | **propre** | 0 | exact (74) |
| boot#4 | 13 | **propre** | 0 | exact (13) |
| boot#5 | 48 | **propre** | 0 | exact (48) |
| **wedge 0x12943500** | 254 | **tronqué @237** | **30** | mort @237 |

Les 5 IBs de boot décodent proprement, contigus, **exactement jusqu'à count**,
**aucun trou**. L'IB MissionTitle est le seul avec un long run de zéros +
données parasites au milieu. **Le décodeur est donc correct** — le problème est
le contenu de l'IB.

## Structure exacte de l'IB coinçant (décodée + resync vérifié)

- **offsets 0–176 : commandes réelles alignées** (le décode tombe proprement sur
  un TYPE2 NOP à 174 ⇒ alignement tenu). Contient des event-writes `16530xxx`.
- **offsets 177–243 : TROU** — zéros + parasites (`135c0000` @237, `00001844`,
  `00007fff`…). Pas du PM4 séquentiel valide ; le décodeur mésaligne et tronque
  sur `135c0000` @237.
- **offsets 244–253 : fence-writes VALIDES** (resync @244/@246 décode propre) :
  ```
  244: c0003b00 (T3 op0x3b)
  246: c0025800 00000003 16520006 129438cd   EVENT_WRITE -> fence 0x16520006
  250: c0025800 00000003 16520002 0000009f   EVENT_WRITE -> fence 0x16520002
  ```
  **Ce sont exactement les fences que le frame gate du game loop attend**
  (0x16520000). Elles sont présentes et valides, mais **inatteignables** : le
  décodeur meurt dans le trou avant de les atteindre.

## Conséquence

Le gel MissionTitle n'est **pas** un bug du décodeur PM4 (les IBs de boot le
prouvent). C'est que l'IB de MissionTitle est traité avec un **trou au milieu**
(177–243) qui isole les fence-writes (244–253) ; le décodeur rejette tout l'IB,
le curseur ring n'avance pas, les fences ne sont jamais écrites, le game loop
bloque.

## Non établi (dit clairement) — la vraie question restante

**Pourquoi le trou 177–243 ?** Les fences en 244–253 étant présentes, une
écriture séquentielle de l'IB aurait aussi rempli 177–243 → contradiction. Trois
lectures :
- (a) **IB déchiré / WPTR prématuré** : le guest publie l'IB avant d'avoir écrit
  le milieu, puis se bloque sur le fence (dans la queue non exécutée) → interblocage
  avec un IB stablement incomplet (le garde r506 « torn-snapshot » ne sauve pas
  un contenu stable-mais-incomplet). C'est le territoire r503–r506.
- (b) **cohérence mémoire** : le guest a écrit 177–243 mais la vue
  `guest_memory_` native est périmée pour cette zone.
- (c) le milieu est en fait la **charge utile** d'un paquet dont l'en-tête (avant
  177) a un count mésestimé — écarté ici : le décode 0–176 est aligné (NOP @174),
  aucun paquet avant 177 n'a un count couvrant le trou.

## Prochaine barrière (précise)

Déterminer entre (a) et (b) : instrumenter le point de publication WPTR / la
complétude de l'IB au moment du drain (le milieu 177–243 est-il écrit APRÈS que
le drain le lit ?), ou comparer deux lectures espacées de l'IB (le milieu se
remplit-il ?). Si (a), le fix est côté synchro ring/IB (attendre la complétude
de l'IB avant de drainer, ou un fence de complétion guest→driver) — territoire
r503–r506. Le décodeur PM4 n'est PAS à corriger.

## Décisions prises

1. **Innocenter le décodeur PM4** par comparaison contrôlée (5 IBs de boot
   propres) — pivot majeur : ne pas « corriger le décodeur ».
2. **Épingler la structure** de l'IB coinçant (trou 177–243, fences valides
   244–253 inatteignables) par décode + resync vérifié.
3. **Ne pas conclure la cause du trou** (WPTR prématuré vs cohérence mémoire) ;
   nommée comme prochaine mesure, pas devinée.
