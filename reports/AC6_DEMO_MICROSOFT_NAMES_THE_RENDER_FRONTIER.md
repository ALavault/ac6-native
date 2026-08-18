# Microsoft nomme la frontière de rendu

Date : 2026-08-18

## Le levier

La démo lie le D3D du XDK **statiquement**. Les fonctions que cette campagne
appelle `sub_XXXXXXXX` dans la famille graphique sont donc du code Microsoft,
et `sdk/xdk-xenon-6132.6/XDK/lib/xbox/d3d9*.lib` en contient les **noms**, dans
les tables de symboles COFF des archives.

`tools/name_xdk_library_function.py` fait la jointure par octets : il glisse
plusieurs fenêtres depuis l'image invitée, les cherche dans chaque archive, et
remonte au symbole englobant. Les champs relogés diffèrent entre la
bibliothèque et l'image liée — cibles de branchement, moitiés `lis`/`addi` —
donc une fenêtre isolée échoue souvent quand ses voisines réussissent. L'outil
publie **le nombre de fenêtres qui touchent** avec chaque réponse : une, c'est
une coïncidence à vérifier ; quinze, c'est une identification.

## Les noms

```text
0x821BA780  D3D::CDevice::KickOff()                                8/16
0x821B9BC8  D3D::CDevice::AddCallsToPrimaryBuffer(SegmentCall*,DWORD)  3/16
0x821C57D0  D3DDevice_Swap                                         1/16
0x821BB078  D3D::CBlocker::Check()                                15/24
0x821B9DB0  D3D::CDevice::CreateInvalidateBuffer(DWORD*,DWORD*)    6/24
0x821B9AE0  D3D::CDevice::BeginRingAlloc(DWORD,DWORD)              5/24
0x821ADAB8  D3D::CounterHandler(DWORD,DWORD)                      14/24
0x821ADC78  D3D::InitXBDMInterface(CDevice*)                       3/24
```

## Ce que deux de ces noms règlent

`0x821ADC78` est **`InitXBDMInterface`**. XBDM est le moniteur de débogage
Xbox. Le commit `12c5a372` avait établi, en lisant les gardes, que ses `bctrl`
passaient par `KeDebugMonitorData` et `KeCertMonitorData` et que la chaîne
« service 47, catégorie 2 » était une inscription auprès du moniteur de
débogage, absent sur console de série. Microsoft le confirme par le nom.

Et `0x821ADAB8`, le fameux « callback » que la campagne poursuivait, est
**`D3D::CounterHandler`** — un gestionnaire de **compteurs de performance**.
`device+0x5460`, qu'il écrit, est donc un champ de compteur, pas une porte de
rendu. La frontière poursuivie pendant de nombreux cycles est close par son
nom.

## Ce qui n'est pas du XDK

```text
0x821AD378  0x821AD7C0  0x821ACCD0  0x821BE9A0  0x821C64E8  0x821C5190
```

Aucune ne correspond, sur six archives `d3d9*` et `xgraphics`. Ce sont donc du
code du jeu, ou une variante compilée autrement. `0x821AD378` — le `switch` sur
le mode d'affichage qui garde `[device+21508]`, mesuré dans `5da91f72` — en
fait partie : c'est du code Namco, pas du D3D.

## Deux bogues de l'outil, trouvés parce qu'il mentait en silence

- L'en-tête COFF a `NumberOfSections` en `+2`, `PointerToSymbolTable` en `+8`.
  Les lire d'un seul `<HII` à l'offset 2 tombe sur `TimeDateStamp`. La première
  version répondait « aucune correspondance » pour des octets qui
  correspondaient manifestement.
- Une fonction COMDAT partage sa valeur avec le symbole de section. Le premier
  classement rendait `.text`.

Les deux ont été trouvés en comparant l'outil à une recherche d'octets faite à
la main, qui, elle, trouvait bien la correspondance.

## Non établi

- Le nom de `sub_821AD378` et des cinq autres.
- La disposition de la queue privée de `D3DDevice`, toujours hors de portée
  (`b19fa6ef`).
- Que `d3d9.lib` 6132.6 soit exactement la bibliothèque liée : les taux de
  correspondance vont de 1/16 à 15/24, ce qui est cohérent avec une version
  proche, pas identique. `D3DDevice_Swap` à 1/16 est le cas le plus faible et
  ne serait pas publié seul.
