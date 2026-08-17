# Cycle 1601 — qualification du build de démonstration

## Identité et extraction

La démo reste un **contrôle différentiel** ; elle ne remplace pas la cible PAL
retail. L'archive utilisateur contient un paquet STFS Xbox LIVE de 324 341 760
octets, SHA-256
`141e9f25d84d1b29746271e6dfc60ca742f40531d0a48f1a47637dac54e2b117`.
Il a été extrait sans installation système avec `unrar`, puis avec
`sp00nznet/360tools` révision
`1b53928767fa134c1ea2ea42007bc39852eea1fc`. Les octets extraits restent
ignorés par Git.

Fichiers qualifiés :

- `Default.xex`, 1 454 080 octets,
  SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
- `DATA.TBL`, 861 entrées et 2 PAC,
  SHA-256 `0d9e11cf19881971e7d14c0077e9e719c1795e0316afab4b48b153351591eef8` ;
- `DATA00.PAC`, SHA-256
  `838356ade0f41fc7eee11684dda8e4d6c07eac7512a23ef1d148eb3144dbb162` ;
- `DATA01.PAC`, SHA-256
  `08ef13fe61caf0b072a4de6de577e965b4f1c8feb88d638ffc099dd4d63238d3`.

## SDK et informations de debug

`xex1tool` identifie un exécutable retail-signé daté du 27 juin 2007, PE
original `ACE6_X360.exe`, base `0x82000000`, entrée `0x821A7160`. Toutes les
bibliothèques statiques déclarées — notamment XAPILIB, D3D9, D3DX9, XGRAPHC,
XBOXKRNL, XNET, XONLINE, XHV et XAUD — portent la version **5632.0**. Ce build
est donc antérieur au retail 6132.x et constitue un bon contrôle de stabilité,
pas une approximation plus proche du SDK retail.

Le PE reconstruit ne contient **aucune table COFF de symboles** (`PointerToSymbolTable=0`,
`SymbolCount=0`). Il conserve toutefois un répertoire CodeView RSDS :

- PDB : `C:\project\ace6\work\ACE6_X360_Release_Demo_Offline\ACE6_X360.pdb` ;
- GUID : `{DE38AC08-72B3-453A-8DB7-2523BE0C3BD6}` ;
- âge : `1`.

Le PDB n'est présent ni dans la démo ni dans les SDK locaux. En revanche, 772
noms de types RTTI MSVC subsistent (`ACE6`, `galib`, etc.), dont
`CAce6MissionManager`, `CX360UnitManager` et `CAce6ArmsManager`. Ils peuvent
qualifier des classes/vtables, mais ne fournissent ni noms de fonctions, ni
lignes source, ni variables locales.

## Scénario et ObjBin

L'entrée DATA 10 est un FHM de 39 127 072 octets. Son enfant 0, borné à
3 355 392 octets et SHA-256
`de7e725ac8e77ac888d8d621d8c67f0247a5563bea59d2f44a0c578b841fce45`,
est accepté sans adaptation par le lecteur natif retail :

- 10 slots racine, présence `1111010111` ;
- 213 unités, 414 enregistrements ObjBin, 4 factions, 3 sous-missions ;
- 229 ordres de compteur, identifiants bornés par la capacité 327 ;
- 629 exécutions de lecteurs SetBin/ObjBin/SubMisTblBin/RadioTblBin ;
- zéro échec de lecteur et 213 unités construites.

La fonction démo `0x822D8928–0x822D8CF8` reproduit la disposition du lecteur
ObjBin retail `0x82330158–0x82330540` : données en `+0x00`, Param en `+0x04`,
manœuvres en `+0x08`, Durable en `+0x0c`, trois Weapon en
`+0x10/+0x14/+0x18`, et enfant final en `+0x1c`. Cette concordance entre SDK
5632 et 6132 prouve la stabilité de la **structure**. Elle ne prouve toujours
pas la sémantique interne des charges DurableBin/WeaponBin.

## Effet sur le checkpoint

Preuve `provisional-covered` uniquement. Aucune des six lanes n'est fermée.
Le prochain seuil est une chaîne complète lecteur → consommateur, croisée sur
les deux XEX, avant d'exposer le moindre champ d'arme ou de durabilité au chemin
produit.

