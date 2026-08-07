# Cycle 1108 — le bit 1 de `global+0x37038`, et une correction

Date : 2026-08-09. La question ouverte du cycle 1107 — et la réponse dément la
formulation que ce cycle avait employée.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Statique seul.**

## Tous les accès, énumérés

Le champ n'est pas atteint par un déplacement : la constante `0x37038` est
matérialisée (`lis 0x3 ; ori 0x7038`) puis indexée sur `PTR_DAT_826e4eb4`.
Six sites la matérialisent, et ils se répartissent ainsi :

| site | fonction | accès |
| --- | --- | --- |
| `0x82213A2C` | `0x82213758` | **écrit `0xFFFFFFFF`** — tous les bits posés |
| `0x822EB1FC` | `0x822EB090` | **écrit `0xFFFFFFFF`** |
| `0x82229500` | `0x82229250` | lit, teste le **bit 0** |
| `0x822EB86C` | `0x822EB5A8` | lit, teste le **bit 1** |
| `0x8226BA60` | — | matérialise la constante, l'usage est plus loin |
| `0x8226D61C` | `0x8226D1C8` | **pose le bit 1** |

Plus `FUN_822667C8`, qui **efface le bit 1** en armant la minuterie.

`0x822EB090` est la fonction qui installe l'enregistrement de zone (elle lit
`+0x270`/`+0x274`, cycle 1105) : **la remise à `0xFFFFFFFF` accompagne le
démarrage de phase**.

## La correction

Le cycle 1107 écrivait : « un drapeau global posé quand le temps est épuisé ».
**C'est trop fort.** L'ordre réel des écritures est :

```
démarrage de phase (0x822EB090)  → tous les bits posés, dont le bit 1
armement d'une minuterie (FUN_822667C8) → bit 1 effacé
minuterie épuisée (0x8226D1C8)   → bit 1 reposé
```

Le bit 1 est donc **posé avant qu'aucune minuterie n'existe**. Sa lecture n'est
pas « le temps est épuisé » mais **« aucun compte à rebours n'est en cours »** —
vrai au démarrage, faux pendant, vrai de nouveau après. La formulation du cycle
1107 confondait un état stable avec l'événement qui y ramène ; elle est corrigée
dans le rapport et dans le registre.

## Ce que le bit conditionne

Il est lu **une seule fois**, dans `0x822EB5A8`, comme l'un des quatre termes
d'une garde :

```c
if ((global[0x2653C] & 4) == 0                 // un autre drapeau global
 || (global[0x37038] >> 1 & 1) == 0            // aucun compte à rebours en cours
 || contexte[0xCE] != 0
 || FUN_82267DA8(contexte) != 0)
      autorisé = false;
else {
      cVar8 = Function_822E6010(objet + 0x348, objet + 0x350, &pile);   // un test géométrique
      ...
      autorisé = ...;
}
if (autorisé) {
    if ((contexte[1] & 4) == 0) { Function_822E79B0(contexte); ... }
    else contexte[1] &= ~4;
}
```

Et `Function_822E79B0` :

```c
*(u64 *)(objet + 0x7F0) = *(u64 *)(objet + 0x350);
*(u64 *)(objet + 0x7F8) = *(u64 *)(objet + 0x358);
Function_8219AD20(objet + 0x348, pile, 0);
```

soit **un instantané de deux paires de mots depuis `+0x350`/`+0x358` vers
`+0x7F0`/`+0x7F8`**, puis un appel à une grosse routine sur `objet + 0x348`.

## Ce que cela établit

Le bit 1 de `global+0x37038` est un **état** — « aucun compte à rebours n'est en
cours » — remis à vrai au démarrage de phase, effacé pendant qu'une minuterie
tourne, reposé quand elle s'épuise. Il sert de **précondition, en un seul
endroit**, à un chemin qui prend un instantané de transformation et appelle
`0x8219AD20`.

## Ce que cela n'établit pas, et ce qu'il ne faut pas en conclure

- **Ce n'est pas un échec de mission.** Aucun appel d'échec, aucune transition
  d'état, aucun débriefing sur ce chemin. La tentation de lire « sortie de zone
  → temps écoulé → mission perdue » est forte et **le code ne la porte pas**.
  Ce que le bit gouverne est un instantané et une routine non identifiée.
- Ce que fait `0x8219AD20` — grosse fonction, aucune entrée au catalogue.
- Les trois autres termes de la garde comptent autant que le bit ; rien ici ne
  dit lequel domine en pratique.
- L'usage du site `0x8226BA60`, qui matérialise la constante sans que la lecture
  soit dans la fenêtre examinée.
- Le **bit 0** du même mot, testé par `0x82229250`, n'a pas été suivi.
