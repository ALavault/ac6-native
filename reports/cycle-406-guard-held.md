# Cycle 406 — la garde a tenu : écran non atteint, aucun chiffre publié

## 1. Résultat

Exécution instrumentée pour répondre à la question du cycle 405 — l'invité
soumet-il encore des tracés pendant que l'image est figée ? La boucle de
pilotage a été portée à 60 itérations et l'arrivée sur l'écran est vérifiée
avant toute lecture du journal.

**L'écran n'a pas été atteint.** Conformément à la règle posée au cycle 405,
aucun chiffre n'est rapporté. Le script s'arrête de lui-même et l'annonce.

C'est le comportement voulu : aux cycles 404 et 405, deux mesures avaient été
lues sur des exécutions qui n'étaient pas sur l'écran visé. La garde évite
désormais que cela se reproduise silencieusement.

## 2. Fiabilité de la navigation, mesurée

Sur les onze dernières tentatives de rejoindre cet écran :

| issue | nombre | itération d'arrivée |
|---|---|---|
| atteint | 8 | 7 à 11 |
| non atteint | 3 | — |

Environ **sept fois sur dix**, avec arrivée entre la 7ᵉ et la 11ᵉ itération.
L'échantillon ne permet pas de distinguer une dégradation d'une simple
variation ; il n'y a pas lieu d'invoquer l'une plutôt que l'autre.

Conséquence pratique : toute mesure sur cet écran doit prévoir de **ne rien
produire** environ une fois sur trois, et le dire, plutôt que de rapporter ce
qui traîne dans le journal.

## 3. La question du cycle 405 reste ouverte

Elle est inchangée et toujours bien posée :

- tracés invités qui continuent → scène re-soumise à l'identique, l'état de
  l'interface est à lire dans les données invitées ;
- tracés qui cessent → l'invité a quitté sa boucle de rendu, l'hôte représente
  le dernier tampon.

L'instrumentation pour y répondre est prête et vérifiée (fenêtre horodatée à
partir de l'instant d'arrivée, contrôle de gel par comparaison de pixels sur la
même fenêtre, immunité à la rotation du journal). Il lui manque seulement une
exécution qui atteigne l'écran.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
