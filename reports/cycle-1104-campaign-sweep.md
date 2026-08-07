# Cycle 1104 — les quinze missions de la campagne

Date : 2026-08-09. Le cycle 1103 avait passé une mission de contrôle. Celui-ci
passe toute la campagne.

## Qualification

- Table de niveaux `DAT_82065840` = `[0x33, 9, 10, 11, … 0x17]`. Les indices 1 à
  15 donnent les entrées DPL **9 à 23** ; l'indice 0 donne l'entrée 51, dont
  l'enfant 0 est **vide** — ce n'est pas un nœud de scénario, et elle est
  écartée sans autre conclusion.
- Quinze charges utiles, enfant 0 de chaque FHM, magie `00 00 00 10`, de
  1 238 016 à 5 056 032 octets. Extraites localement, **aucune versionnée**.
- **Statique seul.** Aucun oracle.

Régénération :

```sh
python3 tools/extract_ac6_pac.py game-files --indices 9 10 11 … 23 \
    --output DIR --decompress
# puis l'enfant 0 de chaque DIR/payloads/00NN.decompressed.bin
python3 tools/roundtrip_ac6_scenario.py SCENARIO
reconstruction/ace-combat-6/build-core/ac6-retail-scenario-probe SCENARIO
```

## Aller-retour : 15 sur 15

Chaque scénario est reconstruit octet pour octet, structure recalculée depuis le
modèle. Jusqu'à **92 051 nœuds** pour l'entrée 21.

Et la même signature partout, sans exception : **zéro octet non nul non
réclamé**, plage de bourrage la plus longue **12 octets**. Sur quinze fichiers
qui n'ont pas servi à écrire la règle, c'est la primitive de conteneur du cycle
1084 qui tient.

## Le lecteur natif : 9 767 exécutions, aucun échec

| entrée | unités | `Obj` | factions | sous-miss. | drapeaux | compteur max / capacité |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 9 (M01) | 230 | 434 | 4 | 4 | 232 | 332 / 339 |
| 10 | 127 | 211 | 2 | 1 | 11 | 97 / 104 |
| 11 | 98 | 262 | 6 | 2 | 4 | 45 / 168 |
| 12 | 179 | 418 | 6 | 2 | 13 | 84 / 219 |
| 13 | 254 | 705 | 6 | 2 | 198 | 315 / 423 |
| 14 | 214 | 621 | 9 | 2 | 10 | 269 / 335 |
| 15 | 190 | 350 | 7 | 6 | 246 | 262 / 297 |
| 16 | 247 | 634 | 6 | 2 | 257 | 320 / 326 |
| 17 | 105 | 248 | 2 | 4 | 16 | 144 / 176 |
| 18 | 191 | 521 | 6 | 2 | 6 | 104 / 213 |
| 19 | 246 | 572 | 7 | 4 | 128 | 207 / 403 |
| 20 | 121 | 189 | 5 | 4 | 10 | 130 / 234 |
| 21 | 253 | 892 | 9 | 5 | 213 | 468 / 471 |
| 22 | 251 | 390 | 4 | 3 | 2 135 | 273 / 274 |
| 23 | 247 | 337 | 5 | 8 | 171 | 189 / 190 |
| **total** | **2 953** | **6 784** | | | **3 650** | |

**9 767 exécutions de lecteur, zéro échec.** Les dix classes sont atteintes :
`SetBin` et `SubMisTblBin` en tête, `ActBin`, `OrderBin`, `SubMisBin` et la
liste `0x28` par descente, `ObjBin` et `ManeuverBin` par le sous-arbre d'unité,
`ComTblBin` et `ComBin` sous les manœuvres.

## La borne des compteurs est serrée, pas lâche

C'est le résultat le plus probant du cycle. Pour chaque mission, le plus grand
identifiant de compteur qu'un `OrderFlagBin` nomme, comparé au compte `u16` du
slot 1 dont le chargeur dimensionne la table :

```
marge (capacité − max) :  1, 1, 3, 6, 7, 7, 32, 35, 66, 104, 108, 109, 123, 135, 196
```

**Deux missions n'ont qu'un seul emplacement de marge**, deux autres trois et
six. Une foulée fausse, un mot de compte mal lu ou un champ d'identifiant mal
placé casserait la borne sur au moins l'une d'elles. Elle tient sur les quinze.

## Trois invariants que la campagne révèle

Ce que quinze missions montrent et qu'une seule ne pouvait pas :

1. **Un enregistrement de classe 0 et un seul, par mission. Un de classe 4 et un
   seul, par mission.** Quinze fois sur quinze. La classe 0 rend la catégorie 1,
   la classe 4 la catégorie 3.
2. **La classe 3 n'apparaît jamais.** Le `switch` de `0x820A7F48` l'implémente,
   la campagne ne l'emploie pas. Seules `{0, 1, 2, 4}` sont exercées.
3. **Le slot 4 est vide dans les quinze.** Le test de vacuité que le code fait
   dessus est donc justifié à l'échelle de la campagne — contrairement au slot 6
   (présent 13 fois sur 15) et au slot 8 (présent **5** fois sur 15), que le
   cycle 1083 avait rangés dans la même phrase.

| slot | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| présent sur 15 | 15 | 15 | 15 | 15 | **0** | 15 | 13 | 15 | **5** | 15 |

Le slot 8 est celui que consomme le troisième appel de `0x820A7070` : **dix
missions sur quinze n'ont pas de troisième passe d'unités.**

## Les deux descentes non modélisées : jamais exercées

Sur **9 767 exécutions couvrant la campagne entière**, ni l'étiquette 2
d'`OrderBin` (`0x82331D98`) ni celle de la liste `0x28` (`0x823308E0`) n'ont
satisfait leur précondition de descente. Les étiquettes 2 sont pourtant
fréquentes ; c'est la précondition — un mot de données résolu, une table, un
premier enfant présent — qui ne se réunit jamais.

Ce n'est pas la preuve qu'aucun code retail ne descend jamais : c'est la preuve
que **la campagne solo ne le fait pas**. Le multijoueur, les missions bonus et
les entrées hors table restent hors de portée de ce balayage.

## Ce que cela n'établit pas

- **Rien de neuf sur le sens.** Les mêmes champs, quinze fois, restent des
  champs dont on connaît le consommateur.
- Aucune micro-exécution p-code hors Mission 01 : les 138 digests restent
  attachés à elle. Ailleurs, le lecteur a tourné sans échouer — affirmation plus
  faible qu'un accord octet pour octet.
- Ce que contient le slot 6, ni pourquoi dix missions se passent du slot 8.
- L'entrée 51, dont l'enfant 0 est vide, n'est pas expliquée.
