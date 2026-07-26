# AC6 — producteurs des records transmis au lecteur NDXR (cycle 173)

Date : 2026-07-18 (Europe/Paris)

## Cible et méthode

Cible canonique AC6 Xbox 360 PAL : `default.xex`, target ID
`ac6-xbox360-pal`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, base
`0x82000000`, projet Ghidra `ace-combat-6`.

Cette passe est une inspection statique et headless du corps XenonRecomp
`0x82102e70..0x821033a7`. Le résultat est comparé au dump Ghidra headless
avec `FindDirectCallsTo.java` et `ReferencesTo.java`. Aucun code généré ni
source natif n'a été modifié.

## Appels du lecteur et du post-helper

Le parent `0x82102e70` conserve les entrées suivantes dans sa frame :

```text
r3 -> r19 : receiver/contexte opaque
r4 -> r25 : sortie vectorielle externe
r5 -> r21 : sortie vectorielle externe
r6 -> r23 : sortie scalaire externe
r7 -> r30 : pointeur vers record_4word_A
r8 -> r29 : pointeur vers record_4word_B
r9 -> r22 : octet/paramètre conservé
```

Deux séquences utilisent le même contrat de sorties et le même couple
`r19`, `r4`, `r5`, `r6` :

```text
site                 lecteur 0x82102148       post-helper 0x82102568
0x82103010           LR 0x82103014            LR 0x8210306c
0x82103208           LR 0x8210322c            LR 0x821032a8
```

Dans la première voie, le lecteur/post-helper reçoivent `r7=r30` et
`r8=r29`, donc les deux records d'entrée. Dans la seconde, ils reçoivent les
deux vecteurs scratch construits localement (`r1+160` et `r1+112`).

Le balayage headless ne trouve aucun `bl` direct vers `0x82102e70` ni vers
`0x82102568`. Il confirme en revanche la référence de donnée
`0x8205cae4 -> 0x82102e70`; ces fonctions sont donc atteintes par une
combinaison de dispatch/table et d'appels internes, pas par une nouvelle
chaîne d'appel directe à promouvoir.

## Production des indices quantifiés

Le parent lit d'abord les premiers et troisièmes mots des deux records :

```text
record_4word_A : float32 +0, +8
record_4word_B : float32 +0, +8
```

Il ajoute une constante, applique un facteur flottant, convertit vers des
entiers avec `fctiwz`, puis les conserve dans la frame (`+96`, `+100`, `+104`,
`+112`). Chaque résultat est ramené d'un cran lorsque la valeur flottante
correspondante est négative. Les valeurs effectivement transmises au lecteur
et au post-helper sont ensuite :

```text
r31, r17, r27, r16 : indices signés après clamp
r9,  r10            : deux indices sélectionnés
```

Le code calcule aussi un workspace de 2048 octets indexé par les indices
décalés de quatre bits. Cette zone est remise à zéro lorsque le couple de
records change de cellule d'index. Il s'agit d'un mécanisme de cache ou de
marquage local confirmé par le contrôle de flux, mais son contenu métier reste
`unknown`.

## Production des deux vecteurs scratch

Dans la voie où les indices ne sont pas égaux, le parent recharge les quatre
mots de chaque record et les assemble sans transformation supplémentaire :

```text
scratch_vector_left  = { A[0], A[1], A[2], A[3] }  à r1+160
scratch_vector_right = { B[0], B[1], B[2], B[3] }  à r1+112
```

Les stores observés sont aux offsets `0`, `4`, `8` et `12` de chaque zone.
Ces vecteurs deviennent respectivement `r7` et `r8` lors du second appel au
lecteur puis du second appel au post-helper. Le parent construit donc bien les
inputs `40/8` du cycle 172 à partir de records entrants de 16 octets et de
zones locales ; aucun nouveau format de fichier ou type gameplay n'est établi.

La voie des indices égaux transmet directement `r30`/`r29`, tandis que la voie
non égale transmet leurs copies scratch. Dans les deux cas, le lecteur écrit
les sorties vectorielles en `r1+128` et `r1+144`, et le scalaire en `r1+96`.
Sur retour positif, le parent recopie ces valeurs vers `r25`, `r21` et `r23`.

## Confiance et limites

- `confirmed` : les deux call-sites du lecteur et du post-helper, avec leurs
  registres d'arguments ;
- `confirmed` : lecture de quatre mots à `r7/r8` et assemblage des deux zones
  scratch de 16 octets ;
- `confirmed` : quantification flottante, clamp négatif, indices transmis et
  remise à zéro de 2048 octets ;
- `cross-match` : relation de table `0x8205cae4 -> 0x82102e70` ;
- `unknown` : type C++, unités, sens du score, identité métier des records,
  origine ultime des pointeurs entrants et rôle de la zone de 2048 octets.

Ne pas nommer ces valeurs `position`, `cellule`, `avion`, `sommet` ou
`coordonnée` sans preuve dynamique ou documentaire. Le contrat actuel est un
contrat ABI/différentiel sur deux records 4-mots et quatre indices quantifiés.

Aucune action humaine, VNC, run Xenia ou intervention sur la GUI n'est
nécessaire pour cette passe. La prochaine étape utile est de suivre la
provenance des pointeurs entrants `r7/r8` depuis le dispatch appelant, sans
modifier les sorties générées.
