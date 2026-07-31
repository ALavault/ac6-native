# Cycle 404 — l'image est figée au pixel près, et la durée d'appui n'y change rien

## 1. Mesure

Sur l'écran bloqué, en mode performance (sans surimpression) :

| observation | valeur |
|---|---|
| dérive sans entrée sur 12 s | **0.000** |
| appui maintenu 2 s sur A | 0.000 |
| appui maintenu 4 s sur A | 0.000 |
| Gauche 2 s puis A 2 s | 0.000 |

Écart **exactement nul** : les images sont identiques au pixel près. Et le
journal du cycle 402 montre `PRESENT` toutes les 16-17 ms. L'invité
**re-présente donc indéfiniment une image identique**.

## 2. Correction d'une mesure antérieure

Le cycle 400 avait relevé une dérive de 0.423 sans entrée et des écarts allant
jusqu'à 8.4, attribués à un fond animé. Ici, la dérive est nulle. Les deux
mesures ne portent donc pas sur le même état : l'exécution du cycle 400 avait
atteint un écran encore animé, celle-ci un écran totalement figé.

La conclusion du cycle 400 — « aucune entrée ne produit de réponse » — reste
valable dans les deux cas, mais sa justification par « la dérive explique tout
l'écart » ne vaut que pour cette exécution-là. Ici la démonstration est plus
simple et plus forte : **rien ne change du tout**.

## 3. Hypothèse écartée

La durée d'appui n'est pas en cause. Le pilote MnK exige des appuis maintenus
parce qu'un appui de 12 ms tombe entre deux scrutations ; on pouvait supposer
que ce dialogue exigeait davantage. Quatre secondes ne changent rien. C'est
écarté.

## 4. État figé, mais présentation active

La combinaison est précise et vaut d'être notée telle quelle :

- la boucle de présentation tourne (60 Hz, mesuré au cycle 402) ;
- l'entrée parvient à l'invité (masques exacts, cycle 401) ;
- aucun fil invité ne tourne en boucle (cycle 403) ;
- l'image ne change jamais d'un seul pixel (ici).

Un programme qui présente activement une image qu'il ne recalcule jamais a
cessé de mettre à jour sa scène tout en continuant à la soumettre. Ce n'est ni
un blocage de fil, ni une perte d'entrée, ni un défaut de rendu du texte.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
