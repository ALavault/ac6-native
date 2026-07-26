# AC6 cycle 252 — résolution des binds stream et index

## Identité et méthode

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- analyse : Ghidra headless, projet canonique, lecture seule et sans analyse ;
- aucune session Xenia, GUI, VNC ou intervention humaine.

Deux audits statiques indépendants ont été croisés. Ils concordent sur les
chemins PPC, mais l'un conservait l'ancienne étiquette vertex declaration pour
`0x821DD188`. Le consommateur du champ tranche cette contradiction en faveur
de l'index-buffer.

## Stream source : `0x821DD068`

L'ABI d'entrée est :

```text
r3 = device
r4 = slot
r5 = buffer
r6 = offset en octets
r7 = stride en octets
r8 = masque dirty
```

Le bind publie le buffer à `device+0x30A4+4*slot`. Les appels non nuls de
`0x82138164..0x82138198` lient deux buffers contigus avec slots `0/1`, offsets
nuls et strides `0x34/0x14`. Les chemins de reset passent eux aussi par
l'entrée `0x821DD068`.

Le chunk configuré `0x821DD0A8` n'est pas universel : le test de buffer à
`0x821DD084` branche à `0x821DD0D8` lorsque celui-ci est nul. Ce chunk est donc
contourné par un reset et ne peut pas maintenir un shadow state exact. Il est
maintenant transparent. La frontière recommandée est l'entrée `0x821DD068`,
qui conserve aussi offset et stride ; le store universel `0x821DD154` a déjà
perdu l'offset.

## Index buffer : `0x821DD188`

La fonction reçoit `device` dans `r3` et le nouvel index-buffer dans `r4`, les
conserve dans `r31/r29`, charge l'ancien buffer à `device+0x308C`, puis publie
toujours le nouveau à `0x821DD20C`.

La sémantique est confirmée par le draw indexé partagé `0x821DF2C0` : il charge
`device+0x308C` à `0x821DF500`, puis lit `buffer+0x00` à `0x821DF574` et
`buffer+0x18` à `0x821DF588` pour construire son paquet. Le caller
`0x821381A4` lie `r30+0x48` juste après les deux vertex streams `r30` et
`r30+0x24`; ces trois ressources ont un stride objet de `0x24`. Des resets
explicites passent `r4=0` à `0x821E6E60` et `0x8233E508`.

Le chunk configuré `0x821DD1C8` n'est pas universel : les branches
`0x821DD1A4` et `0x821DD1B8` peuvent aller directement au store. Il reste
transparent. Le point de commit recommandé est `0x821DD20C`, avec
`r31=device`, `r29=new buffer`, `r30=old buffer`.

Confiance : `confirmed` pour ABI, champ, chemins et sémantique index-buffer ;
`cross-match` pour le nom XDK probable `SetIndices`.

## Correction de l'ancienne conclusion vertex declaration

Le cycle 250 et le script au nom historique
`VerifyVertexDeclarationContracts.java` appelaient à tort `0x821DD188` un bind
vertex declaration. Le script conserve son nom de fichier pour la compatibilité
des commandes, mais vérifie désormais explicitement le contrat index-buffer et
son consommateur. Le vrai bind vertex declaration reste `unknown`.

`0x821DE7D0` reste seulement un chunk du helper de création `0x821DE7A8`; il
ne doit pas alimenter le shadow state d'un device.

## Intégration et limites

La modification minimale de configuration serait d'ajouter :

```toml
0x821DD068 = { name = "rex_sub_821DD068" }
0x821DD20C = { name = "rex_sub_821DD20C" }
```

Elle n'a pas été appliquée : la régénération ReXGlue/XenonRecomp n'est pas
actuellement exécutable dans le checkout sans sa dépendance de configuration
GTK 3. Modifier la configuration sans pouvoir régénérer et tester violerait la
règle interdisant les changements manuels du généré. Les champs stream/index
restent donc à zéro plutôt que de publier un état partiel.

## Validation

- contrat stream/RT/depth : **28/28** assertions PPC exactes ;
- contrat index-buffer étendu : **29/29** assertions PPC exactes ;
- sources de hooks : Clang 21 C++23, `-fsyntax-only -Wall -Wextra -Werror` ;
- artefacts :
  - `artifacts/ac6-cycle252-stream-index-raw-validation.log` ;
  - `artifacts/ac6-cycle252-stream-index-validation.log` ;
  - `artifacts/ac6-cycle252-hook-sources-final-syntax.log`.

## Archive annoncée

La nouvelle archive AC6 annoncée n'était pas encore visible lors de cette
analyse. Aucun contenu d'une archive antérieure n'a été présenté comme nouveau.

## Prochaine frontière autonome

Retrouver le vrai store/bind de vertex declaration, puis rendre la régénération
codegen testable avant d'ajouter les deux frontières stream/index. Aucun run
humain n'est requis pour cette frontière statique.
