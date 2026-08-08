# Cycle 1131 — le consommateur de `PLAD` : un index de route, pas une position

Date : 2026-08-08. Cycle autonome. La suite immédiate du cycle 1130.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon` et dans le corpus canonique
  `exports/`. **Statique seul.** Aucun oracle.

## Comment on l'a trouvé sans la signature

Le cycle 1130 avait établi que le jeu ne compare jamais la signature `PLAD` :
l'entrée est adressée **par son index dans le FHM**. La prise était donc
l'accesseur d'enfant, `FUN_82234DD0(ressource+0x20, index)`, celui-là même que le
chargeur emploie avec l'index 0 pour le conteneur de scénario.

Un balayage des 522 appels de cet accesseur dans le corpus, avec l'index
constant qui les précède, donne un seul site pertinent dans le chargeur de
mission :

```
8219c814  li r4,0x2
8219c81c  bl 0x82234dd0     ; enfant 2 du FHM = PLAD
```

## Ce que `PLAD` est réellement

Deux accesseurs minuscules le disent :

```
82249ba8  stw r4,0x0(r3)     ; vue[0] = le bloc
          lwz r11,0x8(r4)    ; bloc+0x08 = le COMPTE
          blelr              ; compte nul : rien
          addi r11,r4,0x10   ; les enregistrements commencent en +0x10
          stw r11,0x4(r3)    ; vue[1] = le tableau

82249bc8  lwz r10,0x4(r3)    ; le tableau
          rlwinm r11,r4,0x4  ; index * 0x10
          add r3,r11,r10     ; &tableau[index]
```

**`PLAD` est un tableau compté d'enregistrements de `0x10` octets**, après un
en-tête de `0x10` octets dont le mot `+0x08` est le compte. La Mission 01 en
déclare **un** :

```
en-tête : 'PLAD'  0  1  1
record  : (-2025.0, 1500.0, 1345.0)  puis le mot 0x00000000
```

## Ce que le chargeur en lit — et ce qu'il n'en lit pas

```
8219c83c  lwz r4,0x4b40(r11)   ; l'index : global+0x4B40, l'emplacement de joueur
8219c840  bl 0x82249bc8        ; &record[cet index]
8219c850  lwz r11,0xc(r3)      ; le QUATRIÈME MOT du record
8219c860  stw r11,0xf0(r10)    ; -> (contexte+0x12B844) + 0xF0
```

Et `0x12B844 − 0x12B440 = 0x404`. Le gestionnaire d'unités n°1 commence en
`contexte+0x12B440` ; son `+0x404` est **le premier pointeur prédicat**, celui
que `0x8226FEC0` remplit avec l'objet dont la virtuelle `+4` répond oui
(cycle 1096). Autrement dit : l'unité que le gestionnaire a élue.

Et `+0xF0` est, depuis le cycle 1127, **le curseur dans la liste `Obj` — la
route**.

Donc, en une phrase :

> `PLAD` dit, pour chaque emplacement de joueur, **sur quelle entrée de route
> son unité commence**.

Les trois autres appels de ces accesseurs — `0x82097560`, `0x8219F8C0` — sont les
deux autres variantes du chargeur et font exactement la même chose. **Ce sont les
trois seuls consommateurs du tableau dans le binaire.**

## Le fait qui dérange

Aucun des trois ne lit les **trois flottants**. Le chargeur prend le quatrième
mot et rien d'autre ; la vue est une structure de pile qui meurt avec la
fonction, seul `[record+0x0C]` en sort.

**La position `(-2025, 1500, 1345)` est présente dans le fichier de la mission et
n'est lue par personne sur le chemin de chargement.** C'est la sixième fois dans
cette série qu'un champ qui ressemble à une position se révèle non lu là où on
l'attendait, et il faut le dire sans l'habiller.

## Ce que cela ne dit pas

- **Que les trois flottants ne servent à rien.** Ils ne sont pas lus *par le
  chargeur* ; le tableau reste accessible par le système de ressources, et un
  autre consommateur n'est pas exclu — il n'a simplement pas été trouvé par les
  appels de ces deux accesseurs, qui sont les seuls.
- **Que le curseur de route donne une position.** Le cycle 1127 a mesuré les
  blocs `Param` des 434 entrées de route : mode 0 partout, `x` de 0 à 60 000,
  **`y` nul partout** et `z` quasi nul. Une entrée de route ne porte donc pas
  davantage une position utilisable sur cette charge utile.
- **Quel emplacement de joueur** `global+0x4B40` désigne, ni comment il est
  choisi.

## Décision de cycle

Rien n'est porté. Le produit natif n'a ni tableau `PLAD` ni curseur de route, et
lui donner un index de route qu'il n'a pas ne ferait que déplacer la fiction.
Ce qui est gagné est une **chaîne complète et vérifiable** — fichier, accesseurs,
index, champ, destination — et deux faits négatifs précis.

`ctest 24/24`, la porte JF reste verte.
