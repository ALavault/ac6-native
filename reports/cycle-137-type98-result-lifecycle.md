# AC6 — cycle de vie statique du résultat type `0x98`

Date : 2026-07-17 (Europe/Paris)

## Cible et périmètre

Cette passe concerne le `default.xex` Xbox 360 PAL qualifié, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

L'analyse est headless et en lecture seule. Elle ne lance ni Xenia, ni Wine, ni
GUI, et ne modifie ni les sorties générées ni la base Ghidra. Elle poursuit le
cycle 136 en suivant le résultat créé par le service global et écrit à l'objet
du constructeur à l'offset `+0x15c`.

## Résultat principal

Le résultat type `0x98` n'est pas seulement écrit puis abandonné. Le flux
statique est maintenant borné ainsi :

```text
MDLP element(s)
      │
      ├─ service vtable +0x18 / +0x1c (passes de préparation)
      │
      ├─ reserve_or_cursor(0x120, 0x10) -> result
      │                                  │
      │                                  └─ owner +0x15c
      │
      └─ service vtable +0x10(result, context_1, context_2, context_3)
         puis lecture bornée d'une vue de table et post-traitement
```

Cette chaîne prouve une phase de création/attachement et de post-traitement
d'un objet de ressource. Elle ne prouve toujours pas un upload GPU, un draw,
un nœud de scène ou une identité d'aéronef.

## Helper de réserve/cursor `0x820ab4b0`

La décompilation headless de `0x820ab4b0` est :

```c
if (service->field_04 != 0 && *service->field_04 != 0) {
    result = Function_822283E8();
    Function_82228DA8(service, 0x120, 0x10);
    return result;
}
return Function_82226498(DAT_82758F30, 0x120, 0x10, 0);
```

Le contrat sûr est `reserve_or_cursor(size=0x120, alignment=0x10)`, avec une
voie cursor lorsque le service possède son pointeur de stockage et une voie de
secours globale sinon. Le nom d'allocateur n'est pas promu comme abstraction
runtime : `0x82226498` reste partiellement découpée par le prologue partagé.

## Écriture de `owner+0x15c`

Dans le corps compilateur-split autour de `0x820abd80`, le flux :

1. prépare deux vues bornées à partir de tables relatives via
   `0x822383d0` ;
2. appelle `0x820ab4b0` à `0x820abe14` ;
3. écrit immédiatement le résultat dans `owner+0x15c` à `0x820abe20` ;
4. teste ensuite l'octet faible d'un résultat de phase dans `r22`.

Ce test est une condition de contrôle confirmée, mais son sens métier reste
inconnu. Il ne doit pas être appelé `loaded`, `visible` ou `aircraft_ready`.

## Réinjection dans le service

Lorsque l'octet faible de `r22` est non nul, le flux appelle quatre fois le
service global `0x826a0728` :

```text
0x820abe30  slot +0x18 (context pointer A, context pointer B)
0x820abe4c  slot +0x18 (context pointer A, result-table pointer)
0x820abe68  slot +0x1c (context pointer A, context pointer B)
0x820abe84  slot +0x1c (context pointer A, result-table pointer)
```

Les arguments en `r5`/`r4` sont des pointeurs issus des vues locales, pas le
littéral `0x98`. Cela distingue cette phase de préparation de la première passe
du chargeur `0x820a7070`, où `r4` est l'élément MDLP et `r5` vaut explicitement
`0x98`.

À `0x820abeb0`, la même vtable reçoit ensuite :

```text
r3 = service object
r4 = owner+0x15c
r5 = context pointer
r6 = result-table pointer
r7 = context pointer
call service->vtable[+0x10]
```

Cette instruction est la preuve statique la plus forte d'une réutilisation du
résultat stocké à `+0x15c` par le service. Elle ne suffit pas à nommer la
méthode `attach_mesh`, `register_aircraft` ou `submit_draw`.

Lorsque la condition de `r22` est nulle, le chemin ne lance pas ces quatre
passes et positionne à `1` l'octet `service+0x1c` à `0x820abea8`. Cette voie
alternative est conservée comme état de service, sans interprétation.

## Post-traitement de la vue de table

Après le dispatch `+0x10`, le flux utilise :

- `0x822383d0(view, 3)` pour récupérer une entrée bornée ;
- `0x82238408(view, 3)` pour récupérer une entrée auxiliaire ;
- `0x821bab90`, `0x82286210`, `0x822a9690`, `0x822a1258` et `0x820ac918`
  pour le traitement qui suit.

Ces appels sont qualifiés comme post-traitement d'index 3 et non comme appels
de rendu. Une entrée absente ou une longueur nulle prend une branche de sortie
bornée.

Les helpers associés au parcours de records ont également un contrat précis :

- `0x821bf8f0` pose le bit haut d'un descripteur et convertit les offsets
  relatifs en pointeurs lorsque le bit de représentation l'exige ;
- `0x823330f0` effectue l'opération inverse pour un tableau marqué, en ramenant
  les pointeurs vers des offsets relatifs et en nettoyant les flags temporaires.

Ils établissent une normalisation de tables en mémoire. Ils ne démontrent pas
la nature visuelle ou gameplay de leur payload.

## Fin de phase et nettoyage

Le fragment `0x820ab400..0x820ab478` termine la traversée des éléments, puis
réinitialise `service+0x04` et `service+0x08` à zéro. Cette écriture est
compatible avec la libération d'un cursor temporaire, mais ne permet pas de
conclure à une destruction de l'objet `owner+0x15c`.

## Frontière de confiance

`confirmed` :

- réserve/cursor `0x120` alignée sur `0x10` ;
- écriture du résultat à `owner+0x15c` ;
- condition de branche par l'octet faible de `r22` ;
- quatre appels de préparation `+0x18/+0x1c` ;
- réinjection de `owner+0x15c` dans le slot `+0x10` ;
- post-traitement borné de la vue à l'index 3 ;
- nettoyage final des champs cursor `+0x04/+0x08`.

`unknown` :

- type C++ concret de l'objet résultat ;
- correspondance avec un NDXR, MATE, NTXR ou Scene précis ;
- effet des helpers postérieurs sur le renderer ;
- relation avec un aéronef, une caméra, une mission ou une soumission de draw.

La frontière dynamique est donc réduite à l'attribution métier/runtime. Elle
ne bloque pas la poursuite statique ni le travail sur les autres cibles.

## Validation

- `analyzeHeadless` en lecture seule avec `DecompileAt.java` et `DumpRange.java` ;
- exports persistants et journaux `820a7070-range.log`,
  `820abd80-service-init.log`, `820ab400-service-range.log` ;
- CTest AC6 : **41/41 PASS** sur le dernier run ;
- aucune session humaine, Xenia, Wine, VNC ou GUI.

## Suite

Suivre statiquement les fonctions postérieures à `0x820abf08` et les writers de
la vue d'index 3, en conservant les noms offset-qualified. Une session Xenia
ne sera utile que si elle devient nécessaire pour départager l'effet visuel ou
métier de ce résultat ; elle n'est pas requise pour ce front.
