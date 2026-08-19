# Vérification finale

- [x] Cible PAL et SHA-256 exact de l’image.
- [x] 33 instructions d’ancrage vérifiées.
- [x] 8 fonctions jointes à `.pdata` et hachées.
- [x] Les trois callsites directs de `0x821BA1F8` sont exhaustifs.
- [x] Les deux callsites directs de `0x821BAA78` sont exhaustifs.
- [x] `scratch4/scratch5 → PM4_INTERRUPT → 0x821B9710` est fermé statiquement.
- [x] Le contrat C++ accepte 1/2/4/8/16/32 et un masque multi-bit.
- [x] Les masques zéro et hors plage échouent.
- [x] Le self-test C++ compile avec warnings stricts et passe.
- [x] Le vérificateur Python compile et termine avec `status=PASS`.
- [x] Aucun ZIP ou octet propriétaire n’est destiné au remote.
