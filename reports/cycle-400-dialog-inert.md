# Cycle 400 — la boîte de dialogue n'accepte aucune entrée (résultat négatif contrôlé)

## 1. Le pas que j'aurais dû tenter il y a trente cycles

L'objectif est de jouer la mission 1, pas de rendre parfaitement ce dialogue.
Avant de continuer à diagnostiquer son affichage, il fallait vérifier si l'on
peut simplement **y répondre et passer outre**. Ce n'avait jamais été testé.

## 2. Protocole

Écran atteint par le détecteur, puis balayage de dix touches, capture après
chacune. Deux garde-fous, tous deux nécessaires :

- **témoin sans entrée** : deux captures espacées de 2 s sans toucher au
  clavier → dérive de 0.423 (le fond est animé, il change tout seul) ;
- **touches non affectées** : `Tab` et `BackSpace` ne sont liées à rien dans le
  pilote MnK. Elles servent de témoin négatif.

## 3. Mesures

| touche | Δ vs base | affectée ? |
|---|---|---|
| shift (B) | 0.615 | oui |
| Left | 7.158 | oui |
| Right | 7.390 | oui |
| Up | 7.751 | oui |
| Down | 8.219 | oui |
| space (A) | 8.058 | oui |
| Return | 8.100 | oui |
| Escape (Start) | 8.299 | oui |
| **Tab** | **8.459** | **non** |
| **BackSpace** | **8.323** | **non** |

## 4. Lecture

Les touches **non affectées obtiennent le même écart que les affectées**. Et les
écarts croissent de façon monotone dans l'ordre de capture — 0.6, 7.2, 7.4, 7.8,
8.2, 8.1, 8.3, 8.5 — c'est-à-dire avec le temps écoulé depuis la base, pas avec
la nature de la touche.

**La totalité de l'écart s'explique par la dérive du fond animé.** Aucune entrée
ne produit de réponse distinguable. La boîte de dialogue est **inerte**.

Sans le témoin des touches non affectées, ces dix lignes se lisaient « toutes
les entrées fonctionnent ». C'est exactement l'erreur des cycles 394, 397 et
399 : conclure sans mettre l'instrument en cause. Ici le garde-fou était en
place avant la mesure.

## 5. Conséquence

Le blocage n'est pas un défaut de rendu du texte : c'est que **l'invité ne
traite pas les entrées sur cet écran**. Un dialogue au panneau vide, débordant
de l'écran par la droite (cycle 397) et ne réagissant à rien, ressemble à un
écran construit à moitié dont la boucle d'entrée n'a jamais démarré.

Piste suivante, à traiter côté invité et non côté GPU : déterminer si cet écran
attend un événement qui n'arrive jamais — fin d'énumération de contenu, rappel
d'entrée/sortie — plutôt que d'observer davantage de pixels.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
