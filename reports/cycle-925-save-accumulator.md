# Cycle 925 — reprise exacte d’un pas fixe partiel

Les snapshots portent maintenant l’accumulateur sub-tick de `MissionRuntime`.
`SaveStore` écrit la version 3 (`AC6SAVE`) avec ce champ, lit toujours les
versions 1 et 2 en initialisant l’accumulateur à zéro, et refuse les valeurs
non finies ou hors intervalle. Le test sauvegarde une session après `1/120 s`,
réécrit le fichier puis vérifie l’égalité exacte après lecture.

Validations : CTest normal 3/3, ASan/UBSan 3/3, package audit pass.
