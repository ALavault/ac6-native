# Cycle 1595 — audit `default.xex` contre le SDK Xbox 360

## Verdict

Le SDK ajouté est bien un SDK Xbox 360/Xenon tardif et constitue la meilleure
référence locale pour consulter les ABI, les outils XMA/XAudio et les
conventions PPC/AltiVec. Il ne permet toutefois pas d’identifier le SDK qui a
produit AC6. Le `default.xex` PAL est lié à une famille `2.0.6132.*`, alors que
le SDK extrait est `2.0.21256.3`.

La conclusion reproductible est donc :

* `21256.3` = dernier candidat public local qualifié, utile comme corpus de
  comparaison ;
* `6132` = génération de bibliothèques explicitement inscrite dans l’XEX,
  cohérente avec son horodatage de 2007 ;
* SDK exact du build AC6 = `unknown`, sans preuve de build, PDB ou certificat
  portant `21256.3`.

## Identités et empreintes

| Objet | Taille | SHA-256 | Preuve de version |
|---|---:|---|---|
| `game-files/default.xex` | 7 483 392 | `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` | XEX2, `NM-2001`, Media ID `0379EFB3`, PAL |
| `XBOX360 SDK 21256.3.exe` | 1 526 227 088 | `efec946c7b4436d53a6c41bb6bcff8373387ec97557e92f0ef672c85eadc4bc7` | PE File/Product `2.0.21256.3` |
| `XDK/bin/win32/xma2encode.exe` extrait | 978 432 | `941700b1e8b6333a8e80f2ce582d735264e45c1e37b17f20c6efafb069858339` | outil XMA2 du SDK |
| `XDK/TechPreview/Jul12Compiler/include/xbox/crtversion.h` | — | — | CRT 10.10, build `16045`, rbuild `17` |

L’archive est extraite localement dans `sdk/xbox360-21256.3/`; aucune
installation Windows n’a été exécutée. L’archive contient 815 entrées, dont
804 fichiers présents après extraction, pour 424 314 249 octets décompressés.

## Comparaison XEX/SDK

Le rapport local `reports/xex1tool-inventory.txt`, relu contre l’empreinte
ci-dessus, donne les éléments suivants pour l’XEX :

* import `xam.xex` version `2.0.6132.0`, minimum `2.0.5787.0`, 88 imports ;
* import `xboxkrnl.exe` version `2.0.6132.0`, minimum `2.0.5787.0`, 163
  imports ;
* static libraries `XAPILIB 2.0.6132.6`, `D3DX9 2.0.6132.0`,
  `XGRAPHC 2.0.6132.0`, `XBOXKRNL 2.0.6132.0`, `XNET 2.0.6132.0`,
  `XONLINE 2.0.6132.0`, `LIBCPMT 2.0.6132.0`, `XHV 2.0.6132.0`,
  `XMP 2.0.6132.0`, `XAUD 2.0.6132.0` et `D3D9LTCG 2.0.6132.4` ;
* date PE du basefile `ACE6_X360.exe` : 14 novembre 2007 ;
* imports audio utiles : `XMACreateContext`, `XMAReleaseContext`,
  `XAudioRegisterRenderDriverClient` et `XAudioSubmitRenderDriverFrame`.

Le SDK `21256.3` extrait contient bien `xma2encode.exe`, `xact3.exe`, les
symboles Xenon `xboxkrnlc/xboxkrnld/xboxkrnlt`, `xbox360sdk.chm` et le
compilateur MSVC 10.10. En revanche, son `XDK/relnotes.htm` renvoie les notes
de version vers le portail développeur ; `history.txt` est seulement une
entrée de dépôt Xenon du 6 décembre 2013. Aucun document local ne donne une
table de compatibilité qui rattache la série 6132 à `21256.3`.
Les symboles noyau contiennent en outre le chemin interne
`e:\xenon\xdksep13`, cohérent avec une livraison de septembre 2013 ; cela
qualifie l’artefact SDK lui-même, pas le moment de compilation d’AC6.

## Ce que cela donne comme sémantique exploitable

La présence des imports XMA/XAudio est une preuve de frontière média Xenon
dans l’XEX. Elle rend l’hypothèse DMA plausible comme mécanisme interne de
production/consommation audio, mais ne donne ni anneau, ni descripteur, ni
interruption. Le census ne contient pas `libmpeg` ni un export nommé `DMA`.
`id3` reste donc non classifié ; la piste correcte est désormais
`XMA/XAudio → éventuel DMA interne → sortie audio`, avec preuve à obtenir dans
la chaîne d’appel et les accès mémoire du projet Ghidra canonique.

Le SDK 21256.3 peut fournir des noms, structures et outils de comparaison pour
cette analyse. Il ne doit pas fournir de bornes de fonctions, de versions de
librairies ou de sémantique qui contrediraient les imports et les octets du
`default.xex` qualifié.

## Limites

Une bibliothèque versionnée dans un XEX est une borne de génération très utile,
mais une équipe pouvait conserver une bibliothèque legacy avec un compilateur
plus récent. L’audit ne prétend donc pas démontrer mathématiquement que
`21256.3` n’a jamais servi ; il démontre seulement que cette révision n’est
pas prouvée et que la signature embarquée du titre remonte à la famille 6132.

Validation : `sha256sum`, `file`, inventaire 7-Zip, lecture de
`reports/xex1tool-inventory.txt`, et contrôle CTest `ac6-retail-scene-tcam`
passé après le décodage TCAM. Aucun octet retail n’a été copié dans le produit.
