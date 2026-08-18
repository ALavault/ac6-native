# La `D3DDevice` publique s'arrête à `0x2A70`, et tous nos offsets sont après

Date : 2026-08-18

## Correction de `b8482726`

J'y ai écrit que nommer les offsets restants était « un travail borné, avec un
contrôle déjà en main ». Le travail était borné ; le résultat n'est pas celui
annoncé, et le contrôle que je proposais ne validait pas ce qu'il fallait.

## La disposition calculée

`tools/compute_d3ddevice_layout.py` calcule la structure depuis `d3d9.h` et
`d3d9gpu.h` du XDK 6132.6 :

```text
+0      m_Pending                    40    (UINT64 m_Mask[5])
+40     m_Predicated_PendingMask2     8
+48     m_pRing                       4
+52     m_pRingLimit                  4
+56     m_pRingGuarantee              4
+60     m_ReferenceCount              4
+64     m_SetRenderStateCall        404    (D3DRS_MAX/4 pointeurs)
+468    m_SetSamplerStateCall        80    (D3DSAMP_MAX/4)
+548    m_GetRenderStateCall        404
+952    m_GetSamplerStateCall        80
+1152   m_Constants                9120    aligné 128
+10272  m_ClipPlanes                 96
+10368  m_DestinationPacket          64
+10432  m_WindowPacket               12
+10444  m_ValuesPacket               84
+10528  m_ProgramPacket              20
+10548  m_ControlPacket              48
+10596  m_TessellatorPacket          84
+10680  m_MiscPacket                152
+10832  m_PointPacket                32
        taille totale             10864    (0x2A70)
```

## Trois contrôles, tous passés

- `m_pRing` tombe à **+48** et `m_pRingGuarantee` à **+56**, exactement les deux
  champs que `sub_821C57D0` charge, compare, puis met à jour après avoir écrit
  quatre mots — l'ajout d'anneau D3D. Ces deux-là précèdent tout tableau : ils
  ne valident que le préfixe.
- `m_Constants` est `__declspec(align(128))` et tombe à **+1152**, multiple de
  128. Celui-ci, lui, exerce bien les quatre tableaux de pointeurs devant lui.
- Aucun paquet ne se lit comme vide.

L'identification de l'objet comme `D3DDevice` est donc confirmée, pas
supposée.

## Le résultat

**Tous** les offsets que cette campagne manipule sont au-delà de la fin :

```text
10908 (0x2A9C)  au-delà de 10864 — soit privé + 44
10941 (0x2ABD)  au-delà
13216 (0x33A0)  au-delà
14872 (0x3A18)  au-delà
16536 (0x4098)  au-delà
21508 (0x5404)  au-delà   <- le verrou de publication
21600 (0x5460)  au-delà
22264 (0x56F8)  au-delà
```

L'en-tête livré déclare la **partie publique** du device. La queue privée
commence à `0x2A70`, et c'est là que vit tout ce que nous mesurons. Le SDK ne
les nommera pas.

## Ce que cela vaut quand même

- L'objet est identifié avec certitude, et sa partie publique est nommée
  intégralement — dont `m_pRing`, `m_Constants` et les huit paquets GPU.
- L'espoir « le XDK va nommer le verrou de publication » est fermé, ce qui vaut
  mieux qu'ouvert : plus personne n'y passera de temps.
- L'outil reste utile : tout offset inférieur à `0x2A70` rencontré plus tard se
  nomme immédiatement.

## Non établi

- La disposition de la queue privée. Rien ici ne la contraint, et la deviner
  serait exactement ce que cette campagne refuse.
- Si la version 6132.6 correspond à celle de la démo. Les trois contrôles sont
  cohérents avec elle ; ils ne l'établissent pas.

## Une note d'instrument

`ugrep`, qui remplace `grep` dans cet environnement, a rendu **zéro ligne** sur
`d3d9gpu.h` pour des motifs littéraux présents 136 fois (`/bin/grep -c define`
en trouve 136). Il n'a pas signalé d'erreur : il a répondu vide. Deux
recherches successives ont donc failli conclure que les constantes GPU
n'étaient pas livrées avec le SDK. Sur ce fichier, utiliser `/bin/grep`.
