# Cycle 1532 — les producteurs de compteurs authorés sont bornés

## Résultat

Le lecteur de scénario conserve maintenant la localisation exacte
`unité/act/ordre` de chaque `OrderFlagBin` et refuse tout identifiant supérieur
ou égal à la capacité de compteurs déclarée par son propre payload. Les
conditions de sous-mission tag 7 non sentinelles sont soumises à la même borne ;
leur cible doit désigner une sous-mission existante.

Cette tranche est un census statique. Elle n'exécute aucun ordre et n'invente
ni producteur temporel, ni signification de compteur, ni progression de
mission.

## Corpus PAL quinze missions

Le nouveau test store-backed ouvre les entrées campagne 9 à 23 par
`RetailMissionBundle`, puis joint chaque producteur tag 6 au record d'ordre de
même localisation. Il retrouve exactement :

```text
capacités : 339 104 168 219 423 335 297 326 176 213 403 234 471 274 190
producteurs: 232  11   4  13 198  10 246 257  16   6 128  10 213 2135 171
max ID    : 332  97  45  84 315 269 262 320 144 104 207 130 468  273 189
total     : 3650 producteurs, tous localisés et strictement bornés
```

M01 déclare une capacité de 339 slots, mais cela ne signifie pas que 339
compteurs sont compris. Ses 232 producteurs couvrent 133 identifiants distincts
entre 45 et 332, avec 231 opérations set et une opération add. M07 est la seule
mission du corpus à porter des conditions tag 7 : `263,264,97,97`, toutes
bornées ; les quatorze autres n'en portent aucune.

## Validation

```text
build counter/parser/mission-state/session                  pass
corpus AC6_RETAIL_CACHE qualifié                           15/3650
CTest counter + parser + mission-state + session           4/4
contrôle synthétique ID capacité-1 accepté, capacité rejeté pass
micro-exécution ou apply()                                 aucun
git diff --check                                           pass
```

## Frontières restantes

La lane objectifs/campagne reste ouverte. Le scheduler Set/Act/Order, les
événements IA/combat, les consommateurs live, les zones/transitions et la
signification des slots ne sont pas encore raccordés. Les 206 slots M01 sans
producteur scénarisé identifié restent explicitement non attribués.
