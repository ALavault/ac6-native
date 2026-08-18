# L'objet « device » est le `D3DDevice` du XDK, et le SDK en donne la structure

Date : 2026-08-18

## Le problème posé autrement

Depuis plusieurs itérations, la frontière de rendu se lit en offsets bruts d'un
objet en `0x10041A00` : `+10908`, `+10941`, `+13216`, `+14872`, `+14876`,
`+14888`, `+14916`, `+16536`, `+16540`, `+16544`, `+21508`, `+21600`, `+22264`.
Chacun a été deviné par son usage, un par un.

## L'identification

`VdSetGraphicsInterruptCallback` est appelée une fois avec
`r3 = 0x821B9710`, `r4 = 0x10041A00` : l'objet est le contexte du callback
d'interruption graphique. Dans le D3D du XDK, ce contexte est le `D3DDevice`.

La structure `D3DDevice` est **ouverte** dans
`sdk/xdk-xenon-6132.6/XDK/include/xbox/d3d9.h` (lignes 1442-1764) : après les
méthodes viennent les membres de données.

```c
D3DTAGCOLLECTION m_Pending;              // UINT64 m_Mask[5]  -> 40 octets
UINT64           m_Predicated_PendingMask2;
PRING            m_pRing;
PRING            m_pRingLimit;
PRING            m_pRingGuarantee;
DWORD            m_ReferenceCount;
```

d'où, arithmétiquement :

```text
+0   m_Pending                  40 octets
+40  m_Predicated_PendingMask2   8
+48  m_pRing
+52  m_pRingLimit
+56  m_pRingGuarantee
+60  m_ReferenceCount
```

## Le contrôle

Cette prédiction est vérifiable sur du code déjà lu. `sub_821C57D0` contient :

```c
r10 = [r31 + 56];                     // m_pRingGuarantee
r11 = [r31 + 48];                     // m_pRing
if (r11 > r10) sub_821BA780(r31);     // au-delà de la garantie -> en obtenir
... quatre stw successifs, r11 += 4 à chaque fois ...
[r31 + 48] = r11;                     // m_pRing avancé
```

C'est **exactement** l'ajout de commandes dans l'anneau D3D : écrire des mots,
avancer `m_pRing`, et redemander de la place si l'on dépasse
`m_pRingGuarantee`. Les deux offsets tombent juste sans qu'on les ait choisis
pour cela : ils sortent de la somme des tailles déclarées par l'en-tête.

L'identification n'est donc pas une ressemblance de noms.

## Ce que cela ouvre

Les offsets restants sont au-delà de `m_ReferenceCount`, dans les grands
tableaux d'ombre (`m_SetRenderStateCall`, `m_Constants`, `m_ClipPlanes`, puis
les `GPU_*PACKET`). Les nommer demande de **calculer la disposition complète**
à partir de `d3d9.h`, `d3d9types.h` et `d3d9gpu.h` — un travail borné, avec un
contrôle déjà en main : la disposition calculée doit reproduire 48 et 56.

Cela vaut pour tous les champs que cette campagne a devinés, dont
`+21508` — le verrou de publication mesuré dans `5da91f72` — et `+21600`, que
la campagne a longtemps pris pour une garde alors que `sub_821C57D0` le lit
comme valeur.

## Non établi

- La disposition au-delà de `+60`. Rien ici ne la calcule, et aucun des offsets
  cités plus haut n'est nommé dans ce rapport.
- Si la version 6132.6 de l'en-tête correspond à celle avec laquelle la démo a
  été compilée. Le contrôle sur 48/56 est cohérent, mais deux offsets ne
  qualifient pas une version ; les grands tableaux dépendent de constantes
  (`D3DRS_MAX`, `D3DSAMP_MAX`) qui peuvent avoir bougé entre XDK.
