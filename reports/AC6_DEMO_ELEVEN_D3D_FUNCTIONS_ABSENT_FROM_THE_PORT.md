# Onze fonctions D3D absentes du port, et une piste qui se dissout

Date : 2026-08-18

## Le chiffre utile

Des 502 fonctions D3D que l'oracle exécute et que le natif n'atteint jamais
(`b93f52dd`) :

```text
491  sont bien générées par le port — la couverture de codegen n'est pas en cause
 11  ne sont pas générées du tout
```

Et sur toute la plage D3D, 931 fonctions oracle contre 12 942 fonctions
générées au total : la couverture est quasi complète. Onze trous, pas un
gouffre.

## Pourquoi ces onze manquent

Aucune n'est déclarée dans `confirmed-chunks.toml` ni contenue dans un chunk
déclaré. L'outil du dépôt tranche :

```text
$ python3 tools/check_listing_against_pdata.py … 0x821B6BC8
  0x821B6BC8  no .pdata row -- the table is incomplete here
$ …                                    0x821A7160
  0x821A7160  no .pdata row
```

`.pdata` ne les déclare pas, donc XenonRecomp ne les émet pas. C'est
exactement la situation que `CLAUDE.md` décrit : « la table est incomplète, donc
"pas de ligne `.pdata`" est une vraie réponse ».

## La seule nommée

```text
0x821B6BC8  D3D::ComputeClearColor(DWORD, DWORD, __vector4, DWORD*, DWORD*)
            13/60 dans d3d9.lib ET d3d9i.lib
```

Elle prend un `__vector4` — donc du VMX128, la classe précise de fonctions dont
`CLAUDE.md` note qu'elles sont tronquées ou manquées. Et `ComputeClearColor`
sert à `Clear()`, c'est-à-dire au tout début d'une image.

Les dix autres restent sans nom, à un réglage dont le plancher est pourtant nul
(`b417938f`) : `0x821A379C`, `0x821A59E4`, `0x821A7160`, `0x821AB1F8`,
`0x821ABAE0`, `0x821ABDC8`, `0x821AC5F8`, `0x821CD060`, `0x821CD118`.

## Une piste qui se dissout, dite comme telle

`b93f52dd` retenait `D3D::InitializeApiState` (`0x821C5D90`, 29/60) comme le
seul résidu solide. Poursuite :

- elle **est** générée par le port ;
- son adresse n'apparaît nulle part dans l'image hors `.pdata`, sauf en
  `0x823C37C8`, au milieu de constantes flottantes — une coïncidence de valeur,
  pas une entrée de table ;
- aucun site du C++ généré ne compose son adresse ni ne l'appelle par son nom.

Elle est donc atteinte, dans le jeu réel, par un chemin qu'aucune lecture
statique d'ici ne révèle. Je n'ai pas de suite à lui donner, et le dire vaut
mieux que d'ajouter une cinquième conjecture.

## Ce que je n'ai pas fait, et pourquoi

Déclarer ces onze fonctions comme bornes serait l'étape naturelle. Je m'en
abstiens dans cette itération : `CLAUDE.md` enregistre que l'expansion de
bornes a échoué **quatre fois**, chaque refus devenant une règle, et une borne
posée à la hâte sur une fonction sans ligne `.pdata` — donc sans longueur
déclarée — est exactement le cas où l'étendue doit être établie avant, pas
après.

## Non établi

- L'étendue de chacune des onze, condition de leur déclaration.
- Si l'une d'elles est sur un chemin dont le port a besoin. Aucune trap ne les
  a réclamées, ce qui suggère que non — mais une trap absente n'est pas une
  preuve, puisque le port n'atteint pas non plus leurs appelants.
