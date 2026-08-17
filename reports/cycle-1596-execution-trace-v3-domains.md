# Cycle 1596 — trace runtime v3 à six domaines

## Résultat

Le writer C++ `ExecutionTraceJsonlWriter` émet maintenant l’ordre canonique
v3 par tick :

`input`, `simulation`, `objectives`, `graphics`, `media`, `hashes`.

Le domaine `media` est toujours présent, mais porte explicitement
`qualified=false`, une horloge nulle et zéro événement tant que la jointure
RadioTbl → TextData → RIFF/XMA et les sous-titres PAL ne sont pas qualifiés.
Il n’y a donc ni audio ni temporalité inventés. Les hashés couvrent les cinq
domaines de contenu, dont le domaine média.

La v2 reste acceptée par les outils comme entrée historique ; le writer
produit ne l’émet plus. Le contrôle de complétude du replay attend six
événements par échantillon.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j16 \
  --target ac6-execution-trace-tests ac6-retail-replay-trace-cadence-tests ac6-native  pass
CTest -R '^ac6-(execution-trace-tests|retail-replay-trace-cadence)$'              2/2 pass
git diff --check                                                                  pass
```

Cette tranche ferme uniquement le format de trace produit, pas la lane
XMA/ASF ni les gates JV/JP/JG. Aucune lane du checkpoint 2 n’est promue.
