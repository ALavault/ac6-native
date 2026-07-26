# AC6 — vtable concrète et slots du worker NDXR

> Qualification importante : le cycle 142 rétrograde la liaison entre cette
> table et les owners dynamiques du worker en `cross-match`/`unknown`. Les
> contrats locaux et les mots de table ci-dessous restent valides, mais la
> contradiction entre le champ 9 bits formé dans `r4` et le déréférencement de
> la feuille doit être résolue avant de parler d'un record pointer ou d'un
> producteur `r5` prouvé. Voir `cycle-142-ndxr-vtable-provenance-qualification.md`.

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Cette passe reste statique, headless et en lecture seule. Elle suit le même
owner `r31` identifié au cycle 140 et ne modifie aucun export ni aucun runtime.

Les vérifications utilisent :

```bash
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript ReferencesTo.java 0x8205c980

./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpDataWords.java 0x8205c980 96

./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x82101b80 0x82101d20

./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x82100280 0x82100400
```

## Adresse de vtable

Le constructeur/initialiseur observé autour de `0x820f9dc8` calcule puis écrit
la valeur suivante à l'offset zéro de l'owner :

```text
lis  r11,-0x7dfa        ; 0x82060000
subi r11,r11,0x3680     ; 0x8205c980
stw  r11,0x0(r3)
```

`ReferencesTo.java 0x8205c980` retrouve l'écriture à `0x820f9dfc`. Le pointeur
`0x8205c980` est donc une table/vtable concrètement observée pour cet
initialiseur. La provenance de cette instance par rapport aux owners
dynamiques du worker reste à corréler.

## Slots utilisés par le worker

Le dump big-endian de la table donne :

```text
vtable + 0x5c = 0x8205c9dc -> 0x82101be0
vtable + 0x13c = 0x8205cabc -> 0x821002f0
```

Les deux calculs correspondent aux dispatchs déjà observés :

- les appelants `0x820fbbd4` et `0x820fcf3c` demandent le slot `+0x13c` ;
- le worker `0x82105bb8..0x82106354` demande le slot `+0x5c` ;
- la table candidate contient aussi ces deux méthodes appelantes à `+0x10c` et
  `+0x110`, ce qui fournit un cross-match de famille mais pas une preuve de
  vtable dynamique utilisée à l'exécution.

## Contrat du slot `+0x5c`

`0x82101be0` est une feuille de quatre instructions :

```text
82101be0  lhz r11,0x1c(r4)
82101be4  add r11,r11,r4
82101be8  lwz r3,0x8(r11)
82101bec  blr
```

Son contrat local prouvé est donc :

```text
input : r4 -> enregistrement avec offset big-endian à +0x1c
result: r3 = mot 32 bits à (r4 + u16be(+0x1c) + 0x8)
```

Le worker transmet bien le résultat de son dispatch indirect à
`0x822c2148` en `r5`, mais la liaison de ce dispatch avec cette feuille
particulière n'est pas encore prouvée. La nature de l'enregistrement et du mot
chargé reste inconnue ; le champ 9 bits préparé dans `r4` doit d'abord être
réconcilié avec le déréférencement `r4+0x1c`.

## Contrat du slot `+0x13c`

`0x821002f0` effectue :

```text
821002f0  lwz r11,0x8(r3)
821002f4  rlwinm r11,r11,0x0,0x5,0x3
821002f8  stw r11,0x8(r3)
821002fc  blr
```

Avec `MB=5` et `ME=3`, le masque tournant conserve tous les bits sauf le bit
4. Le contrat retenu est donc `clear_bit_4_at_owner_plus_8`, sans lui donner
un nom de mode ou d'état de jeu.

Les deux appelants exécutent ce mutateur avant d'entrer dans le worker. Cela
prouve une préparation d'un champ de contrôle à `owner+0x8`, mais ne prouve pas
qu'il s'agit d'un état graphique, de scène, d'avion ou de vol.

## Révision du niveau de preuve

`confirmed` localement :

- table/vtable `0x8205c980` écrite par l'initialiseur observé ;
- slot `+0x5c` vers `0x82101be0` ;
- slot `+0x13c` vers `0x821002f0` ;
- formule locale de la feuille `0x82101be0` ;
- effacement du bit 4 à `owner+0x8`.

`unknown` :

- classe nominale de la vtable ;
- vtable effectivement chargée par les owners dynamiques du worker ;
- nature des enregistrements adressés via `+0x1c` ;
- sens métier du mot à `record+0x8` ;
- relation entre ce mot et le `r5` des trois appels ;
- relation finale avec NDXR, draw, scène ou vol.

## Validation native

La suite native AC6 déjà présente a été rejouée après cette analyse
documentaire :

```bash
ctest --test-dir .build/ace-combat-6/native --output-on-failure
```

Résultat : **41/41 tests passés**, aucun échec, durée 16,54 s. Cette validation
ne transforme pas les contrats statiques en preuve de parité retail ; elle
confirme seulement qu'aucun code natif existant n'a été régressé.

## Suite statique

Suivre les writers des enregistrements référencés par `r4`, puis comparer le
mot retourné à `0x82101be0` avec les champs passés aux trois appels
`0x822c2148`. Tant que cette corrélation n'est pas faite, conserver les noms
`record_offset_0x1c`, `record_word_0x08` et `owner_flag_bit_4`. Aucune session
humaine n'est requise.
