# Cycle 58 — budget et porte de progression du runner Xenia AC6

## Décision

Le runner oracle Xenia ne prolonge plus implicitement une capture noire. Les
captures par défaut restent à 8 s et 20 s. Une seconde capture au-delà du
budget non qualifié de 20 s est refusée sauf si l'expérience fournit une regex
de log qui représente un progrès observé et qualifié.

## Contrat du runner

`run_xenia_ac6_oracle_baseline.sh` exige exactement deux instants de capture,
croissants et positifs. Il écrit avant le lancement `experiment-plan.txt`,
vérifie que Xenia est encore vivant pendant chaque attente, puis écrit un
statut terminal structuré.

Pour dépasser le budget, l'appelant doit fournir les deux paramètres :

```bash
XENIA_AC6_CAPTURE_SECONDS='8 35' \
XENIA_AC6_REQUIRED_LOG_REGEX='marqueur-observe-et-qualifie' \
workspaces/ace-combat-6/scripts/run_xenia_ac6_oracle_baseline.sh \
  artifacts/ac6/xenia-progress
```

La regex est une précondition de l'expérience, pas une preuve de progression :
elle n'est pas assimilée à une transition de mission ni à une cible virtuelle
valide. Si le marqueur n'apparaît pas avant la seconde capture, le runner sort
avec le code 4 et écrit `progress-gate.txt`; il ne poursuit pas une attente
indéfinie.

## Motivation

La baseline antérieure ne montrait pas de progression exploitable entre ses
captures 8 s et 20 s. Augmenter seulement les délais ne résout pas le blocage
actuel : la frontière de traversal conserve des cibles directes
`0x8222ccd0`, `0x8222b740` et `0x82227378` qui doivent être qualifiées par une
trace d'objet/table ou une analyse Xenon valide, pas par une image noire plus
tardive.

## Validation statique

Les validations suivantes ont été exécutées sans lancer de retail XEX :

```bash
bash -n workspaces/ace-combat-6/scripts/run_xenia_ac6_oracle_baseline.sh
shellcheck workspaces/ace-combat-6/scripts/run_xenia_ac6_oracle_baseline.sh
XENIA_AC6_CAPTURE_SECONDS='8 21' \
  workspaces/ace-combat-6/scripts/run_xenia_ac6_oracle_baseline.sh /tmp/ac6-invalid
XENIA_AC6_UNGATED_MAX_SECONDS=zero \
  workspaces/ace-combat-6/scripts/run_xenia_ac6_oracle_baseline.sh /tmp/ac6-invalid
XENIA_AC6_CAPTURE_SECONDS='8 20 35' \
  workspaces/ace-combat-6/scripts/run_xenia_ac6_oracle_baseline.sh /tmp/ac6-invalid
```

Les trois dernières commandes ont refusé leur configuration avec le code 2.
Cette modification borne une future expérience; elle ne produit pas de
nouvelle preuve de runtime et ne résout pas encore le blocage de traversal.
