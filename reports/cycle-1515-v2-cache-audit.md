# Cycle 1515 — audit v2 de la fermeture hors-ligne

## Delivered

`tools/audit_ac6_retail_content_cache.py` accepte maintenant le format v2
produit par `RetailContentImporter` : pointeur `current` v2, en-tête d’index
avec manifeste média et limite totale de 8 Gio, puis 926 enregistrements
ordonnés. Les anciennes fixtures v1 (17/24 entrées) restent supportées.

Pour un index v2 complet, l’audit exige la fermeture DATA.TBL entière et la
présence des quinze mondes 119–133, en plus des contrats communs et de la
matrice campagne existante.

## Validation

Tests ciblés : `6 passed`.

Sur `/tmp/ac6-retail-v2-smoke` :

```
retail_cache=pass missions=15 blobs=926 \
  bytes=5424368676 \
  index=cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85
```

L’audit vérifie aussi les digests et tailles de chaque blob du cache ; aucune
lecture de PAC n’est nécessaire.
