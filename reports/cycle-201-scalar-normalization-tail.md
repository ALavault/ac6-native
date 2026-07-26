# AC6 cycle 201 — normalisation scalaire du chemin commun

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique headless en lecture seule avec `DumpRange.java` et
`FindDirectCallsTo.java`. Aucun état Ghidra, binaire ou artefact généré n'a
été modifié.

## Helper `0x822667c8`

Le point `0x822667c8` est un sous-entry PPC non borné comme fonction autonome
par Ghidra, mais son contrat brut est stable :

- entrée : `r3 = objet`, `f1 = scalaire` ;
- l'objet de travail est `objet+0x140` ;
- `fabs(f1)` est préparé et écrit à `objet+0x148` ;
- une valeur constante nulle peut remplacer cette magnitude lorsque les bits
  de contrôle de `objet+0x14c` l'imposent ;
- une valeur de signe dérivée de `f1 >= 0` est enregistrée dans les flags de
  `objet+0x14c` ;
- `objet+0x144` reçoit la valeur normalisée utilisée par la suite.

Les masques PPC sont conservés comme opérations de bits : ils ne justifient
pas encore le nom `sign`, `direction` ou `velocity`. La qualification sûre est
`scalar_magnitude_and_flags`.

## Appelants

Deux appels directs sont confirmés :

1. `0x8226e474`, dans le chemin commun après les handlers d'état, transmet
   `record+0x20` seulement lorsque ce flottant est strictement positif. Le
   même chemin avait auparavant calculé `+0x48/+0x4c/+0x50/+0x54` depuis les
   bornes du record.
2. `0x8226a270`, dans une initialisation de l'objet, transmet une constante
   flottante et prépare ensuite d'autres flags et callbacks de service.

Cela établit que le scalaire `record+0x20` possède une normalisation dédiée
avant son usage dans la suite d'état. Cela ne prouve pas qu'il s'agit d'une
vitesse, d'une altitude ou d'un paramètre de vol.

## Relation avec les cycles 199–200

La chaîne statique actuelle est donc :

```text
record +0x00..+0x1c
        │
        ├── 0x82268b28 → bornes +0x28c..+0x298
        ├── état +0x44 → table 0x8226e38c → +0x48..+0x54
        └── record +0x20 > 0 → 0x822667c8 → +0x140/+0x144/+0x148/+0x14c
```

Cette représentation est exploitable pour un contrat de transcription, tout
en maintenant séparés les champs de record, les bornes et les flags de
normalisation.

## Qualification et intervention humaine

- `confirmed` : offsets, branche `record+0x20 > 0`, magnitude absolue,
  stockage `+0x144/+0x148/+0x14c`, appels directs ;
- `cross-match` : normalisation de paramètre dans le même système de
  transition/intervalle ;
- `unknown` / `needs-dynamic-evidence` : sémantique gameplay du scalaire et
  rattachement à un avion/caméra.

Aucune intervention humaine n'est requise. Une trace Xenia ne serait utile
que pour donner un nom gameplay aux flags.

## Validation

- `FindDirectCallsTo.java 0x822667c8` ;
- `DumpRange.java 0x822666b0 0x82266850` ;
- `DumpRange.java 0x8226e460 0x8226e4f0` ;
- `DumpRange.java 0x8226a1e0 0x8226a2b0` ;
- CTest AC6 : gate connu `41/41` ;
- launcher Xenia/Wine : `status=ready`, `release=16e1eb8`,
  `renderer=vulkan`.

