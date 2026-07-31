# Cycle 445 — balayage dense, mais la rotation confond la comparaison

## 1. Le défaut du cycle 444 est corrigé

Plus d'échantillonnage : chaque mot est lu (`kStride = 4`). Le coût est tenu par
une fenêtre de 16 Mo par passe, tournant sur 16 positions pour couvrir 256 Mo.
Chaque fenêtre n'est comparée qu'à elle-même, donc la rotation ne coûte rien en
sensibilité.

## 2. Relevé

| condition | blocs modifiés |
|---|---|
| repos (25 s) | `0xA33D0000`, `0xA1910000`, `0xA1900000`, `0xA18F0000` |
| après **Gauche** ×3 | `0xA33D0000` ×3, `0xA1910000` |

## 3. Pourquoi cela ne conclut toujours pas

**La rotation introduit exactement le biais qu'elle devait éviter.** Une fenêtre
n'est visitée qu'une passe sur seize ; ce qui change pendant l'appui n'est vu
que si la fenêtre correspondante était en cours de balayage à ce moment-là.

Comparer « repos » et « après Gauche » revient donc à comparer deux ensembles de
fenêtres différentes, tirées par la cadence et non par la condition. Les
comptes — 3 contre 1 — n'ont pas de sens sous ce régime.

De plus `0xA33D0000` contient `0xA33DA000`, la page qui change en permanence
depuis le cycle 443. Sa présence dans les deux colonnes est du bruit connu.

## 4. Le vrai obstacle, nommé

Trois cycles de balayage (443, 444, 445) ont buté sur le même compromis :
couvrir largement **et** densément **et** de façon synchrone avec l'entrée. On
ne peut pas avoir les trois avec une somme périodique.

Ce qu'il faut n'est pas un balayage mais un **déclenchement** : capturer un état
mémoire complet juste avant l'appui, un second juste après, et les comparer hors
ligne. Une seule paire, densité totale, aucune rotation, aucun échantillonnage.

Cela demande un mécanisme de capture à la demande — un cvar basculé, ou une
capture déclenchée par le premier front de `DPAD_LEFT` vu dans le pilote — qui
n'existe pas encore dans la sonde.

## 5. Bilan des trois cycles de balayage

Aucun n'a localisé le champ. Chacun a échoué pour une raison distincte et
identifiée : plage trop étroite (443), échantillonnage trop lâche (444),
rotation désynchronisée (445). Les trois diagnostics sont justes et cumulables ;
le quatrième essai doit les corriger ensemble, par capture déclenchée.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
