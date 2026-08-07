# Cycle 1109 — `0x8219AD20` est le moteur de transition d'une machine à états

Date : 2026-08-09. La routine que le cycle 1108 avait laissée sans nom.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Fonction `0x8219AD20`, étendue `[0x8219AD20, 0x8219B467]`, 1 864 octets.
- **51 appelants**, tous dans le cluster `0x822Exxxx` sauf `0x82199A0C`.
- **Statique seul.**

## Ce que la forme dit

Le corps ne ressemble à rien d'autre qu'à une chose. Dans l'ordre :

1. **Deux chaînes remontées.** Deux boucles identiques suivent un pointeur de
   membre — un mot de 64 bits dont la moitié haute est une adresse de code et
   la moitié basse un ajustement — en rappelant chaque fois le gestionnaire avec
   la constante **`-4`**. L'une part de `param_2` (la cible), l'autre de
   `this+0x18` (l'état courant). Chaque étape est empilée dans un **tampon
   circulaire** de 16 octets par entrée.
2. **Recherche de l'ancêtre commun.** Les deux chaînes sont ensuite comparées
   entrée par entrée jusqu'à divergence.
3. **Une boucle sur la chaîne courante**, avec la constante **`-1`**.
4. **Une boucle sur la chaîne cible**, avec la constante **`-3`** — et cette
   boucle-là **transmet `param_3`**, l'argument reçu par la fonction.
5. **Une boucle finale** qui appelle l'état devenu courant avec **`-5`**, et
   tant qu'il répond zéro, le rappelle avec **`-3`**, en descendant.

C'est la transition d'une **machine à états hiérarchique** : sortir des états
quittés, entrer dans les états atteints, puis descendre dans les sous-états
initiaux. Les quatre constantes forment l'alphabet de signaux réservés —
`-4` interroge le sur-état, `-1` sort, `-3` entre, `-5` transition initiale.

> Le rôle de chaque constante est lu **à sa position dans l'algorithme**, pas à
> une chaîne ou à une table : c'est une interprétation de structure. Elle est
> forte — l'ordre remonter / comparer / sortir / entrer / descendre n'admet
> guère d'autre lecture — mais ce n'est pas la même chose qu'un nom trouvé dans
> le binaire.

## Où vit l'état

`0x8219AD20` reçoit `this = objet + 0x348` : ses appelants passent cette adresse.
À la fin, il écrit

```c
*(u64 *)(this + 0x08) = param_2[0];
*(u64 *)(this + 0x10) = param_2[1];
```

soit **l'état courant sur 16 octets en `objet+0x350..0x360`**.

Ce qui referme le cycle 1108. `Function_822E79B0` faisait :

```c
*(u64 *)(objet + 0x7F0) = *(u64 *)(objet + 0x350);
*(u64 *)(objet + 0x7F8) = *(u64 *)(objet + 0x358);
Function_8219AD20(objet + 0x348, cible, 0);
```

**Il sauvegarde l'état courant, puis transitionne.** Les deux paires de mots
copiées en `+0x7F0`/`+0x7F8` ne sont pas une transformation géométrique — c'est
l'état d'où l'on vient, mis de côté pour pouvoir y revenir.

## Ce que cela corrige

Le cycle 1108 décrivait `Function_822E79B0` comme « un instantané de
transformation ». **C'était faux** : `objet+0x350`/`+0x358` est le pointeur
d'état de la machine, pas une matrice. La proximité de `objet+0x348` avec les
champs `+0x348`/`+0x350` lus par `Function_822E6010` m'avait fait supposer de la
géométrie ; la fin de `0x8219AD20` montre que ces mots sont un pointeur de
membre.

Et la lecture du cycle 1108 s'en trouve renforcée sur le fond : le bit
« aucun compte à rebours en cours » garde bien **une transition d'état**, dans
une machine dont les 51 gestionnaires occupent le cluster `0x822Exxxx`.

## Ce que cela n'établit pas

- **Quels états.** Aucun gestionnaire n'est nommé, et les sites d'appel de
  `0x822E8840` et voisins ne sont même pas contenus dans des fonctions définies :
  ce cluster reste partiellement non délimité, malgré S0.
- Que la machine soit celle de la mission, du joueur, ou d'autre chose. Elle vit
  sur un objet dont on ne connaît que des offsets.
- Le rôle exact de `param_3`, transmis aux seuls gestionnaires d'entrée.
- Les helpers `0x8219AC88`, `0x8219B468`, `0x8219B768`, `0x8219B870` et la table
  `PTR_Function_8219BD90_8206465C` ne sont pas décrits ici, seulement situés.

## Prochaine tranche, si elle a lieu

Délimiter les fonctions du cluster `0x822Exxxx` — 51 gestionnaires dont
plusieurs ne sont pas des fonctions pour Ghidra — puis lire lesquels
transitionnent vers lesquels. C'est la carte de la machine, et c'est ce qui
dirait enfin de quoi elle est la machine.
