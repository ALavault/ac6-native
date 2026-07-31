# Cycle 438 — l'écran bloqué est identifié par sa table virtuelle

## 1. Relevé, stable sur tous les échantillons

```
[ac6-screen-id] screen=0xA3317DE0 vtable=0x820679A0
                slots=821C3690, 82079088, 821CA6C8
```

## 2. Ce que cela donne

| élément | valeur |
|---|---|
| objet écran | `0xA3317DE0` |
| **classe (table virtuelle)** | **`0x820679A0`** |
| méthode virtuelle 0 | `0x821C3690` |
| méthode virtuelle 1 | `0x82079088` |
| méthode virtuelle 2 | `0x821CA6C8` |

L'écran est désormais nommé par **son propre code**, et non par un appel noyau
survenu au même moment — c'est précisément ce que le cycle 437 exigeait après la
méprise sur le sélecteur.

## 3. Rapprochement avec le cycle 417

La sonde de dialogue relevait alors `+0 -> 0x820679F4`, une table virtuelle
**voisine** de celle-ci (`0x820679A0`, soit 84 octets plus bas). Deux classes
distinctes du même groupe, vraisemblablement dérivées d'une base commune.

Je note la proximité sans en tirer de parenté : deux adresses voisines ne font
pas une hiérarchie, et cette série a assez souffert de déductions de ce genre.

## 4. Ce qui devient possible

Les trois méthodes virtuelles sont des adresses de fonctions recompilées,
lisibles directement. La méthode qui traite l'entrée — donc celle qui reçoit le
front d'appui de A mesuré au cycle 427 et n'en fait rien — est parmi elles ou
atteignable depuis elles.

C'est la première fois que le code **propre à cet écran** est localisable. Les
quarante cycles précédents opéraient sur des couches partagées : pilote,
noyau, gestion des manettes, sélecteur de périphérique — toutes désormais
mesurées saines.

## 5. Reprise

Lire `0x821C3690`, `0x82079088` et `0x821CA6C8`, en entier et sans filtrer la
sortie, pour trouver celle qui consulte les champs d'entrée. Puis y suivre le
sort du bit A (`0x1000`).

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
