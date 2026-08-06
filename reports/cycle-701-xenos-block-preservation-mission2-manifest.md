# Cycle 701 — blocs Xenos préservés et manifeste Mission 2 enrichi

Date : 2026-08-03  
Périmètre : décodeur NTXR portable et provenance de la ressource PAL.

## Résultat

Le décodeur `decode_ac6_ntxr_base_texture` conserve désormais, en plus du
RGBA8 de compatibilité, les blocs BC1/BC3 linéarisés après correction de
l'endian 8-in-16 et du parcours Xenos tiled 2D. Le bloc logique
`(by * blocks_x + bx)` est conservé dans `NtxrDecodedTexture::compressed_blocks`.
La fixture NTXR BC3 128×128 vérifie à la fois le pixel RGBA8 rouge et les
octets natifs du premier bloc (`ff 00 ... 00 f8`). Le backend du cycle 700 peut
donc consommer directement cette représentation sans réinventer le swizzle.

Le manifeste PAL source-only a aussi été enrichi pour la route statique
selector 2 : DPL 10 → DATA.TBL[10], bornée à `DATA00.PAC`, avec structure FHM /
MDLP et comptes NTXR/NDXR/MATE. `interactive_runtime_qualification` reste
explicitement `false` et la sauvegarde remplie manque toujours.

## Validation

```text
ac6-ntxr-tests : pass
ac6-vulkan-backend-tests : pass
CTest complet : 51/51 pass, 39.50 s
git diff --check : pass
```

Manifest courant :

```text
reports/ac6-pal-campaign-manifest.json
SHA-256 ca25ba8b15324163ceab4495aa924cc427e3cf3b1317cd88ead3d6a1fcd5cf8f
```

Hashes source :

```text
include/ac6/ntxr.h       34b6c014f383edbbbfe60ae4c608dcfe9f1c5c7e327f74fccd2c4b490257f2f1
src/ntxr.cpp             f972b65c74823c5b6cac682563e26240400a46678b7251dae8e83bc5893bc8bb
tests/ntxr_tests.cpp     4f9d5df2b0b76729271c18e58ed15e76e01422d6b1d515e24f3862592c115f08
```

## Frontière suivante

Il faut maintenant relier les blocs préservés aux `MATE/NDXR` et au contrat de
shader qualifié, puis vérifier les mips et l'image réellement sélectionnée sur
les chemins hangar/cutscene/gameplay. Cette étape ne doit pas être remplacée
par une exception D5B4. La qualification interactive de Mission 2 dépend
toujours d'un profil de sauvegarde retail non vide.
