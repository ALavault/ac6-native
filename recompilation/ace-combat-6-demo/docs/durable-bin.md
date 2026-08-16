# `DurableBin` — preuve et modèle prudent

## Identités

La cible du produit reste le `Default.xex` démo, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.

La preuve fournie pour cette note porte sur le XEX PAL SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
Les deux identités ne sont pas fusionnées : les adresses ci-dessous sont une
preuve de layout à réconcilier dans le binaire démo avant de déclarer le
consommateur du payload atteint et qualifié.

## Layout établi

Le lecteur principal est `ObjBin::getReadBuffSize` à `0x82333578` et
`ObjBin::read` à `0x82333758`. Le message
`Error / DurableBin::read() / data empty!\n` est à `0x8201009C`.

Dans le switch de `ObjBin::read`, le child type 2 est `DurableBin` et le type
3 est `WeaponBin`. La branche `DurableBin` (`0x823338DC..0x82333954`) :

1. récupère le descripteur du child type 2 ;
2. réserve `0x10` octets dans l’arène de lecture ;
3. écrit l’adresse du wrapper dans `ObjBin+0x0C` ;
4. résout le pointeur relatif du payload ;
5. écrit ce pointeur à `DurableBin+0x00` ;
6. journalise l’erreur si le pointeur résolu est nul.

Le modèle ABI utilisé par le runtime est donc `DurableBinAbi` dans
`include/ac6demo/mission_bins.hpp` : une adresse invitée 32 bits au premier
mot et douze octets réservés. `ObjBinAbi` fait `0x20` octets, avec `durable` à
`+0x0C` et `weapon` à `+0x10`. Les champs sont des adresses invitées, jamais
des pointeurs hôte : cela conserve l’ABI Xbox sur Linux x86-64.

Le payload est non propriétaire et sa longueur reste inconnue. Le buffer de
mission décodé doit donc rester vivant tant que `DurableBinView::data` est
consulté.

## Ce qui reste volontairement opaque

Le parseur ne lit aucun champ du payload. Il ne justifie donc ni HP, ni
armure, ni multiplicateur, ni format AC5 à neuf octets. La première preuve
utile dans le `Default.xex` démo est le premier chargement indirect depuis
`*(ObjBin+0x0C)+0`, puis l’inventaire des offsets et types effectivement lus.
Jusqu’à cette preuve, les hooks métier ne doivent pas interpréter ni
remplacer `DurableBin`.

## Contrôle de frontière sur le `Default.xex` démo

Une analyse Ghidra fraîche du `Default.xex` démo, dans le projet
`ace-combat-6-demo`, confirme seulement la frontière code/donnée voisine :
`0x8233372C..0x82333734` est non décodé après l’appel à `0x82331808`, puis le
code reprend à `0x82333738`. La plage exportée est `0x8233372C`, taille
`0x0C`, SHA-256 des octets
`dd75c2ba4664b8a715397e12595977a3cd32cece9568b9b49e915fb0fcca1667`.

Cette observation ferme une frontière de codegen, pas le schéma du payload :
elle ne transforme pas les adresses de la preuve PAL externe en preuve démo et
ne constitue pas encore le premier consommateur de `DurableBin::payload`.
