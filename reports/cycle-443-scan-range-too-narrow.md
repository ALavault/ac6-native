# Cycle 443 — 4 Mo balayés, aucun changement lié à l'entrée

## 1. Méthode

Somme de contrôle par page de 4 Ko sur `0xA3000000`–`0xA3400000` (1024 pages),
recalculée toutes les 30 trames, et signalement des pages dont la somme change.
Aucun présupposé sur l'objet propriétaire du dialogue — c'était le point du
cycle 442.

## 2. Résultat

| condition | pages modifiées |
|---|---|
| repos (5 s) | `0xA33DA000`, **10 fois** |
| appui **Gauche** | `0xA33DA000`, 8 fois |

Une seule page bouge, et elle bouge **en permanence**, indépendamment de toute
entrée. C'est du remue-ménage par trame, pas un état de sélection.

**Aucune page de la plage balayée ne distingue Gauche du repos.**

## 3. Ce que cela dit

Le champ de sélection n'est **pas** dans `0xA3000000`–`0xA3400000`.

Or les objets suivis jusqu'ici y sont tous : écran `0xA3317DE0`, sélecteur
`0xA3300060`, sous-objet `0xA330C398`. La plage était donc plausible — elle est
simplement trop étroite.

L'hypothèse implicite mise en cause au cycle 442 — « le dialogue appartient à cet
écran » — n'est **ni confirmée ni infirmée** : le balayage ne l'a pas atteinte,
faute de couvrir la bonne région.

## 4. Ce que la mesure vaut quand même

Elle est propre et son témoin est intégré : la page qui bouge sans arrêt montre
que le détecteur fonctionne. Un balayage muet aurait été ambigu ; celui-ci ne
l'est pas — il voit du changement, simplement pas celui qu'on cherche.

## 5. Reprise

Relever d'abord **où vit réellement la mémoire invitée** au lieu de le supposer :
l'allocateur (`xmemory.cpp`) ou la carte mémoire du noyau donnent les plages
effectivement engagées. Puis balayer celles-là, et non une plage choisie parce
que trois objets connus s'y trouvaient.

C'est la cinquième fois que je restreins un instrument sur une supposition —
motif textuel (436, 439, 440), critère de pointeur (442), plage mémoire (ici).
La correction est toujours la même : mesurer l'étendue avant de chercher dedans.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
