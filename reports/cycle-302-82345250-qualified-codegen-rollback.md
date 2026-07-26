# AC6 cycle 302 — `0x82345250` qualifiée, retrait refusé après échec codegen

## Front exact et preuve

Le listing headless du front `0x8234530C -> 0x8234524C` montre que la cible
précède immédiatement la coupure configurée `0x82345250`. Le contrat
`Verify82345250Boundary.java` passe **28/28** assertions sur
`ac6-xbox360-pal/default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Il établit que `0x82345250` n'a ni référence entrante ni entrée Ghidra, dépend
de la frame `0x120` et des compteurs initialisés dans `sub_82345100`, et se
trouve dans la boucle fermée par `0x8234530C -> 0x8234524C`. La pseudo-entrée
est donc **confirmed**. Les entrées `0x82345260`, `0x823452A8` et `0x82345300`
ne sont pas qualifiées par cette preuve.

## Échec et rollback

Le retrait expérimental de la seule ligne `0x82345250` produisait le hash TOML
`5a355eb5877e1b2e71e487353d619b835f6e8cca65fd2e78c4811488a44af9cf`
et annonçait 23 320 fonctions, mais ReXGlue a terminé par :

```text
terminate called after throwing an instance of 'std::bad_alloc'
what(): std::bad_alloc
```

La commande avait vidé partiellement `generated/`; ce résultat n'est donc pas
accepté. `0x82345250` a été réinsérée, restaurant exactement le hash cycle 301
`0593b784d34b610177d6146ed53af45d0f18e0939ec021579966cc792fbaa841`.
Une régénération complète réussit avec 23 321 fonctions. Après reconfiguration
CMake explicite, le runtime cycle 301 relie de nouveau avec le SHA-256
`63f5ca2d0c164cfb868acc20ff761ed67d09cb371f14f57c9780f24435919132`.

## Décision

- changement fonctionnel accepté : aucun ;
- état effectif : cycle 301 restauré ;
- frontière runtime : `0x8234530C -> 0x8234524C` ;
- blocage exact : retrait qualifié mais non validable dans ce cycle à cause du
  `std::bad_alloc` ReXGlue ;
- prochaine action : rejouer uniquement cette régénération dans un processus
  surveillé, puis build/smoke/tests avant d'accepter le retrait ;
- intervention humaine : aucune.
