# Cycle 912 — paquet Linux et outils hors runtime

CPack installe désormais uniquement :

- `ac6-native` et ses headers ;
- le README ;
- les scripts d'indexation/sealing NDXR, NTXR, manifests et replay sous
  `share/ac6-native/tools`.

Le tarball vérifié ne contient ni PAC/DATA, ni tranche retail, ni oracle,
ni bibliothèque Xbox/Xenia. Le contenu observé est le binaire, trois headers,
README et cinq outils Python.
