# Cycle 444 — balayage élargi à 256 Mo, mais l'échantillonnage le rend non concluant

## 1. Ce qui a été fait

Plage portée de 4 Mo à **256 Mo** (`0xA0000000`–`0xB0000000`), granularité 64 Ko,
somme de contrôle toutes les 60 trames.

## 2. Résultat brut

| condition | blocs modifiés |
|---|---|
| repos (6 s) | `0xA1910000` ×1 |
| appui **Gauche** (4 s) | `0xA1910000` ×4 |

Un seul bloc, présent dans les deux conditions.

## 3. Le défaut que j'ai introduit

Pour tenir le coût, j'ai échantillonné **un mot sur seize** (`kStride = 64`
octets). Conséquence directe : si la sélection tient dans **un seul mot**, la
probabilité qu'il soit échantillonné est d'environ **1/16**.

**Ce test ne peut donc pas conclure.** Une absence de signal est ici l'issue
attendue même si le champ se trouve dans la plage balayée. Je ne tire aucune
conclusion de ce relevé, ni sur la localisation du champ, ni sur l'hypothèse du
cycle 442.

La différence de fréquence — 1 changement au repos contre 4 sous Gauche — n'est
pas exploitable non plus : durées différentes, effectifs minuscules.

## 4. Pourquoi c'est la même erreur, encore

Sixième instrument restreint sur une supposition, et cette fois la restriction
était **la mienne, consciente, pour des raisons de coût**, sans que j'en calcule
l'effet sur la sensibilité avant de lancer.

C'était calculable en une ligne : un mot sur seize, donc 1/16 de chance. Le
faire avant aurait épargné l'exécution.

## 5. Reprise correcte

Deux options, toutes deux sans échantillonnage :

- balayer **densément** une plage réduite, mais choisie d'après la carte mémoire
  réelle et non supposée — lire les plages engagées dans l'allocateur ;
- ou surveiller en deux temps : sommes denses sur 64 Ko une région à la fois,
  en faisant tourner la région entre les captures, jusqu'à couvrir l'espace.

La seconde est lente mais ne repose sur aucune hypothèse et finit par converger.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
