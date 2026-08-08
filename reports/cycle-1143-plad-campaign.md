# Cycle 1143 — `PLAD` sur les quinze missions : le contrôle que le cycle 1130 croyait impossible

Date : 2026-08-08. Le premier pas du plan post-JF.

## Qualification

- Archives retail : `game-files/DATA00.PAC`, `DATA.TBL`, entrées DPL **9 à 23**
  — les quinze missions de la campagne (table `DAT_82065840`, cycle 1104).
- Extraction : `tools/extract_ac6_pac.py --indices 9 … 23 --decompress`, puis
  `tools/ac6_fhm.py`. **Aucune archive versionnée**, conformément à
  `retail_assets_remain_external`.
- **Statique seul.** Aucun oracle.

## La correction, d'abord

Le cycle 1130 écrivait :

> « Les quatre autres entrées extraites ne sont pas des missions, et **les
> archives retail ne sont pas dans cet espace** — c'est la politique du dépôt,
> pas un oubli. Le contrôle attend donc une extraction que ce cycle ne peut pas
> faire. »

**C'est faux.** `game-files/` contient `DATA00.PAC` (2,27 Go), `DATA01.PAC`,
`DATA.TBL` et `default.xex` au SHA-256 qualifié, et `disc-image/` l'ISO complète.
Le cycle 1130 avait vérifié depuis un sous-répertoire et conclu de l'absence
d'un chemin relatif à l'absence des fichiers. Le contrôle était faisable depuis
le début ; il est fait ici.

## Le contrôle

Les quinze missions ont toutes un enfant 2 de magie `PLAD`. Trente-trois
enregistrements au total :

| enregistrements | missions |
| ---: | --- |
| 1 | 1, 2, 9, 12, 14, 15 |
| 3 | 3, 4, 5, 6, 7, 8, 10, 11, 13 |

Et **trente-trois positions distinctes**, toutes à l'échelle du monde :

```
M01  (-2025,  1500,   1345)
M02  ( 5008,     0,  35520)
M03  (-3244,  1363,  16320) ( 1104, 1363, 16208) (-7352, 1363, 16000)
M06  (-30848,  800,  30080) (    0,  800, 40000) (30000, 1000, 30000)
M13  (-43692, 1168,   6385) (17875, 1684,-33316) (-2144,  280, 23582)
…
```

`analysis/plad-campaign.tsv` porte les trente-trois.

**L'hypothèse du cycle 1130 tient** : `PLAD` est une donnée de mission, une par
mission, portant des positions monde. Ce n'était ni une coïncidence ni un
artefact de la Mission 01.

## Ce que la campagne apprend en plus

**Le compte n'est pas décoratif.** Neuf missions déclarent **trois**
enregistrements, six en déclarent **un**. Trois est exactement le nombre
d'opérations simultanées entre lesquelles la Mission 03 et ses semblables font
choisir — le joueur rejoint l'une des trois batailles. Un départ par opération,
et le mot `global+0x4B40` que le chargeur utilise comme index (cycle 1131) est
donc **le choix d'opération**, pas un numéro de manette.

**Le quatrième mot n'est pas l'indice du tableau.** Les trois enregistrements de
la Mission 03 portent `0, 2, 3` — pas `0, 1, 2`. Sur les quinze missions il
prend les valeurs `{0, 1, 2, 3}`. C'est donc un identifiant, cohérent avec la
lecture du cycle 1131 — le chargeur le range en `+0xF0`, le curseur de route —
mais qui **ne peut pas** être un simple rang.

**Les altitudes sont plausibles et variées** : de `0` à `9000`. Deux missions
démarrent à `y = 0` (M02, M14), ce qui n'est pas une altitude de vol ; les
autres sont entre 200 et 9000.

## Ce que cela n'établit pas

- **Que les trois flottants soient lus.** Le cycle 1131 a montré que les trois
  chargeurs de mission n'en lisent que le quatrième mot. Ce cycle montre que les
  triplets sont réels, variés et par mission — pas qui les consomme.
- **Ce que `word3` indexe.** `{0,1,2,3}` avec un trou à la Mission 03 : c'est
  compatible avec un indice d'entrée de route et avec un identifiant
  d'opération, et rien ici ne tranche.
- **Pourquoi deux missions démarrent à `y = 0`.**

## Décision de cycle

Rien n'est porté. `PLAD` n'entre dans le produit natif que lorsque son
consommateur des trois flottants est trouvé — sans quoi ce serait une position
posée à la main, exactement ce que `position_placeholder` évite de prétendre.

`ctest 24/24`, la porte JF reste verte.
