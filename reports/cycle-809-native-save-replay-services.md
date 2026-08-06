# Cycle 809 — services de sauvegarde et replay

Ajout de `SaveStore` (slots non nuls, sauvegarde/reprise par snapshot) et
`ReplayLog` (séquence d’`InputFrame` ordonnée, effaçable). Ces services sont
indépendants du frontend, du renderer et des données guest.

Validation : build CMake et CTest `1/1`, incluant slots invalides/valides et
enregistrement/effacement d’un replay.
