# Cycle 984 — correspondance campagne `selector → DPL`

## Contrat

Le catalogue doit séparer la sélection de campagne, l’identifiant DPL et la
résolution physique `DPL → DATA.TBL`. La première correspondance est qualifiée
par le binaire; la seconde ne doit pas être extrapolée.

## Preuve binaire

Pour le mode campagne 1, la fonction `0x821B6E58` du projet Ghidra canonique
`ace-combat-6` charge la table XEX `0x82065840` dans le module `default.xex`.
Sur le `default.xex` PAL
(`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`), les
entrées observées sont:

```text
selector  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
DPL       9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
```

Le code borne les sélecteurs supérieurs à 15 vers le slot 0 de la table
(`0x33` dans le corpus); cette borne n’est pas une identité de mission
physique. Le catalogue conserve donc
`DPL_to_DATA.TBL_route` comme frontière inconnue.

## Catalogue et runtime

Les entrées 3–15 portent désormais leur `campaign_selector` et leur
`dpl_resource_id`, mais restent `partial`; `data_table_entry_index` est nul,
le parse est `not_attempted` et les lacunes sont explicites. La mission 2
reste partielle pour sa qualification interactive. Le générateur du manifeste
natif ne consomme que la mission 1 `qualified`; aucune route physique n’est
créée par extrapolation.

## Validation

```text
python3 tools/audit_campaign_catalog.py reports/ac6-pal-campaign-catalog.json --xex game-files/default.xex --data-tbl game-files/DATA.TBL
python3 tools/generate_campaign_manifest.py reports/ac6-pal-campaign-catalog.json reports/ac6-native-gameplay-definitions.json /tmp/ac6-qualified-campaign.tsv
python3 tools/audit_code_reachability.py reports/ac6-code-reachability-inventory.json --xex game-files/default.xex
```

Résultats:

```text
cmake --build build -j2                         PASS
CTest sous Xvfb + SDL_AUDIODRIVER=dummy        100% tests passed, 0 failed, 5/5
campaign_catalog=pass missions=15 qualified=1 partial=14 unqualified=0
campaign_manifest=pass qualified=1
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```
