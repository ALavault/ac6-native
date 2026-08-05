# Cycle 976 — catalogue campagne → manifeste natif

## Source de vérité

Le catalogue PAL versionné reste la seule source des routes retail. Les
objectifs et prérequis sont dans
`reports/ac6-native-gameplay-definitions.json`, explicitement séparés de la
qualification retail pour éviter d’inventer une sémantique à partir d’une
route partielle.

## Générateur

```text
catalog qualified route + gameplay definition
  -> hash de provenance
  -> TSV native campaign
```

Le générateur refuse : catalogue invalide, route incomplète, définition
manquante, définition sans route qualifiée, objectif hors bornes et prérequis
dupliqués/cycliques. Aucun sélecteur ou identifiant DPL n’est extrapolé.

Résultat PAL local : `qualified=1`, ligne `1 1 9 9 2 -`.

## Validation

```text
python3 tools/generate_campaign_manifest.py \
  reports/ac6-pal-campaign-catalog.json \
  reports/ac6-native-gameplay-definitions.json /tmp/ac6-qualified-campaign.tsv
campaign_manifest=pass qualified=1
cmake --build build -j2
DISPLAY=:105 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

La génération est prête pour les futures routes dès qu’elles seront
qualifiées, sans publier les treize inconnues actuelles.
