# Cycle 1592 — provenance du SDK

## Verdict

La version du SDK Xbox 360/XDK utilisée par l’équipe originale pour construire
l’ISO PAL n’est pas identifiée dans les artefacts disponibles. Le `default.xex`
est qualifié comme XEX2 PAL (Media ID `0379EFB3`), mais ni le rapport de
préparation ni les chaînes exploitables du XEX ne fournissent un numéro XDK,
une version de compilateur ou un journal de build. `v0.0.0.8` dans les
manifestes désigne la version/base XEX de l’oracle, pas le SDK de l’ISO.

L’installeur ajouté ensuite, `XDKSetup5849.17.exe`, est bien un PE InstallShield
Microsoft de version fichier/produit `1.00.5849.17`, taille 393 782 016 octets,
SHA-256 `af1d81c65b8e31c5e1e70145854611be254b4e8755a2e90deabf906489c8d7b0`.
Mais son contenu est celui du XDK Xbox original : chemins `XDK/xbox`,
`d3d8`, `xboxkrnl`, `xmv` et symboles noyau 3944/4039/5455. Ce n’est pas une
preuve du SDK Xenon/Xbox 360 et l’installeur n’a pas été exécuté.

Le nouvel artefact `XBOX360 SDK 21256.3.exe` est, lui, qualifié comme SDK
Xbox 360 : PE InstallShield, version fichier/produit `2.0.21256.3`, taille
1 526 227 088 octets, SHA-256
`efec946c7b4436d53a6c41bb6bcff8373387ec97557e92f0ef672c85eadc4bc7`.
L’archive expose notamment `XDK/bin/win32/xma2encode.exe`,
`XDK/bin/win32/Xbox360Codec.dll`, `XDK/bin/win32/cl.exe`, les symboles
`xboxkrnlc/xboxkrnld/xboxkrnlt`, ainsi que `XDK/doc/*/xbox360sdk.chm`.
Il s’agit donc de la meilleure candidate locale au dernier SDK Xbox 360
public, mais cela ne relie pas encore cette révision au build d’AC6 : l’ISO,
le XEX et les rapports de build ne portent toujours pas d’identifiant
`21256.3`.

## Ce qui est identifié

Le SDK versionné utilisé comme référence d’outillage de l’oracle
`AC6_recomp` est :

* dépôt déclaré : `https://github.com/rapidsamphire/rexglue-sdk.git` ;
* branche déclarée : `ac6recomp-fixes` ;
* dernier commit vendeur :
  `06d1f5785153cd57c0e6b289f587adca67859714` ;
* arbre Git vendored :
  `741541d6035616dc406f7d74c2fe8f155913c77b`.

Le manifeste précise que le commit upstream n’a pas été conservé dans cette
intégration ; le couple commit vendeur/arbre est donc la qualification
reproductible. Le pin upstream `rexglue-sdk` `0.9.0` /
`cb58065c793429aa92895d778af58d12e9d26d8f` du manifeste de confiance reste
une référence d’outillage, jamais une preuve du SDK Microsoft de l’ISO.

## Limite et suite

`extract-xiso`, Ghidra, XEXLoaderWV, CMake, Clang, Vulkan et le XDK 5849
classique sont des outils de préparation ou de reconstruction, pas le SDK
Xenon de production démontré pour ce jeu. Le SDK `21256.3` est désormais une
référence candidate qualifiée, pas une preuve de provenance de l’ISO.
Pour fermer cette question, il faut une preuve bornée issue du XEX/certificat,
d’un PDB/état de build ou d’un artefact développeur portant le numéro XDK.
En l’absence de cette preuve, le SDK original reste `unknown`, sans
inférence à partir d’un nom de bibliothèque ou d’une date.
