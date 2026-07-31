# Cycle 405 — mesure ratée, et état vérifié du dossier P1.3

## 1. La mesure tentée, et pourquoi elle ne compte pas

Question : pendant que l'image est figée, l'invité **soumet-il encore des
tracés**, ou l'hôte se contente-t-il de représenter le dernier tampon ?

Deux tentatives, deux échecs, tous deux instrumentaux :

1. **comptage par différence** — le fichier de journal tourne (rotation) entre
   les deux prises ; les différences sont sorties négatives (−4769 tracés).
   Une différence de compteurs suppose un compteur monotone ; il ne l'était pas.
2. **comptage par horodatage** — immunisé à la rotation, mais cette
   exécution-là **n'a pas atteint l'écran cible** (aucune correspondance du
   détecteur sur 45 itérations). Le chiffre obtenu, ~56 tracés par présentation,
   provient d'un écran non identifié.

Ce chiffre n'est donc **pas attribuable à l'état figé** et je ne le présente pas
comme une réponse. La question reste ouverte.

## 2. Ce qui est établi par mesure sur P1.3

| fait | cycle | méthode | garde-fou |
|---|---|---|---|
| le panneau déborde à droite (264 → ≥1279 sur 1280) | 397 | pixels | surimpression désactivée |
| l'arborescence de contenu ne change rien | 397 | comparaison avec/sans | — |
| la transformation NDC est correcte | 399 | lecture de `util/draw.cpp` | calcul revérifié à la main |
| l'écran ne réagit à aucune entrée | 400 | balayage de 10 touches | témoin de touches non affectées |
| l'entrée parvient à l'invité, masques exacts | 401 | sonde `X_INPUT_STATE` | correspondance terme à terme |
| présentation à 60 Hz, aucun appel noyau | 402 | journal debug | — |
| aucun fil invité ne tourne en boucle | 403 | `/proc` × 2 prises | intervalle CPU, pas instantané |
| image identique au pixel près | 404 | écart exact 0.000 | témoin de dérive |

## 3. Ce qui a été réfuté, y compris par moi

- textures manquantes (394) — réfuté par test de suppression
- navigateur « non soumis » (397) — la surimpression masquait la zone
- échelle NDC nulle (398) — artefact de format `{:.3f}` de ma propre sonde
- « pas de boucle d'entrée » (400→401) — l'entrée est bien délivrée
- attente asynchrone (401→402) — aucun appel noyau avant blocage
- machine à états qui tourne en boucle (402→403) — rien ne tourne
- durée d'appui insuffisante (404) — 4 s n'y changent rien
- stockage de contenu absent (395→397) — sans effet

## 4. La question ouverte, formulée précisément

Un programme qui présente à 60 Hz une image qu'il ne recalcule jamais a cessé de
mettre à jour sa scène sans cesser de la soumettre. Reste à trancher, par une
exécution qui atteint effectivement l'écran :

- si les tracés invités continuent → la scène est re-soumise à l'identique, et
  c'est l'état de l'interface qu'il faut lire dans les données invitées ;
- s'ils cessent → l'invité a quitté sa boucle de rendu et l'hôte représente le
  dernier tampon.

Le détecteur atteint l'écran environ deux fois sur trois ; toute mesure sur cet
écran doit **vérifier qu'elle y est arrivée** avant de publier un chiffre. Les
deux échecs ci-dessus viennent tous deux de l'avoir négligé.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
