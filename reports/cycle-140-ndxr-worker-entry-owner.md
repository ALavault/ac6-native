# AC6 — entrée et propriétaires statiques du worker NDXR

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Cette tranche est exclusivement headless et en lecture seule. Elle complète
`cycle-139-ndxr-caller-boundary.md` en recherchant les branches vers l'entrée
du worker, sans attribuer de nom métier au propriétaire.

Commande :

```bash
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript FindPpcBranchesTo.java 0x82105ba8 0x82105bb8 0x82106358
```

Le balayage PPC brut trouve exactement deux branches d'appel vers l'entrée
cataloguée `0x82105ba8` :

```text
0x820fbbd4 -> 0x82105ba8   bl 0x82105ba8
0x820fcf3c -> 0x82105ba8   bl 0x82105ba8
```

Aucun appel direct vers `0x82105bb8` n'est encodé. Le corps observé à
`0x82105bb8..0x82106354` est donc la continuation contiguë de l'entrée
`0x82105ba8`, et non une troisième fonction appelée séparément.

## Réconciliation de la frontière Ghidra

L'export `exports/82105ba8.json` ne conserve que le petit préambule
`0x82105ba8..0x82105baf` et déclare `0x82382ec8` comme appel sans retour. Le
dump brut montre pourtant la séquence contiguë :

```text
82105ba8  mfspr r12,LR
82105bac  bl 0x82382ec8
82105bb0  subi r12,r1,0x88
82105bb4  bl 0x82384448
82105bb8  stwu r1,-0x560(r1)
```

Le corps continue ensuite jusqu'à l'épilogue autour de `0x82106354`. La
frontière d'export est donc un artefact de désassemblage/analyse ; elle ne doit
pas être utilisée pour conclure que `0x82105bb8` est une fonction indépendante.
Cette constatation reste une correction de frontière, pas une identification
de classe.

## Contrat des deux entrées

Dans les deux appelants, l'argument d'entrée est le même registre propriétaire
local :

```text
0x820fbbd0  or r3,r31,r31
0x820fbbd4  bl 0x82105ba8

0x820fcf38  or r3,r31,r31
0x820fcf3c  bl 0x82105ba8
```

Le worker reçoit donc `r31` comme objet `context` dans ces deux chemins.
Avant l'appel, chaque chemin effectue un dispatch par le premier mot de cet
objet ; le vtable effectivement chargé par l'instance reste à corréler avec
les tables statiques :

```text
lwz r11,0(r31)
lwz r11,0x13c(r11)
mtspr CTR,r11
bctrl
```

Le worker réutilise ensuite le premier mot de `context+0x00`, puis demande le
slot `+0x5c` pour le contrat NDXR décrit au cycle 139. Les deux offsets sont
donc des dispatchs de l'interface sélectionnée par l'instance ; leur cible
statique et leur identité métier restent à qualifier tant que la provenance du
vtable dynamique n'est pas fermée.

Après le retour du worker :

- le chemin `0x820fbbd4` écrit le résultat de succès `1` dans `r3`, puis
  initialise quatre flottants à `owner+0x6d50..0x6d5c` et un octet à un offset
  globalement calculé depuis `owner` ;
- le chemin `0x820fcf3c` écrit directement `r3=1` et quitte par son épilogue.

Ces écritures qualifient les deux appels comme des chemins de préparation ou de
mise en place, mais ne prouvent ni scène, ni modèle, ni avion, ni renderer.

## Ce qui est maintenant confirmé

`confirmed` :

- deux appelants directs de l'entrée `0x82105ba8` ;
- même objet d'entrée (`r31`) dans les deux appelants ;
- continuité entre le préambule `0x82105ba8` et le corps `0x82105bb8` ;
- dispatch via le premier mot de l'owner à `+0x13c` avant le worker ;
- dispatch via le premier mot de `context` à `+0x5c` dans le worker ;
- retour de succès consommé par les deux chemins.

`unknown` :

- classe et vtable concrètes de `r31` ;
- type du pointeur retourné par le slot `+0x5c` ;
- relation du traitement quantifié avec NDXR, scène, draw ou vol ;
- sémantique des flottants `owner+0x6d50..0x6d5c`.

## Suite

La suite statique la plus rentable est de résoudre la valeur de vtable chargée
par `r31`/`context`, puis de suivre les writers de `context+0x28`, de `r31` et
de `r4`. Les offsets de l'owner et les résultats quantifiés doivent rester
offset-qualified jusqu'à ce que cette identité soit confirmée. Aucune session
humaine n'est requise.
