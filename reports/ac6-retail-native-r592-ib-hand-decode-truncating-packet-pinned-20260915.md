# r592 — Décodage complet de l'IB qui coince : le paquet tronquant est l'offset 237 (0x135c0000, TYPE0 count 4957) dans une zone morte, et les fence-writes attendus sont DERRIÈRE

## Qualification

- Décodage hors-ligne du dump `vd ib full` (r510) capturé dans le run VD_TRACE
  r591 (`ac6-r593-vdtrace.log`). Règles du décodeur PM4 lues dans
  `native_xenos.cpp` (`type_of`, `count_of`, longueurs type0/1/2/3).
- Oracle : non. Aucun run ce cycle (le dump complet existait déjà).

## L'IB décodé mot par mot (0x12943500, 254 mots)

Règles : type0/3 → `1+count`, count=`((h>>16)&0x3FFF)+1` ; type1 → 3 ;
type2 (NOP) → 1.

- **offsets 0–176 : commandes réelles valides** — TYPE3 opcodes 0x3c/0x60/0x54/
  0x3b/0x2b/0x36/0x46, écritures registre TYPE0 (counts petits), un TYPE2 NOP à
  174. Contient des event-writes `16530006`/`16530002` aux offsets 17,30,40,48.
- **offsets 177–235 : 59 mots de `0x00000000`** — décodés chacun comme TYPE0
  count 1 (2 mots), donc l'alignement dérive.
- **offset 237 : `0x135c0000`** → TYPE0 count `(0x135c)+1 = 4957` →
  `require_words(4958)`, 17 mots restants → **TRONQUÉ ICI**. C'est une donnée
  parasite (adresse/masque : voisins 236=`00001844`, 239=`00001841`,
  240=`fffff8ff`), pas un vrai en-tête.
- **offsets 246–253 : commandes de fence réelles** — `16520006` (248),
  `16520002` (252) : **exactement les fences que le frame gate du game loop
  attend** (0x16520000). Elles sont **APRÈS** le point de troncature.

## Conséquence (corrige l'hypothèse « préfixe à committer »)

Le décodeur rejette **tout** l'IB sur la troncature à l'offset 237. Comme les
fence-writes critiques (`16520006`/`16520002`) sont **au-delà** (248/252),
committer seulement le préfixe valide (0–176) **ne suffit pas** : il faut
franchir la zone morte (177–237) pour atteindre 246–253. La dérive
d'alignement vient des 59 mots nuls : depuis 177, en consommant 2 mots par
« paquet » TYPE0-count1, le curseur tombe sur 237 (`0x135c0000`) au lieu de 238
(`c0022100`, un vrai TYPE3 opcode 0x21) — décalé d'un mot.

## Ce qui est établi / non établi

- **Établi** : le paquet tronquant est l'offset 237 (`0x135c0000`, TYPE0 count
  4957) ; il est dans une zone nulle+parasite (177–~245) entre deux sections de
  commandes réelles ; les fences attendues par le game loop sont derrière
  (248/252) ; le rejet global de l'IB les prive d'exécution → gel.
- **Non établi** : POURQUOI la zone 177–245 est nulle+parasite. Trois lectures :
  (a) mémoire IB **périmée/non initialisée** (le guest a écrit 0–176 et 246–253
  mais pas 177–245 — mais 1,48 M échecs stables excluent une simple course
  d'écriture) ; (b) `count`=254 **trop grand** / mauvaise taille d'IB ; (c) IB
  à **deux sections** dont le milieu n'est pas du PM4 séquentiel. Trancher
  demande de comparer à un IB de boot qui décode proprement (même structure ?)
  et/ou d'analyser le générateur de commandes guest.

## Prochaine barrière (précise)

Comparer cet IB à un IB de boot **réussi** (même dump `vd ib full` mais sur un
drain OK — il faut lever la garde « once per process » ou logguer un IB de boot)
pour voir si les IBs de boot ont aussi une zone nulle en milieu et comment le
décodeur la franchit alors. Cela dira si le fix est : (a) aligner/sauter la zone
nulle, (b) corriger la taille d'IB lue, ou (c) tolérer une troncature en zone
morte en cherchant la reprise de PM4 valide (les fences en 246+). Le fix vit
dans `Pm4Decoder`/`expand_indirect` (`native_xenos.cpp`), validé par un run où
`updates` dépasse MissionTitle.

## Décisions prises

1. **Épingler le paquet tronquant par décodage exhaustif** (offset 237,
   `0x135c0000`), et non plus par extrapolation head/tail.
2. **Corriger l'hypothèse « committer le préfixe »** : les fences nécessaires
   sont derrière la zone morte, donc un simple commit de préfixe ne débloque pas.
3. Aucun code de fix ; la cause de la zone morte reste à établir (nommée), pas
   devinée.
