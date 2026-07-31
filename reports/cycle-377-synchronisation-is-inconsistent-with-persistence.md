# Cycle 377 — la synchronisation n'explique pas une absence permanente

## 1. Raisonnement, avant de dépenser une mesure

L'hypothèse restante du cycle 376 : les deux planches sont chargées très tard
(#2537, #2548), au moment où la passe les échantillonne ; une barrière manquante
ferait lire une image non encore écrite.

Confrontation avec un fait déjà mesuré : **les glyphes sont absents de *toutes*
les images capturées** — la rafale de huit captures du cycle 360, espacées de
2,5 s, est identique ; les captures des cycles 336 à 372 le sont aussi, sur des
dizaines de secondes.

Or une course au chargement se résoudrait en **une trame** : la texture est
écrite une fois (cycle 374 : dispatch émise, groupes corrects), puis
échantillonnée des milliers de fois. Même si la première lecture précédait
l'écriture, toutes les suivantes la verraient.

**Une absence permanente n'est donc pas ce que produit une course
chargement/échantillonnage.** L'hypothèse est fortement affaiblie sans qu'une
mesure supplémentaire soit nécessaire.

Elle ne survivrait que si la texture était rechargée à chaque trame — or le
cycle 365 a mesuré **un seul chargement** par planche, et le cycle 366 **aucune
invalidation**.

## 2. État réel de l'enquête

Vingt-deux causes éliminées par mesure. La dernière hypothèse nommée est
maintenant écartée par cohérence interne, sans candidat de remplacement étayé.

Ce qui est solidement établi sur ces deux textures :

- elles contiennent des données sources réelles (368) ;
- elles sont chargées une fois, avec une dispatch correcte (365, 374) ;
- elles ne sont jamais invalidées ni recopiées (366) ;
- leur vue, format, constante de fetch, dispositions invitée et hôte sont
  identiques à des textures qui rendent (362-364, 371-372) ;
- la passe qui les dessine peint bien l'écran (360) ;
- le chemin de chargement qu'elles empruntent fonctionne pour ~3 200 autres
  textures (376).

Et l'échantillon reste nul, de façon permanente.

## 3. Ce que cela signifie pour la suite

Toutes les mesures faites portent sur **ce qui est déclaré ou émis**. Aucune n'a
observé **le contenu de l'image hôte après écriture**. C'est le seul maillon
jamais inspecté directement, et il est désormais le seul endroit où la
contradiction peut se loger.

Mesure suivante, et elle est de nature différente des vingt-deux précédentes :
lire les octets de l'image Vulkan après la dispatch de chargement — pas les
paramètres qui la décrivent — pour ces deux textures et pour une texture témoin
qui rend. Si l'image hôte est vide alors que la source ne l'est pas, la
transformation elle-même est fautive, et le *load shader* devient examinable
ligne à ligne.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
