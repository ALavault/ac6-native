# AC6 — workspace publié par le worker NDXR (cycle 155)

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Passe statique, headless et en lecture seule. Les plages du worker,
`0x82222e98` et ses helpers ont été examinées sans modifier le projet Ghidra,
les sorties générées ou le code natif.

Commandes utilisées :

```bash
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x82105ba8 0x82105c98 \
  -postScript DumpRange.java 0x82105e20 0x82105f90 \
  -postScript DumpRange.java 0x82106320 0x82106420 \
  -postScript DumpRange.java 0x82222e70 0x82222f40 \
  -postScript DumpRange.java 0x82221d70 0x82221e60 \
  -postScript DumpRange.java 0x82222a10 0x82222a90
```

## Provenance du pointeur publié

Au début du worker `0x82105bb8` :

- `r18` conserve le contexte reçu en `r3` ;
- `r16` reçoit `context+0x28`, la table de descripteurs déjà qualifiée ;
- `r25` commence comme un curseur sur cette table ;
- chaque groupe avance ce curseur de `0x10` octets.

Après le parcours initial, le worker calcule une taille, impose `r5=0x10` et
`r6=0`, puis appelle :

```text
0x82105eb8  rlwinm r31,r10,0x4,0,0x1f
0x82105ebc  or     r4,r31,r31       ; taille calculée
0x82105ec0  lwz    r3,...           ; objet/service global
0x82105ec4  bl     0x82222e98
0x82105ec8  ...
0x82105ecc  or     r25,r3,r3        ; résultat conservé
```

Le worker réutilise ensuite `r25` comme base de sa nouvelle zone de travail.
Il écrit notamment un en-tête `0x400`, met à zéro une zone à pas de `0x8`,
puis traite la seconde série de descripteurs en écrivant dans cette zone.
L'épilogue publie finalement la même valeur :

```text
0x82106344  stw r25,0x30(r18)
```

La chaîne statique est donc :

```text
table context+0x28
  -> calcul de taille
  -> 0x82222e98
  -> zone de travail retournée en r3/r25
  -> remplissage borné
  -> publication context+0x30
```

`context+0x30` ne doit donc plus être traité comme un simple compteur ou
curseur sans autre qualification. Il reçoit un pointeur de zone de travail
dans ce chemin. Le contenu et le propriétaire de cette zone restent à suivre.

## Qualification de `0x82222e98`

La fonction reçoit quatre registres utiles (`r3` service, `r4` taille,
`r5` alignement ou paramètre de granularité, `r6` drapeau). Son corps :

1. conserve les paramètres dans les registres non volatils ;
2. additionne `r4` et `r5` avant la sélection ;
3. appelle `0x82221d80`, qui classe explicitement les tailles
   `0x10..0x4000` en indices `0..10` ;
4. choisit ensuite entre les chemins `0x82222a20` et `0x822223c0` ;
5. retourne le pointeur résultant en `r3`.

Cette structure est une preuve statique forte d'un service de réservation ou
de gestion de zone mémoire. Elle ne permet pas d'identifier le type C++, le
propriétaire de l'allocation, ni la durée de vie de la zone. Le rapport utilise
donc « service d'allocation / zone de travail » comme description technique,
pas comme nom de gameplay.

## Ce qui est maintenant confirmé

`confirmed` :

- le contexte du worker est `r18` ;
- la table source initiale est `context+0x28` ;
- le résultat de `0x82222e98` est conservé en `r25` ;
- `r25` sert de base aux écritures de la seconde phase ;
- `context+0x30` reçoit `r25` à l'épilogue ;
- `0x82221d80` est une classification de taille à seuils ;
- le worker initialise et remplit une zone de travail avant publication.

`unknown` :

- la structure exacte publiée à `context+0x30` ;
- le consommateur ultérieur de cette zone ;
- le rôle des entrées et compteurs après publication ;
- la sémantique métier des valeurs quantifiées du cycle 154.

## Suite statique

Suivre les lectures de `context+0x30` et les accès à la zone retournée par
`0x82222e98`, en conservant la séparation entre l'objet NDXR, le service
d'allocation et les consommateurs. Une session Xenia ou une intervention
humaine n'est pas nécessaire tant que ces références restent résolubles dans
le binaire headless.
