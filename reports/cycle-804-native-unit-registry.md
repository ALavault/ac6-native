# Cycle 804 — registre d’unités natif

Ajout de `UnitRegistry` avec `EntityId`, owner, asset ID et activation.
L’enregistrement rejette les IDs nuls, l’auto-ownership et les doublons ;
l’activation d’une unité absente échoue sans effet de bord.

Validation : build CMake et CTest `1/1` réussi, avec contrôles positifs et
négatifs.
