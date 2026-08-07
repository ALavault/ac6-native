# Cycle 1101 — qui écrit les 339 compteurs de mission

Date : 2026-08-08. La question laissée ouverte au cycle 1100, et la fermeture
de la boucle d'objectifs.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique seul.**

## La recherche

Le lecteur du cycle 1097 calcule l'adresse d'un compteur ainsi :

```
8226e830  lhz  r11,0x0(r7)      ; l'identifiant, u16
8226e844  rlwinm r9,r11,0x2     ; id*4
8226e848  lwz  r10,0x5c(r31)    ; la table, contexte+0x5C
8226e84c  add  r11,r11,r9       ; id*5
8226e850  rlwinm r11,r11,0x2    ; id*20
8226e854  add  r11,r11,r10      ; &compteur[id]
```

La multiplication par 20 n'est pas un `mulli` : c'est un décalage, une addition
et un décalage. `scripts/FindStride20Uses.java` reconnaît les deux formes et
croise avec les fonctions qui chargent `0x5C` : **54 fonctions font une
multiplication par 20, 16 d'entre elles chargent aussi `0x5C`.**

## L'écrivain, unique

`0x82267468(contexte, id, opérande)`. Il calcule `&compteur[id]`, lit l'octet
`opérande+0x08` et saute dans une table de quatre entrées, vérifiée à
`0x822674C4` :

| code | adresse | opération |
| ---: | --- | --- |
| 0 | `0x822674D4` | `valeur = (s16)opérande+0x00` — affectation d'un littéral |
| 1 | `0x822674E0` | `valeur += (s16)opérande+0x00` — ajout d'un littéral |
| 2 | `0x822674F0` | `valeur = 1 + alea % littéral`, `alea` venant de `0x82380798` |
| 3 | `0x82267510` | `valeur = compteur[opérande+0x04] + compteur[opérande+0x06]` |

Les deux identifiants du code 3 valent `0xFFFF` pour « absent », et la mise à
jour est alors sautée. Un code supérieur à 3 n'écrit rien.

Puis, **quelle que soit la branche prise** :

```c
if (compteur[id].valeur == 1 && compteur[id].instant == FLT_MAX)
    compteur[id].instant = *(float *)(contexte + 0x64);   // l'horloge de mission
```

`FLT_MAX` (`0x7F7FFFFF`, en `0x82008120`) est la sentinelle « jamais » du
projet : la même constante sert de valeur par défaut au tableau d'horodatage
des sous-missions (`0x82267078`). L'instant n'est donc posé **qu'une fois**, et
seulement quand la valeur vaut exactement 1.

## La disposition d'une entrée

L'entrée fait `0x14` octets ; trois champs sont établis :

| offset | contenu |
| --- | --- |
| `+0x00` | `s32`, la valeur que compare la condition d'étiquette 7 |
| `+0x04` | `float`, l'instant du premier passage à 1, sinon `FLT_MAX` |
| `+0x08` | un mot de bits, `\|= 1 << k` en `0x8226CF4C` |

Les huit octets restants ne sont pas caractérisés.

## Les quatre appelants, et celui qui compte

| site | appelant | ce qu'il écrit |
| --- | --- | --- |
| `0x82296BFC` | `0x822969F8`, **cas 6** du répartiteur d'ordres | l'écriture de scénario |
| `0x8226928C` | `0x82269188` | met le compteur **10** à 1 quand huit autres passent des seuils |
| `0x822887F4` | `0x82288730` | une écriture unique, gardée par un bit « déjà fait » |
| `0x8226CF2C` | `0x8226C388` | suivie d'un `\|= 1 << k` sur le champ `+0x08` |

`0x822969F8` récupère l'enregistrement d'ordre par un appel virtuel `+0x10` et
commute sur son premier octet. Les indices de mots qu'il déréférence —
`puVar9[3]` au cas 2, `puVar9[4]` au cas 3, `puVar9[6]` au cas 5, `puVar9[7]`
au cas 6 — **sont exactement la table `tag → mot` que le parseur `OrderBin`
écrit** (cycle 1091). Le cas 6 est donc `OrderFlagBin`, nommé par sa propre
chaîne d'erreur `0x82010350`.

Sa charge utile est lue ainsi :

```c
puVar5 = *(u16 **)puVar9[7];
opérande.littéral  = puVar5[1];              // +0x02
opérande.opération = classement de *(u8 *)(puVar5 + 2);   // +0x04 -> 0, 1 ou 2
Function_82267468(contexte_de_mission, *puVar5 /* +0x00 */, &opérande);
```

**`OrderFlagBin` est donc la seule écriture de compteur que le scénario porte.**
Le nom, tiré de la chaîne d'erreur du parseur au cycle 1083, se révèle exact :
c'est bien un ordre qui lève un drapeau.

## Mesure sur la Mission 01

En parcourant les programmes `Set → Act → Order` des 230 unités :

```
ordres, par étiquette   0:233  1:206  2:890  3:442  5:686  6:232  7:53  8:233
ordres OrderFlag                                    232
identifiants distincts                              133
domaine des identifiants                         45 .. 332      (table : 339)
opérations                        0 (affectation) 231,  1 (ajout) 1
littéraux                         1 ×136,  2 ×57,  0 ×36,  99 ×2,  3 ×1
```

**Aucun identifiant ne sort de la table** que le chargeur dimensionne depuis le
compte `u16` du slot 1. C'est la même forme de preuve que le cycle 1096 pour
les octets de classe et de faction : le domaine des données tombe exactement
dans le domaine que le code implémente.

Confirmation indépendante : `0x82269188` lit huit compteurs par déplacement
immédiat — `0x118`, `0x2A8`, `0x370`, `0xE4C`, `0x5B4`, `0x7BC`, `0xA28`,
`0x974`. **Tous sont des multiples exacts de 20**, soit les compteurs 14, 34,
44, 183, 73, 99, 130 et 121, tous sous 339.

## La boucle, et ce qui lui manque

```
unité → programme Set/Act/Order → OrderFlagBin → compteur[id]
                                                      ↓
sous-mission ← saut ← condition d'étiquette 7 ← compteur[id]
```

Elle est complète **en tant que mécanisme**. Elle n'est pas exercée dans la
Mission 01 : celle-ci écrit 232 drapeaux mais **ne contient aucun pas
d'étiquette 7** (cycle 1097). Ses compteurs sont donc écrits et lus ailleurs —
par `0x822670D8`, `0x822671C8`, `0x82269188` et les autres comparateurs de la
liste des 16 — pas par son propre script de sous-mission.

Le dire autrement : le mécanisme d'objectif conditionnel existe, la Mission 01
ne s'en sert pas pour enchaîner ses sous-missions.

## Portage natif

`retail_mission_state` porte l'écrivain : les quatre opérations, la sentinelle
`0xFFFF`, l'horodatage à la première valeur 1, et l'ordre exact — retail
calcule, écrit, **puis** teste l'horodatage, y compris quand rien n'a été
écrit. Deux divergences déclarées : un littéral nul est refusé au lieu de
diviser par zéro, et les identifiants du code 3 sont bornés.

`retail_scenario` expose les ordres `OrderFlagBin` du scénario. Le test rejoue
les **232** ordres de la Mission 01 sur une table de 339 entrées et exige que
chacun atterrisse.

## Ce que cela n'établit pas

- **Ce que chaque compteur signifie.** On sait qui l'écrit, avec quelle valeur
  et depuis quel ordre de quelle unité ; on ne sait pas ce que « compteur 97 »
  désigne. Les joindre aux noms demanderait autre chose que ce cycle.
- Le champ `+0x08` de l'entrée, et les huit octets de queue.
- Qui initialise le tableau à `FLT_MAX` : l'allocation du chargeur ne le fait
  pas, et le réinitialisateur n'a pas été trouvé.
- Les autres lecteurs de la liste des 16 ne sont pas classés un par un.
