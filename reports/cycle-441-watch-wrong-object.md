# Cycle 441 — surveillance mémoire : bonne méthode, mauvais objet

## 1. Ce qui a été fait

Instantané des 512 premiers octets de l'objet écran `0xA3317DE0` à chaque trame,
et signalement des mots qui changent. Approche choisie au cycle 440 précisément
parce qu'elle **ne dépend d'aucun motif textuel** — les quinze cycles précédents
ayant échoué sur ce point.

## 2. Résultat

| condition | décalages modifiés |
|---|---|
| repos (4 s) | `+32` |
| appui **Gauche** | `+32` |
| appui **A** | `+32` |

Un seul décalage bouge, **identique dans les trois cas**. Aucun changement ne
distingue une entrée d'une autre, ni d'ailleurs de l'absence d'entrée.

Or l'appui sur Gauche **fait visiblement basculer** le surlignage de NO vers YES
(mesuré au cycle 421, bande 131 contre bruit ~3). Le champ de sélection existe
donc, et il **n'est pas** dans les 512 premiers octets de cet objet.

## 3. Diagnostic de la tentative

La méthode est valide et le restera : elle a bien détecté le seul mot qui bouge,
sans rien présupposer de la forme du code. **C'est la cible qui était fausse.**

L'objet écran est un conteneur ; l'état du dialogue vit dans un sous-objet
atteint par pointeur, comme le sélecteur de périphérique vivait en `[screen+4]`
(cycle 435). Surveiller le conteneur ne montre pas ce que font ses membres.

## 4. Incident d'outillage

La première exécution était inexploitable : mon étiquette `[ac6-watch]` entrait
en collision avec une sonde GPU existante portant la même, si bien que les
relevés mélangeaient deux sources. Renommée en `[ac6-objwatch]`.

À retenir : vérifier qu'une étiquette de journal est libre avant de l'employer.
Le coût a été une exécution complète.

## 5. Suite

Appliquer la même surveillance au **sous-objet dialogue**, une fois son pointeur
localisé. Le repérage se fait sans motif : balayer les mots de l'objet écran qui
ressemblent à des pointeurs invités (`0xA3xxxxxx`), les suivre, et surveiller
chacun. Le champ qui bascule sur Gauche et pas sur A désigne alors le dialogue.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
