# AC6 `ac6-demo-native` domaine 1 — checkpoint cycle 1764

Verdict : **DOMAIN1-GO / DOMAIN2-NO-GO**, `supported=false`.

La publication du store PAL démo est fermée pour l’identité exclusive
`ac6-demo-xbox360-pal`, `Default.xex` SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
Le retail `acc302c1…bcde`, les projets Ghidra historiques et les sorties C++
générées ne participent pas à cette preuve.

## Fermetures

- les neuf fichiers sont hachés intégralement avant toute publication de
  `current` ; le nom de génération est recroisé à son inode après le hash ;
- la lecture publiée recroise pointeur, inode, marqueur et SHA avant/après la
  validation ;
- aucun token temporaire n’est supprimé dans une génération et aucun rollback
  ne remplace ou supprime un nom ambigu ;
- l’ancien pointeur possède un lien d’inode vérifié avant l’échange. Une panne
  `fsync` post-échange ou le remplacement du pointeur déplacé restaure
  l’ancien `current` par `RENAME_EXCHANGE` ; une entrée étrangère n’est pas
  supprimée ;
- `import_impl` est séparé en staging et publication CAS, sous le ratchet de
  complexité source.

Commits : `5b30f1d7c9ff57cb76afac779efeee5b31bf1cda`,
`0487efef05d490a1723a907a0ce00b555a85e169` et
`1246fd2b650a508a8498b63628960f29be1b90ce`.

## Validations

```text
cmake --build reconstruction/ac6-demo-native/build -j16          PASS
SDL_AUDIODRIVER=dummy xvfb-run -a ctest ...                      3/3 PASS
audit_cpp_complexity.py --root reconstruction/ac6-demo-native    PASS (11 fichiers)
cmake --install ... --prefix <workspace>; test ! -e bin/bin      PASS
import corpus PAL 9 fichiers / 322 371 032 octets                PASS
verify du store PAL isolé                                        PASS
```

Les tests couvrent publication multi-processus, échecs rename/fsync,
rollback post-échange, swap du nom de génération après SHA, remplacement de
l’ancien pointeur après échange, pointeur corrompu/symlink, VFS borné et
surface de production. Le store PAL temporaire de validation a été supprimé
après succès.

## Frontière restante

Le nettoyage des orphelins est volontairement absent : une opération échouée
ne supprime jamais une entrée par nom ambigu. Un futur GC devra être
descriptor/identité-aware. Cela ne bloque pas le domaine 1.

Le domaine 2 et tous les claims runtime, renderer, frontend, mission,
objectifs, terminal et autonomie restent **NO-GO**. Cette fermeture ne change
pas `supported=false` et ne promeut aucun résultat endogène.
