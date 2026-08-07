# Cycle 1107 — à quoi sert `contexte+0x28C..0x298`

Date : 2026-08-09. Les quatre flottants que le cycle 1105 avait vus écrire, et
ce que le moteur en fait.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Statique seul.**

## Le prédicat, et les axes

Sept fonctions lisent les quatre champs en flottants. La plus petite les définit :

```c
undefined8 FUN_82268ba0(int contexte, float *point) {
  if (point == 0)
      point = (float *)(*(int *)(*(int *)(global + 0x29FC8) + 0x1008) + 0x50);
  if (point[0] < contexte[0x28C] || contexte[0x294] < point[0] ||
      point[2] < contexte[0x290] || contexte[0x298] < point[2])
      return 0;
  return 1;
}
```

**Les deux paires sont `point[0]` et `point[2]`** — les composantes 0 et 2 d'un
triplet, la première et la troisième. Le rectangle est donc **horizontal**, dans
le plan du sol, et l'altitude n'y entre pas. C'est la réponse à la question que
le cycle 1105 avait laissée ouverte sur les axes.

Le sujet par défaut est `*(*(global + 0x29FC8) + 0x1008) + 0x50` : une position à
l'offset `+0x50` d'un objet — **la même disposition que les objets construits par
la factory** du cycle 1096, qui reçoivent trois flottants en `+0x50/+0x54/+0x58`.

`FUN_82268BA0` a **onze appelants** répartis dans tout le binaire. C'est le
prédicat général « ceci est-il dans la zone ».

## L'application, et ce que coûte la sortie

`0x82268FF0` fait le même test sur le même sujet, et bifurque :

```c
if (dans la zone) {
    if (contexte[0x74] > 0) return;
    if (contexte[4] & 0x80000) return;
    if (contexte[8] & 0x100) return;
    code = 0x14;
} else {
    FUN_822667c8(DAT_82069b28, contexte);              // arme un compte à rebours
    *(uint *)(sujet + 0x1394) |= 2;
    FUN_82256228(global + 0x2458C);
    ...
}
```

Et `FUN_822667C8(durée, contexte)` est un **armement de minuterie** :

```c
*(global + 0x37038) &= ~2;                 // efface un drapeau global
contexte[0x148] = |durée|;                 // la durée totale
contexte[0x144] = |durée|;                 // le restant
contexte[0x14C] = (drapeaux & ~6) | 1;     // bit 0 : armée
if (durée < 0) contexte[0x14C] = drapeaux & ~7;   // durée négative : désarmée
```

## Le point qui relie deux fronts

**C'est la même minuterie que le pas d'étiquette 0 d'une sous-mission arme.**
Le cycle 1097 avait relevé, sans savoir ce que c'était :

```c
if (pfVar3[8] > 0) FUN_822667C8(...);
```

La limite de temps d'une sous-mission et le compte à rebours de sortie de zone
sont **le même mécanisme**, sur les mêmes champs `contexte+0x144/0x148/0x14C`.

Et le gestionnaire de mission `0x8226D1C8` — qualifié depuis le cycle 1093 — les
relit chaque trame :

```
8226d610  lwz  r11,0x14c(r28)      ; les drapeaux
8226d620  rlwinm r11,r11,0,31,31   ; bit 0 inversé
8226d628  beq  → saute             ; armée : ne rien faire
8226d62c  lfs  f0,0x144(r28)       ; le restant
8226d634  bgt  → saute             ; s'il reste du temps
8226d648  stw  r10,0x0(r11)        ; *(global + 0x37038) |= 2
```

Le bit 1 de `*(global + 0x37038)` est exactement celui que `FUN_822667C8` efface
en armant. Un drapeau global posé quand le temps est épuisé.

## Ce que cela établit

`contexte+0x28C..0x298` est **la zone de mission** : un rectangle aligné sur les
axes dans le plan horizontal du monde, stocké en (min x, min z, max x, max z).
Il vient du slot 6 au démarrage (cycle 1105), un pas de sous-mission le remplace
(cycle 1097), onze sites l'interrogent, et en sortir arme une minuterie que le
gestionnaire de mission décompte jusqu'à lever un drapeau global.

La boucle complète, de la donnée au comportement :

```
slot 6 → FUN_82266EF0 → contexte+0x270/0x274 → 0x82268C10 → FUN_82268B28
       → contexte+0x28C..0x298 (min/max par axe)
                    ↓
       FUN_82268BA0(point) → dedans / dehors
                    ↓ dehors
       FUN_822667C8 → contexte+0x144/0x148/0x14C → 0x8226D1C8 → global+0x37038 bit 1
```

## Ce que cela n'établit pas

- **Ce que fait le jeu du drapeau `global+0x37038` bit 1.** C'est la question
  suivante, et c'est elle qui dirait si l'expiration est un échec de mission.
- La valeur de `f31` dans la comparaison de `0x8226D1C8` n'est pas épinglée :
  la lecture « le restant est tombé à son plancher » suppose zéro, ce que ce
  cycle n'a pas vérifié.
- Qui décrémente `contexte+0x144` — le tick n'apparaît pas dans la fenêtre lue.
- L'identité du sujet `*(global+0x29FC8)->[0x1008]`. Sa position est à `+0x50`
  comme celle des objets construits, ce qui est cohérent avec le joueur sans le
  prouver.
- Le code `0x14` que `0x82268FF0` produit dans la branche « dedans ».
