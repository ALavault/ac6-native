# Cycle 1106 — personne ne lit `contexte+0x278`

Date : 2026-08-09. La question ouverte du cycle 1105, et sa réponse : un négatif
établi, pas un négatif supposé.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Statique seul.**

## Une méthode qui ne marchait pas, et pourquoi

Premier réflexe : chercher les fonctions qui récupèrent le contexte de mission
par `*(global + 0x29C80)` et lisent `+0x278`. **Aucune des huit candidates ne le
faisait** — et le contrôle a montré que le test ne valait rien : `0x8226A018`,
consommateur avéré du contexte, ne le récupère pas non plus par ce chemin. Il le
reçoit en paramètre.

Un test sans pouvoir discriminant qui rend « aucun » n'est pas un résultat.
Il a fallu en changer.

## Ce qui a marché : l'octet de type

Le déplacement `0xA6` — l'octet de type d'un bloc de zone — est bien plus rare
qu'un `0x278`. Dix-neuf sites, seize fonctions. Trois étaient connues
(`0x82309020` le lecteur, `0x82266EF0` le cache, `0x8226A018` l'installateur),
deux étaient neuves dans le cluster mission :

**`0x82268C10(contexte, bloc)`** — l'installateur *général* :

```c
if (bloc != 0 && *(char *)(bloc + 0xA6) != 2)
    FUN_82268b28(bloc[0x28], bloc[0x30], bloc[0x34], bloc[0x3C], contexte);
*(int *)(contexte + 0x26C) = bloc;      // toujours, même si rien n'a été posé
```

**`0x8226F600`** lit `*(contexte + 0x26C)` et se déclenche quand son octet de
type vaut **1**.

C'est donc `contexte+0x26C` — *l'enregistrement de zone courant* — qui est lu,
pas `+0x278`.

## L'énumération, site par site

Il reste à montrer que `+0x278` n'est lu nulle part. Dix sites dans le binaire
lisent ce déplacement ; chacun appartient à une **autre structure** :

| site | fonction | ce que la structure fait de `+0x278` |
| --- | --- | --- |
| `0x82091468` | `0x82090818` | le charge avec `+0x270`, `+0x274`, `+0x27C` et les convertit en entiers par `fctidz` — ce sont des **flottants** |
| `0x82198528`, `0x8219858C`, `0x821985D8` | `0x82198488` | index décalé de 3 bits et flottants voisins |
| `0x821AAA3C` | `0x821AA9A8` | `*(+0x278)+0xB4 = *(+0x270)[0x968]*0xE0 + …` — même famille que `0x821AC368`, champs `+0x5064`/`+0x55FC` |
| `0x821CB554` | `0x821CAA50` | comparé à **3** comme entier, `+0x26C`/`+0x270` lus en flottants |
| `0x823D9F24`, `0x823D9F6C` | `0x823D8CE0` | pointeur d'une autre structure, voisin `+0xE8` |
| `0x823DACB8` | `0x823DACA8` | comparé en **non signé** à une soustraction, `+0x274` écrit comme compteur |
| `0x823DAF38` | `0x823DAEE0` | même forme : compteur et seuil |

Et l'accès **indexé** au trio est exclu aussi : les seules instructions du
binaire qui forment `+0x270`, `+0x274` ou `+0x278` par arithmétique dans le
cluster mission sont `0x82266F40`, `0x82266F38` et `0x82266F30` — **à
l'intérieur de `FUN_82266EF0`**, l'écrivain lui-même.

**`contexte+0x278` est écrit et jamais lu.** Le pointeur de type 2 — le seul
rectangle propre à la mission — est mis en cache et n'est consommé par rien.

## Ce que le code lit à la place

Quatorze sites appellent `0x82268C10`, tous dans le cluster `0x822Exxxx`, et
tous passent `+0x270` ou `+0x274`, choisis par le même prédicat de mode
`0x82267BF0` :

```
822e4d30  bl 0x82267bf0
822e4d44  lwz r4,0x274(r31)     ; mode 4, 6, 7, 9 ou 0x0E
822e4d50  lwz r4,0x270(r31)     ; sinon
```

Avec un repli, et une remise à zéro explicite :

```
822eb3b0  lwz r11,0x26c(r31)    ; si rien n'a été installé
822eb3c4  addi r4,r11,0x1358    ; bloc statique 0x82671358
822eb3c8  bl 0x82268c10
822ed338  li r4,0x0             ; ailleurs : installer un bloc nul
822ed340  bl 0x82268c10
```

Le bloc statique `0x82671358` porte l'octet de type **0** et un rectangle
**entièrement nul** (`+0x28` à `+0x3C` = 0.0).

## Ce que cela explique

Deux missions — les entrées **10 et 22** — ont un slot 6 qui ne contient
**qu'un** élément, de type 2. Pour elles, `FUN_82266EF0` n'écrit jamais `+0x270`
ni `+0x274` : l'installateur reçoit un pointeur nul, ne pose aucun rectangle,
range `0` en `+0x26C`, et le repli statique prend la main.

Le repli n'est donc pas une précaution théorique : **la campagne l'exerce sur
deux missions**, et c'est précisément celles dont le seul enregistrement de zone
est du type que l'installateur refuse.

## Ce que cela n'établit pas

- **Pourquoi** le type 2 est produit par l'outillage de mission puis ignoré par
  le moteur. Donnée morte, reliquat d'édition, ou consommée par un chemin que le
  binaire PAL ne contient pas — rien ici ne tranche.
- Ce que devient un rectangle nul en `contexte+0x28C..0x298` : l'usage de ces
  quatre champs n'est pas suivi.
- Quels axes du monde sont les deux paires de coordonnées.
- L'ambiguïté du cycle 1105 sur `0x821AC368` est **levée dans un sens utile** :
  sa famille (`0x821AA9A8` compris) manipule des objets à champs `+0x5064`,
  `+0x968`, `+0x55FC`, qui ne sont pas ceux du contexte de mission. Ce n'est
  toujours pas la preuve de quelle classe il s'agit.
