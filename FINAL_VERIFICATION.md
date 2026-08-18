# Vérification finale

- [x] Cible : image mémoire PAL de la démo.
- [x] SHA-256 de l'image vérifié.
- [x] Les imports `KeCertMonitorData` et `KeDebugMonitorData` sont joints par leurs descripteurs XEX.
- [x] Le layout 15 × 12 octets de la table de factory est vérifié par son consommateur.
- [x] Les valeurs 17 et 6 sont séparées en deux records adjacents.
- [x] Tous les writers D-form de `device+0x5460` et du masque global sont recensés.
- [x] Le branchement nul à `0x821C5920` est une continuation, pas un retour.
- [x] `0x821ADD90` possède exactement deux callsites directs.
- [x] La table de 17 métriques à `0x823C2EA8` est vérifiée.
- [x] Le vérificateur Python termine avec `status=PASS`.
- [x] Aucun ZIP, XEX, PE, PAC, FHM ou octet propriétaire n'est destiné au commit.
