# Vérification finale

- [x] `CModeTaskRanking` est distingué des classes CampaignResult et MedalMission.
- [x] Les chaînes OFFLINE/ONLINE et les neuf formats de board sont extraites.
- [x] La table de sept colonnes à `0x82391988` est reproduite exactement.
- [x] Le record générique de 116 octets est séparé des lignes de réponse de 48 octets.
- [x] Le normaliseur de 72 octets et le tableau de dix lignes sont qualifiés.
- [x] Les callbacks de liste et de ligne locale sont cartographiés.
- [x] Les cibles des 16 états écran sont inventoriées.
- [x] L'automate backend asynchrone est borné jusqu'à l'état terminal 14.
- [x] La sémantique incertaine de `row+0x20` reste explicitement partielle.
- [x] L'extracteur reproductible passe sur le PE reconstruit.
- [x] Aucun XEX, PE, PAC, FHM, dump de code ou payload propriétaire n'est inclus.
