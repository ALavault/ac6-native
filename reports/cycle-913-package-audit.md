# Cycle 913 — audit automatique du tarball

`tools/audit_native_package.py` inspecte le tarball CPack et rejette :

- les archives/artefacts retail (`DATA*.PAC`, XEX, NTXR, oracle, readbacks) ;
- les marqueurs Xbox/Xenia/PPC dans les fichiers binaires.

Le paquet courant passe : `package_audit=pass entries=17`. Le script d'audit est
lui-même livré comme outil, mais ses chaînes de contrôle Python sont exclues du
scan binaire.
