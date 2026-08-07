# Cycle 1105 — ce que contient le slot 6

Date : 2026-08-09. La question laissée ouverte par le balayage de campagne.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Données : les quinze scénarios de campagne, entrées DPL 9 à 23, extraites
  localement et non versionnées.
- **Statique seul.**

## Le répartiteur de la racine, en entier

Le lecteur de scénario `0x82249718` n'utilise pas de table : c'est une suite de
dix blocs conditionnels. Les voici tous, ce qui referme au passage une question
du cycle 1096 :

| slot | champ | lecteur | dimensionneur |
| ---: | --- | --- | --- |
| 0 | `+0x04` | `0x82309D20` | `0x82309C00` |
| 1 | `+0x08` | `0x82309A88` | `0x82309978` |
| 2 | `+0x0C` | `0x82309758` | `0x82309620` — `SubMisTblBin` |
| 3 | `+0x10` | `0x823094D8` | `0x823093C8` — `RadioTblBin` |
| 4 | `+0x14` | `0x823092E0` | `0x823092A0` |
| 5 | `+0x18` | `0x82309158` | `0x82309120` |
| **6** | **`+0x1C`** | **`0x82309020`** | **`0x82308F18`** |
| 7 | `+0x20` | `0x82308F50` | `0x82308F18` |
| 8 | `+0x24` | `0x82309D20` | `0x82309C00` |
| 9 | `+0x28` | `0x82308F50` | `0x82308F18` |

**Le slot 8 partage le lecteur du slot 0.** C'est la raison, jamais énoncée
jusqu'ici, pour laquelle `0x820A7070` consomme les deux avec le même code
(cycle 1096) : ce sont deux listes de la même classe.

## Le lecteur du slot 6

`0x82309020` n'a **aucune chaîne d'erreur** — comme la liste `0x28`, sa classe
ne peut pas être nommée par elle-même. Sa forme :

```c
record[0] = data;                       // l'octet de tête est le compte
record[1] = buffer;                     // un tableau de pointeurs, foulée 4
for (i = 0; i < count; i++) {
  element  = buffer + 4*i;
  *element = enfant i résolu, ou 0;
  switch (*(u8 *)(*element + 0xA6)) {   // un octet de type dans le bloc pointé
    case 0: record[2] = element; break;
    case 1: record[3] = element; break;
    case 2: record[4] = element; break;
  }
}
```

Les trois derniers mots retiennent donc **le dernier élément de chaque type**.
Les blocs pointés font au moins `0xA7` octets et sont presque entièrement des
flottants.

## Du slot à la zone de mission

Le chargeur passe ce slot à `FUN_82266EF0` (`0x8219BF74`), qui recopie les mêmes
trois pointeurs, par le même octet `+0xA6`, dans **`contexte+0x270`,
`+0x274`, `+0x278`**.

`0x8226A018`, au démarrage de la mission, choisit entre les deux premiers :

```c
if (Function_82267BF0()) bloc = contexte[0x274];   // type 1
else                     bloc = contexte[0x270];   // type 0
if (bloc != 0 && *(char *)(bloc + 0xA6) != 2)
    FUN_82268b28(bloc[0x28], bloc[0x30], bloc[0x34], bloc[0x3C], contexte);
```

`0x82267BF0` répond vrai quand le mode rendu par `FUN_82090438(global+0x70, 0)`
vaut 4, 6, 7, 9 ou 0x0E. **Le type est donc choisi par le mode de jeu**, et le
type 2 est explicitement exclu de cette installation.

Et `FUN_82268B28` dit ce que sont ces quatre flottants :

```c
contexte+0x28C = min(a, c);   contexte+0x294 = max(a, c);
contexte+0x290 = min(b, d);   contexte+0x298 = max(b, d);
```

Quatre valeurs normalisées en min/max par paire : **un rectangle aligné sur les
axes**. Le slot 6 fournit donc la zone de mission initiale.

C'est la **même fonction** que le pas d'étiquette 0 d'une sous-mission appelle
(cycle 1097), avec ses propres flottants. Le slot 6 pose la zone au démarrage ;
le script de sous-mission la remplace ensuite.

## Ce que les données disent, sur les quinze missions

| entrée | éléments | types | rectangles |
| ---: | ---: | --- | --- |
| 9 | — | slot absent | |
| 10 | 1 | 2 | (3920, 31544) – (6432, 38040) |
| 11, 12, 15, 16, 17, 18, 23 | 2 | 0, 1 | ±50000 les deux |
| 13, 14, 19, 21 | 3 | 2, 0, 1 | type 2 propre, types 0/1 ±50000 |
| 20 | — | slot absent | |
| 22 | 1 | 2 | (−18424, −3048) – (−13304, 8) |

Deux régularités nettes :

- **les types 0 et 1 portent toujours la boîte monde ±50000**, vingt-deux
  occurrences, identiques d'une mission à l'autre — ce sont des préréglages
  cuits dans chaque scénario, pas des données de mission ;
- **le type 2 porte toujours un rectangle propre à la mission**, six
  occurrences, toutes différentes, et nettement plus petites.

Le type 2 est précisément celui que `0x8226A018` refuse d'installer. Il est mis
en cache en `contexte+0x278` pour un autre lecteur.

## Ce que cela n'établit pas

- **Quel autre lecteur consomme `contexte+0x278`**, donc à quoi sert le
  rectangle propre à la mission. C'est la question suivante, et elle est nette.
- **Quels axes du monde** sont `bloc+0x28`/`+0x34` et `bloc+0x30`/`+0x3C`. Les
  vérifier contre les coordonnées des ordres d'étiquette 2 est faisable et n'a
  pas été fait ici.
- Le reste des ~0xB0 octets de chaque bloc, dont un flottant en `+0x38` qui
  varie par mission (1500.0, 2000.0) sans être lu par ce chemin.
- Pourquoi les entrées 9 et 20 se passent du slot 6.
- **Une ambiguïté à ne pas masquer** : `0x821AC368` écrit lui aussi
  `+0x270/+0x274/+0x278`, mais depuis trois globaux sans rapport, et les objets
  qu'il y range ont des champs en `+0x5064` et `+0x55FC`. Il n'a **aucun
  appelant direct**, donc rien ne le relie au contexte de mission ; le plus
  probable est qu'il opère sur une autre classe de contexte — le chargeur en
  alloue trois tailles selon le mode (`0x82199F68`). Ce n'est pas tranché.
