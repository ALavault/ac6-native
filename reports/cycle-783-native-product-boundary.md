# Cycle 783 — frontière produit native et dérivation canonique LY

Date : 2026-08-04

## Résultat

Le projet Ghidra canonique `ace-combat-6`, XEX PAL SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
confirme que `0x8234D110` copie les axes bruts puis décompose les signes avec
la table `0x8201250C`. Pour LY, `device+0x3E` alimente `device+0x28` (positif)
et `device+0x2A` (négatif). Un scan des déplacements directs est ambigu ;
aucun consommateur gameplay n'est attribué sans probe dynamique ciblé.

La reconstruction possède désormais un target produit séparé
`ac6_product_core` et l'exécutable unique `ac6-native`. Le contrat expose
`MissionAssetDatabase`, `MissionRuntime`, `MissionScenario`, `VulkanRenderer`,
`InputFrame`, `WorldFrame` et des `EntityId` stables. `FixedStepDriver` impose
un pas exact et refuse les retards dépassant sa borne sans muter l'état.

`ac6-native` ne lie ni `ac6_data_table`, ni le Scene shell, ni le HUD et la
physique diagnostics, ni `input_821ce088`. Tant que Mission 01 n'est pas
qualifiée, son lancement échoue explicitement avec le code 2.

## Validation

- build GNU C++ 15.2 : succès ;
- test discriminant produit : succès ;
- CTest complet : 64/64, quatre skips retail/headless attendus ;
- audit `nm` : aucun symbole RexGlue, XAM, xboxkrnl, PPC généré,
  `campaign_flight`, `campaign_hud` ou `input_821ce088` ;
- dépendances dynamiques : libc, libstdc++, libm et libgcc seulement.

## Frontière restante

Le prochain probe doit surveiller les lectures de `device+0x28/+0x2A` pendant
une fenêtre pitch positive, puis retenir seulement le premier lecteur dont la
sortie rejoint l'enfant possédé par `CAce6UnitPlayer`. Aucune physique native
ne doit être raccordée avant ce contrôle positif, son nul et son retour à zéro.
