# Cycle 1103 — la Mission 02 comme jeu de contrôle

Date : 2026-08-09. Tout ce qui précède a été construit sur la Mission 01. Ce
cycle pointe le même code sur une mission qui n'a rien construit.

## Qualification

- Niveau 2 → entrée DPL **10**, lu dans `DAT_82065840` :
  `[0x33, 9, 10, 11, … 0x17]`. L'entrée 9 était la Mission 01 ; l'indice 2 donne
  10, sans interpolation.
- Charge utile : entrée 10, enfant 0 du FHM (26 enfants), **1 338 944 octets**,
  SHA-256 `b9d380df500b5dc57b71b49287cb31c724a89a5653104b8c5e1885ba4d5dc3ea`.
  Extraite localement, **non versionnée**, hors du dépôt.
- **Statique seul.** Aucun oracle.

Régénération :

```sh
python3 tools/extract_ac6_pac.py game-files --indices 10 --output DIR --decompress
# puis l'enfant 0 du FHM DIR/payloads/0010.decompressed.bin
```

## Aller-retour, du premier coup

```
identique              oui      1 338 944 octets, même SHA-256
nœuds / tables / données   23 881 / 10 841 / 22 434
octets de structure   329 932
octets non réclamés   266 356   dont non nuls : 0
                                34 131 plages, la plus longue de 12 octets
```

La primitive de conteneur, la règle de présence, le signe du compte et la
signature de bourrage — **au plus 12 octets, toujours nuls** — se retrouvent
identiques sur des données qui n'ont servi à rien établir.

## Le lecteur natif, sans modification

`ac6-retail-scenario-probe` fait tourner le même code que les tests, sur la
charge utile non versionnée :

| | Mission 01 | Mission 02 |
| --- | ---: | ---: |
| slots racine présents | `1111010111` | `1111011101` |
| enregistrements d'unités | 230 | **127** |
| sous-enregistrements `Obj` | 434 | **211** |
| factions | 4 | **2** |
| sous-missions | 4 | **1** |
| octets de classe | `{0:1, 1:40, 2:188, 4:1}` | `{0:1, 1:73, 2:52, 4:1}` |
| octets de faction | `{0:140, 1:42, 2:48}` | `{0:127}` |
| ordres `OrderFlag` | 232 | **11** |
| compteurs distincts | 133 | **7** |
| unités construites | 230 | **127** |
| exécutions de lecteur | 666 | **340** |
| échec de lecteur | aucun | **aucun** |

Ce qui tient sans retouche :

- les **dix slots** de la racine ;
- l'indirection de niveau 0 — 127 emballages sur 127 portent **exactement un**
  enfant ;
- le domaine des octets de classe : `{0, 1, 2, 4}`, tous dans le `switch` que
  `0x820A7F48` implémente, aucun `3` ici non plus ;
- l'octet de faction indexe la table du slot 5 : une seule faction utilisée sur
  les deux déclarées ;
- les identifiants de compteur des ordres `OrderFlag` : **1 à 97, tous sous les
  104** que le compte `u16` du slot 1 déclare — la même borne que le cycle 1101
  a établie sur 339, retrouvée sur une autre valeur ;
- la classification, l'insertion et le recensement par faction : 127 unités
  construites sans qu'aucune ne tombe hors domaine.

## Ce que la Mission 02 corrige

Le cycle 1083 disait : « slots 4 et 6 vides, exactement là où le code teste la
vacuité ». **C'était une propriété de la Mission 01, pas de la famille.**

| slot | Mission 01 | Mission 02 |
| ---: | --- | --- |
| 4 | absent | absent |
| 6 | absent | **présent** |
| 8 | présent | **absent** |

Le slot 8 est celui que le troisième appel de `0x820A7070` consomme (cycle
1096) : la Mission 02 n'a donc pas de troisième passe d'unités. Et le slot 6,
jamais vu rempli, l'est ici — son contenu n'est pas caractérisé.

## Les deux descentes non modélisées, toujours non exercées

Le port refuse de deviner deux descentes : l'étiquette 2 d'`OrderBin`
(`0x82331D98`) et l'étiquette 2 de la liste `0x28` (`0x823308E0`). Il échoue
bruyamment si un nœud satisfait leur précondition.

Sur 340 exécutions de lecteur couvrant les 127 programmes `Set → Act → Order`,
les 211 `Obj`, la table de sous-missions et la table radio : **aucun
déclenchement**. La Mission 02 compte pourtant 133 ordres d'étiquette 2 — c'est
donc bien la *précondition de descente*, et non l'étiquette, qui reste
inexercée.

C'est un résultat négatif, et il vaut ce qu'il vaut : deux missions ne sont pas
la campagne. Ce qui est acquis, c'est que la garde n'a pas menti sur des données
neuves.

## Ce que cela n'établit pas

- **Rien de sémantique de plus.** Les mêmes champs, sur une autre mission,
  restent des champs dont on connaît le consommateur et pas le sens.
- Le contenu du slot 6, inconnu.
- Aucune micro-exécution p-code n'a été faite sur la Mission 02 : les 138
  digests restent une propriété de la Mission 01. Le lecteur natif y a tourné
  sans échouer, ce qui est plus faible qu'un accord octet pour octet.
- Les manifestes ne sont pas produits pour la Mission 02 : le contrat de porte
  ne couvre que la Mission 01, et rien ne demande de l'élargir aujourd'hui.
