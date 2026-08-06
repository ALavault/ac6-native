# Cycle 928 — résolution des boutons combinés

`InputMappingDatabase::resolve` privilégie toujours le masque exact, puis
sélectionne le sous-masque correspondant le plus spécifique. Les commandes
SDL restent donc actives lorsqu’un autre bouton est maintenu, sans ambiguïté
entre plusieurs bindings.

Le test vérifie qu’un binding pause `0x0010` résout aussi l’état combiné
`0x0011`, tandis qu’un état sans sous-masque reste rejeté.

Validations : CTest normal 3/3, ASan/UBSan 3/3, package audit pass.
