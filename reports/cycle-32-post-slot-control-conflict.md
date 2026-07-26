# Cycle 32 — objets post-slot et conflit de contrôle AC6 PAL

Pour le XEX PAL qualifié par SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
`Function_8233ABE8` appelle `Function_823462A8` sur trois objets globaux :
`0x82675880`, `0x82675900` et `0x82675980`. Les bases sont espacées de `0x80`.
Chaque appel effectue un dispatch indirect via le slot de vtable `+0x8`, puis
appelle `Function_823461B8`.

L'export de `Function_823462A8` contient ensuite un test du champ objet `+0x8`,
un appel conditionnel et une remise à zéro. Mais l'export de
`Function_823461B8` marque son unique callee `FUN_82382efc` comme sans retour.
Ces deux vues ne peuvent pas toutes deux définir le flux runtime normal. Le
statut de cette frontière devient donc `manual-review`, pas un service typé ni
un hook stable.

Prochaine preuve exacte : sous XenonTests ou Xenia, enregistrer l'entrée et la
sortie de `0x823461b8`, le LR, la valeur objet `+0x8` et l'identité de l'objet
parmi les trois bases. Cette trace décidera si le marquage no-return est erroné,
si le code postérieur est exceptionnel, ou si l'export a une limite incorrecte.
