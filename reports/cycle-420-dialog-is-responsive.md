# Cycle 420 — le dialogue **répond** ; l'énumérateur est hors de cause

## 1. Test A/B de l'élément factice

| bras | injection | énumération | après entrées |
|---|---|---|---|
| `--ac6_fake_content_item=true` | 1 | `added 1 items` | identique |
| témoin (`false`) | 0 | `added 0 items` | identique |

Les deux bras se comportent **exactement pareil**. L'énumération vide n'est
donc pas la cause, et l'énumérateur est **définitivement écarté** — cette fois
par expérience contrôlée avec témoin, et non par raisonnement.

## 2. La découverte importante, faite par le témoin

Après la séquence `A, Gauche, A, Entrée`, l'écran n'a **pas** changé de page :
c'est le même dialogue, mais **YES est surligné à la place de NO**.

**La sélection s'est déplacée.** Le dialogue est interactif.

## 3. Ce que cela invalide

Les cycles 400 et 404 concluaient que « l'écran ne réagit à aucune entrée ».
**C'est faux.** L'erreur est méthodologique et nette :

- ma métrique était l'écart absolu moyen sur **toute l'image** ;
- un surlignage de bouton ne concerne que ~2 % des pixels ;
- le signal réel se noyait sous le seuil que j'avais calibré sur la dérive du
  fond.

Le témoin de touches non affectées du cycle 400 était bon ; c'est la **grandeur
mesurée** qui était trop grossière. Un bon garde-fou sur une mauvaise métrique
donne une fausse certitude — et celle-ci a orienté vingt cycles.

Le détecteur a d'ailleurs signalé `other` ici : il ne reconnaissait plus la
bande de boutons parce qu'elle avait changé d'aspect. J'ai failli lire cela
comme « l'écran a avancé ».

## 4. État réel du blocage

Le dialogue affiche YES/NO, la sélection se déplace, mais **valider ne fait pas
avancer**. Le blocage est là, et nulle part ailleurs : entre la validation et la
transition d'écran.

## 5. Reprise

1. mesurer par **région** (la bande de boutons seule), jamais en moyenne
   plein écran ;
2. déterminer ce que fait l'invité à la validation de YES — c'est le seul pas
   qui reste avant la transition attendue.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
