# Cycle 1597 — audit du `default.xex` contre chaque SDK Xenon 6132

## Conclusion

Le `default.xex` PAL a été comparé séparément aux trois installateurs
disponibles. Le résultat est cohérent avec une chaîne de build 6132 tardive :

* `XAPILIB` est le discriminant principal et l’XEX porte explicitement
  `v2.0.6132.6` ; le corpus `6132.6` est donc le meilleur candidat local ;
* `D3D9LTCG` est identique entre les snapshots installés `.0` et `.2`, puis
  change dans `.6` ; cela concorde avec l’XEX qui porte `v2.0.6132.4`, mais
  aucun texte local ne relie explicitement le fichier de l’installateur `.6`
  au numéro interne `.4` ;
* les imports `xam.xex` et `xboxkrnl.exe` restent `v2.0.6132.0` dans l’XEX et
  dans les bibliothèques d’import des trois SDK ;
* le nom sans suffixe `XDKSetupXenon6132.exe` contient `FileVersion` et
  `ProductVersion` `2.0.6132.0`. La désignation externe « 6132.1 socle » ne
  peut donc pas être validée comme nom embarqué de cet artefact.

Conclusion opérationnelle : `6132.6` est la référence SDK la plus probable
pour AC6, avec des bibliothèques versionnées mélangées comme l’indique déjà
l’XEX (`XAPILIB .6`, `D3D9LTCG .4`, noyau/audio .0). Cela reste une
qualification de compatibilité, pas une preuve cryptographique du SDK exact
utilisé par Namco : l’XEX ne contient pas le programme d’installation ni un
manifest de build complet.

## Entrées qualifiées

| Objet | Version embarquée | SHA-256 |
|---|---|---|
| `game-files/default.xex` | XEX2, PAL, build PE `14 Nov 2007`, imports `6132.0` | `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` |
| `XDKSetupXenon6132.exe` | `2.0.6132.0` | `bce96e0a2a1284cacf8efcc4fd90cde5127e2389e8be3fa6a0981635a8cec1dd` |
| `XDKSetupXenon6132.2.exe` | `2.0.6132.2` | `bae3eb4b6340557813db60781ce530e498b466c289bd3fd760adee82556d7010` |
| `XDKSetupXenon6132.6.exe` | `2.0.6132.6` | `80dfe71356db0b79db10bffea1f443da0b5329a4f55fe756c53b2bbdd4a2491e` |

Les trois archives sont extraites, sans installation Windows, dans :

* `sdk/xdk-xenon-6132.0/` — 1 570 fichiers, 1 472 676 715 octets ;
* `sdk/xdk-xenon-6132.2/` — 1 570 fichiers, 1 472 857 452 octets ;
* `sdk/xdk-xenon-6132.6/` — 1 570 fichiers, 1 473 648 120 octets.

7-Zip signale des données après la fin physique du PE auto-extractible, mais
le CAB interne est lu jusqu’au bout et l’extraction se termine `Everything is
Ok` pour les trois installateurs. Les répertoires SDK restent hors du produit.

## Audit XEX par SDK

Le `default.xex` contient :

* `xam.xex` `v2.0.6132.0`, minimum `v2.0.5787.0`, 88 imports ;
* `xboxkrnl.exe` `v2.0.6132.0`, minimum `v2.0.5787.0`, 163 imports ;
* `XAPILIB .6`, `D3DX9 .0`, `XGRAPHC .0`, `XBOXKRNL .0`, `XNET .0`,
  `XONLINE .0`, `LIBCPMT .0`, `XHV .0`, `XMP .0`, `XAUD .0` et
  `D3D9LTCG .4`.

Pour chaque SDK, `llvm-nm` retrouve 86/88 imports XAM et 156/163 imports
noyau dans `xapilib.lib` et `xboxkrnl.lib`. Les mêmes deux symboles XAM
(`XamLoaderLaunchTitle`, `XamLoaderTerminateTitle`) et les mêmes sept
symboles noyau (`KeTls*`, `KiApcNormalRoutineNop_0`, `_vsnprintf`, `sprintf`)
ne sont pas exposés par ces archives d’import ; cette limite est identique
aux trois snapshots et ne discrimine donc aucune révision.

Les quatre imports audio observés dans l’XEX sont exposés par `xboxkrnl.lib`
dans les trois SDK : `XMACreateContext`, `XMAReleaseContext`,
`XAudioRegisterRenderDriverClient` et `XAudioSubmitRenderDriverFrame`.
Les symboles noyau qualifiés portent tous `xboxkrnl.exe@6132.0+1861.0`.

| SDK | `xapilib.lib` | `d3d9ltcg.lib` | `xboxkrnl.lib` | imports XAM/noyau |
|---|---|---|---|---|
| 6132.0 | 7 404 870 o, `eaa96e10…68852` | 89 314 312 o, `d2b7e781…c55373` | 166 774 o, `770d76ab…e32bc` | 86/88, 156/163 |
| 6132.2 | 7 409 882 o, `890d6d5b…694da4` | identique à `.0` | identique à `.0` | 86/88, 156/163 |
| 6132.6 | 7 415 706 o, `b4487e59…5d6aed` | 89 314 906 o, `f6fd4f03…9c2862` | identique à `.0` | 86/88, 156/163 |

Les hashes complets sont conservés par les commandes d’audit ; les suffixes
abrégés ci-dessus servent uniquement à lire le tableau.

## Deltas observés

La comparaison de 1 590 chemins relatifs donne 1 495 fichiers identiques
entre `.0` et `.2`, et 1 447 entre `.0` et `.6`. Les points utiles sont :

* `.0 → .2` : `xapilib.lib` change (581 membres contre 582), mais
  `d3d9ltcg.lib` reste exactement identique ;
* `.2 → .6` : `xapilib.lib` change encore, et le groupe D3D9 change :
  `d3d9.lib`, `d3d9i.lib`, `d3d9d.lib`, `d3d9ltcg.lib` et `d3d9ltcgi.lib` ;
* `xboxkrnl.lib`, `xaudio.lib`, `xgraphics.lib`, `xnet.lib` et `xonline.lib`
  sont bit-à-bit identiques dans les trois extractions ;
* `xmaencode.exe` est bit-à-bit identique dans les trois extractions, de
  version embarquée `2.0.6132.0`, SHA-256
  `3a5485fa38140df5fe9e305827ccfeb063b1e6e446808c066eb8447861b37b91`.

Cela valide la structure de delta suivante, avec une correction de
numérotation nécessaire : le premier delta `XAPILIB` est déjà visible entre
les packages `.0` et `.2`; le delta D3D9 observable dans les fichiers fournis
n’apparaît qu’entre `.2` et `.6`. Il est compatible avec un sous-numéro
`D3D9LTCG .4` inscrit par l’XEX, sans le prouver textuellement.

La conversion Markdown des trois CHM contient 8 486 pages anglaises par SDK.
Après retrait du frontmatter de provenance et normalisation de la seule
différence de casse du bandeau, le contenu des 8 482 pages techniques est
identique entre `.0`, `.2` et `.6`; seules `README.md`, `PROVENANCE.md`,
`index.md` et `toc.md` changent. `xdk_whats_new.md` documente le socle June
2007, mais pas les correctifs des point releases. La documentation confirme
donc l’ABI 6132 et ses interfaces ; elle ne fournit pas de changelog local
permettant d’attribuer textuellement les deltas `.2/.4/.6`.

## Portée sémantique

Les headers `xmadecoder.h` et `xmahardwareabstraction.h`, les imports
XMA/XAudio et les symboles noyau audio sont présents dans chaque snapshot.
Ils qualifient uniquement l’ABI et les interfaces Xbox 360/Xenon disponibles
pour l’audit ; aucune fonction applicative du XEX n’est renommée ni promue à
partir d’un symbole SDK.

## Validation

Audit reproductible par `sha256sum`, `file`, `strings`, `llvm-ar`, `llvm-nm`,
comparaison de manifests et lecture de `reports/xex1tool-inventory.txt` pour
le même XEX SHA-256. Aucun octet retail n’a été copié dans le produit natif et
aucune lane du checkpoint 2 n’est fermée par cette comparaison.
