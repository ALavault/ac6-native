# Cycle 797 — frontière native initiale

Création du produit `reconstruction/ace-combat-6` avec la bibliothèque
`ac6_product_core`, l’exécutable `ac6-native` et un test CTest déterministe.
Le runtime ne synthétise aucun état Mission 01 : il avance un tick et reste
`mission_ready=false` tant qu’un manifeste retail qualifié et un résolveur
d’assets ne sont pas raccordés.

Validation : configuration CMake, compilation complète, CTest `1/1` réussi ;
`ac6-native` termine volontairement avec le code 2 (fail-closed).
