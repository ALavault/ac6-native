# Cycle 956 — catalogue campagne 15 missions

`reports/ac6-pal-campaign-catalog.json` est désormais l’artefact machine-
readable de campagne. Il contient exactement les missions 1–15, les hashes
PAL du XEX et de `DATA.TBL` sur chaque provenance, ainsi que le statut de
qualification, le parse et les lacunes explicites. Les missions sans route
qualifiée portent `null` pour sélecteur/DPL/entrée physique et restent
`unqualified`; aucune extrapolation d’entrée DATA.TBL n’est faite.

L’auditeur `tools/audit_campaign_catalog.py` vérifie schéma, cardinalité,
provenance, routes qualifiées et échecs explicites. Résultat actuel :
`missions=15 qualified=1 partial=1 unqualified=13`. L’inventaire des slices
Mission 01 reste vérifié séparément (`rows=14`).

Ce checkpoint ferme l’artefact et la garde fail-closed, pas la qualification
retail des 13 routes encore inconnues.
