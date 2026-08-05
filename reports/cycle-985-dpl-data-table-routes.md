# Cycle 985 — routes physiques `DPL → DATA.TBL`

## Contrat qualifié

Le catalogue ne doit pas confondre un identifiant DPL avec une entrée physique
par simple égalité numérique. La chaîne binaire qualifiée établit toutefois
une règle précise pour le sous-ensemble direct : `0x821D1128` compare le DPL à
`0x39D`, transmet inchangé tout identifiant inférieur à cette borne à
`0x821CD130`, et la table chargée par `0x821CC250` utilise cet index physique.

Provenance : projet Ghidra canonique `ace-combat-6`, module `default.xex`,
cible `PAL-default-xex`, `default.xex` PAL SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
Le `DATA.TBL` PAL contient 926 entrées et porte le SHA-256
`82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.

## Résultat campagne

La table mode 1 qualifie les sélecteurs 1–15 vers les DPL 9–23. Tous ces IDs
sont dans la branche directe; le catalogue porte donc:

```text
selector       1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
DPL            9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
DATA.TBL       9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
```

Les entrées 3–15 restent `partial`: leur route physique est connue mais leur
payload est encore `not_attempted`, avec `payload_not_decoded` comme unique
lacune de route. La borne `0x39D` reste explicitement inconnue pour les IDs
égaux ou supérieurs; aucune règle d’identité générale n’est publiée.

## Validation

```text
campaign_catalog=pass missions=15 qualified=1 partial=14 unqualified=0
campaign_manifest=pass qualified=1
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
CTest sous Xvfb + SDL_AUDIODRIVER=dummy: 100% tests passed, 0 failed, 5/5
```
