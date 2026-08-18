# Dix imports de données noyau non patchés, et une frontière à retirer

Date : 2026-08-18

## Ce qu'est réellement `0x82000560..0x820007BC`

Le descripteur d'imports du XEX (`0x000103FF`, offset `0x2844`) déclare deux
bibliothèques :

```text
lib 0  xam.xex        174 adresses
lib 1  xboxkrnl.exe   292 adresses
         dont 151 en 0x82000560..0x820007BC
         dont 141 en 0x82375984..0x823767C4  (enregistrements de 16 octets)
```

La plage que j'avais mesurée hier comme « 151 mots contigus de forme
`0x0001xxxx`, nature non établie » est donc la table des **adresses d'import
xboxkrnl de 4 octets**. Chaque mot vaut `(index de bibliothèque << 16) |
ordinal` tant que le chargeur ne l'a pas remplacé par l'adresse résolue.

Le port n'en patche aucun : `analysis/demo/ac6-demo-import-thunks-v1.json`
compte 228 entrées = 87 (xam) + 141 (xboxkrnl), c'est-à-dire **uniquement les
enregistrements de 16 octets**. Balayage du C++ généré : **67 lecteurs de la
plage, zéro écrivain.**

## Les dix qui comptent

Les noms et les types viennent de la bibliothèque d'import officielle
`sdk/xdk-xenon-6132.6/XDK/lib/xbox/xboxkrnl.lib` (687 enregistrements
d'import courts, chacun portant son ordinal et son type). Sur les 151 slots,
**141 sont de type CODE** — XenonRecomp remplace les appels, leur valeur ne
sert pas — et **10 sont de type CONST**, c'est-à-dire des données dont le
guest lit la valeur :

| Slot | Ord. | Nom | Lecteurs | dont atteints |
|---|---|---|---|---|
| 0x82000608 | 446 | `VdGlobalDevice` | 17 | 4 |
| 0x820006E4 | 89 | `KeDebugMonitorData` | 15 | 5 |
| 0x82000610 | 614 | `KeCertMonitorData` | 11 | 5 |
| 0x820006F0 | 27 | `ExThreadObjectType` | 4 | 3 |
| 0x82000630 | 403 | `XexExecutableModuleHandle` | 3 | 1 |
| 0x8200057C | 344 | `XboxKrnlVersion` | 2 | 1 |
| 0x820005DC | 448 | `VdGpuClockInMHz` | 1 | 1 |
| 0x820005F0 | 449 | `VdHSIOCalibrationLock` | 1 | 1 |
| 0x82000700 | 173 | `KeTimeStampBundle` | 1 | 1 |
| 0x82000638 | 430 | `ExLoadedCommandLine` | 1 | 0 |

Neuf des dix sont lus par du code réellement exécuté. `VdGlobalDevice` est lu
entre autres par `sub_821C64E8` — la fonction que le rapport `infos` nommait
« owner / initialiseur » du chemin de rendu — et par `sub_821C5190`, voisine
immédiate du soumetteur.

## La frontière à retirer

La campagne poursuivait cette chaîne :

```text
service 47, catégorie 2, événement 17, canal 6
→ callback 0x821ADAB8
→ arme device+0x5460
→ réveil du renderer
```

Trois mesures la démontent.

**Un.** Les `bctrl` de `sub_821ADC78` passent tous par
`[[KeDebugMonitorData]] + 24` ou `[[KeCertMonitorData]]`, avec `r3` valant 28,
puis 47, puis 64. Ce ne sont pas des numéros de service du jeu : ce sont des
codes de notification au **moniteur de débogage**. Sur une console de série il
est absent, et ces appels sont censés être sautés.

**Deux.** Le seul site de toute l'image qui compose l'adresse `0x821ADAB8` est
à l'intérieur de cette préparation d'appel — `{2, 0x821ADAB8}` sur la pile,
passé en `r4` avec `r3 = 47`. Aucune table de données ne référence l'adresse.
`0x821ADAB8` est donc un **processeur de commandes du moniteur de débogage**,
pas un callback de rendu.

**Trois.** `sub_821C57D0` lit `[r31+21600]` dans `r7` et l'empaquette dans un
calcul ; il ne le compare pas à zéro et n'en fait pas une garde. « Le
soumetteur lit le drapeau, le trouve nul et repart sans déclencher », que j'ai
publié en `247eee0b`, ne décrit pas ce que fait cette fonction.

## Ce qui reste vrai de `247eee0b`

Les comptes le restent : un seul écrivain de `[device+0x5460]` dans l'image
(`sub_821ADAB8`, non atteint), un seul lecteur (`sub_821C57D0`, atteint 11 863
fois), la file qui se remplit à 163 933 écritures et l'anneau déclenché
seulement au tick 0. C'est l'**interprétation** de ces comptes qui tombe, pas
les comptes.

## Non établi

- Que patcher les dix slots change quoi que ce soit d'observable. Rien ici ne
  le montre ; c'est la prochaine expérience, et elle est bornée.
- Ce qui, sur une console de série, arme `device+0x5460` — puisque ce n'est
  pas le moniteur de débogage.
- Le cas de `xam.xex` : 174 adresses déclarées, 87 enregistrements de
  16 octets, et 87 autres dont l'emplacement n'a pas été localisé ici.
