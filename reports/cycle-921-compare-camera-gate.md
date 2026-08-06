# Cycle 921 — comparaison Mission 01 sans caméra synthétique

Le mode `--compare-mission01` refuse maintenant tout manifeste sans `camera`
qualifiée pour la mission. Il ne peut plus tomber silencieusement sur la
caméra de suivi du `WorldFrame`, qui reste réservée au smoke développeur
`--present-manifest` lorsque le champ est absent.

Validation : CTest normal 3/3, CTest ASan/UBSan 3/3 et package audit pass.
