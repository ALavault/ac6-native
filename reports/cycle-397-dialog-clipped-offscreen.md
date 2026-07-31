# Cycle 397 — le contenu provisionné ne change rien ; la boîte de dialogue déborde de l'écran

## 1. Le test de contenu est tranché : négatif

L'arborescence `content/B13EBABEBABEBABE/4E4D07D1/00000001` était en place.
La boucle de pilotage rallongée (45 itérations au lieu de 24) a atteint l'écran
à l'itération 7.

La capture est **identique** à celle sans contenu : mêmes boutons YES/NO, aucun
navigateur GAME DATA. **L'hypothèse « stockage de contenu manquant » est
réfutée.** Elle était de toute façon fragile — l'oracle dessine des emplacements
vides — et la mesure le confirme.

## 2. Ce que le retrait de la surimpression a révélé

Toutes les captures précédentes portaient la surimpression de diagnostic sur la
moitié gauche de l'écran. C'est exactement la zone où le contenu du panneau
devrait se trouver. **Vingt captures ont été interprétées à travers un cache
qui masquait la région en cause.** `--ac6_performance_mode=true` la désactive.

Sans elle, mesures sur `content3/SAVE-clean.png` :

| grandeur | mesure |
|---|---|
| bord gauche du panneau | x = 264 |
| bord droit du panneau | x ≥ 1279 (jamais refermé) |
| largeur d'écran | 1280 |
| bord droit attendu si centré | 1016 |
| bouton NO | tronqué par le bord de l'écran |

Le panneau **déborde de l'écran par la droite**. Un panneau centré dont le bord
gauche est à 264 doit se refermer à 1016 ; il court jusqu'au bord. Le
sur-dimensionnement est d'environ 35 %.

## 3. Conséquence sur les conclusions antérieures

Le cycle 395 concluait que le navigateur GAME DATA n'est « soumis par aucune
passe » et que le défaut est dans la logique invitée. Cette conclusion est
maintenant **douteuse** : si la transformation du dialogue est fausse au point
de pousser le panneau hors de l'écran, les libellés et le navigateur peuvent
être dessinés **au-delà de x = 1280**, donc invisibles sans être absents.

C'est une hypothèse, pas une mesure. Elle n'est pas vérifiée. Ce qui est mesuré,
c'est la géométrie du panneau ; le sort du texte reste à établir.

La distinction est décisive et se teste : rendre à une largeur supérieure, ou
lire les coordonnées des sommets des passes de dialogue, dirait immédiatement si
le texte existe hors champ.

## 4. Discipline

C'est le deuxième renversement en trois cycles (après celui du cycle 394 sur les
textures). Les deux ont la même cause : une conclusion tirée d'une observation
dont le champ était restreint sans que la restriction soit prise en compte —
ici, une surimpression opaque devant la zone examinée.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
