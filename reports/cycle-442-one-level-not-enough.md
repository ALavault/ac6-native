# Cycle 442 — un niveau de pointeurs ne suffit pas

## 1. Extension de la surveillance

Balayage de l'objet écran à la recherche de mots ressemblant à des pointeurs
invités (`0xA3xxxxxx`, alignés), puis surveillance des 256 premiers octets de
chaque cible — un niveau d'indirection, jusqu'à 24 cibles.

## 2. Résultat

| condition | changements |
|---|---|
| repos (4 s) | `obj=0xA330C398 +8` |
| appui **Gauche** | `obj=0xA330C398 +8` |
| appui **A** | `obj=0xA330C398 +8` |

Un seul mot bouge, **identique dans les trois cas**, exactement comme au cycle
441 sur le conteneur.

## 3. La tension qu'il faut nommer

Le surlignage bascule **visiblement** de NO à YES sur Gauche — mesuré, bande
131 contre bruit ~3 (cycle 421). Une valeur change donc quelque part dans la
mémoire invitée.

Et pourtant **rien** dans l'ensemble surveillé — conteneur plus 24 sous-objets —
ne s'en ressent.

Les explications possibles, aucune vérifiée :

1. le champ est **plus loin** que 256 octets dans un sous-objet surveillé ;
2. il est à **deux niveaux** d'indirection ou davantage ;
3. mon critère de pointeur (`0xA3xxxxxx`) exclut la bonne région — d'autres
   plages invitées existent ;
4. l'écran de dialogue n'est pas rattaché à cet objet écran du tout.

La quatrième mérite attention : rien n'a jamais démontré que le dialogue
YES/NO appartient à l'objet suivi par `sub_821C56F8`. Cette hypothèse traîne
depuis le cycle 438 sans avoir été testée.

## 4. Ce que la méthode a tout de même acquis

Elle est fiable : elle détecte un mot qui bouge dans un sous-objet, sans
présupposé de forme. Deux cycles l'ont appliquée à deux périmètres, et tous deux
sont **négatifs de façon informative** — ils excluent le conteneur et son premier
niveau.

C'est une élimination, pas un progrès vers le livrable, et il faut le dire ainsi.

## 5. Reprise, par ordre de coût

- élargir la fenêtre par sous-objet (256 → 2048 octets) : une constante ;
- assouplir le critère de pointeur : relever d'abord les plages réellement
  présentes plutôt que de supposer `0xA3` ;
- surtout : **vérifier que le dialogue appartient bien à cet écran**, en
  cherchant l'objet dont un champ vaut successivement les deux états de
  sélection, par balayage large de la mémoire invitée entre deux captures.

Le dernier point est le seul qui teste l'hypothèse restée implicite.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
