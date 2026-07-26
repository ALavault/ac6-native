# AC6 cycle 300 — fermeture `0x82345200` et front `0x82345228`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x82345200` est-elle indépendante, ou une
  coupure interne de `sub_82345100` qui possède le backedge fatal
  `0x82345214 -> 0x82345144` ?

## Preuve headless

Le vérificateur en lecture seule `Verify82345200Boundary.java` passe **41/41**
assertions. Il établit l'unique entrée ABI `0x82345100` et sa frame `0x120`,
l'absence de référence entrante et d'entrée Ghidra à `0x82345200`, ainsi que la
dépendance au scratch `r1+0x50` et aux registres des boucles englobantes. Le
retour de l'appel à `0x821DE898` est consommé dès `0x82345204`. Les backedges
`0x82345214 -> 0x82345144` et `0x82345220 -> 0x8234513C` partagent la même
frame et l'unique épilogue de `sub_82345100`.

Verdict : `0x82345200` est une instruction interne **confirmed**, pas une
entrée PPC indépendante. Le vérificateur enregistre `0x82345228` comme voisin
configuré sans le classifier.

## Patch, rollback et régénération

Une seule pseudo-entrée a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x82345200 = { name = "rex_sub_82345200" }
```

Les entrées `0x82345228`, `0x82345250`, `0x82345260` et `0x823452A8` restent
inchangées.

- hash TOML avant :
  `fd6d05bd12199d792d8f8b16e04cc9f0b19db373c04810864e6bad62922e047f` ;
- hash TOML après :
  `6dd8972ebf79a8244f7a606e9e1d8d78c604ee6233511e7a24573212aef646a0`.

La réinsertion en flux de cette seule ligne reproduit exactement le hash
avant. ReXGlue termine en **13,09 s** et génère **23 322** fonctions. Le
runtime lié a le SHA-256
`35d69600debbc539da729a8ee0619724622a9c5177c99c1293c129e0d596858c`.
Le symbole `rex_sub_82345200` a disparu ; `rex_sub_82345228` demeure. Aucun
fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

L'ancien fatal est fermé. Le smoke atteint désormais :

```text
Unresolved branch from 0x8234522C to 0x82345134
```

La source appartient à l'entrée préservée `0x82345228`. Ce déplacement est une
preuve de progrès du chemin retail, pas une qualification par voisinage : le
prochain audit exact est `0x82345228`, et aucune entrée ultérieure n'est retirée
par inférence.

## Validation native et état

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 GCC `-j16` et corpus : **44/44 PASS** en 33,28 s ;
- build AC6 Clang/probes `-j16` et corpus : **48/48 PASS** en 30,12 s ;
- installation racine et garde `bin/bin` : PASS ;
- fonctions générées : **23 322** ;
- pseudo-entrée `0x82345200` : fermée ;
- frontière runtime : `0x8234522C -> 0x82345134` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune ; GUI Ghidra : non utilisée.
