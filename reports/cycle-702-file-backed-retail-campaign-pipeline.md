# Cycle 702 — pipeline campagne file-backed sur le corpus PAL

Date : 2026-08-03  
Périmètre : runtime de campagne générique, lecture bornée des PAC et
qualification physique des routes selector 1/2.

## Résultat

`CampaignRuntimeState` expose maintenant le même choix de mission pour des
`CampaignPacBankSource` file-backed que pour les spans synthétiques. Le
callback reçoit uniquement `(offset, stored_size)` de l’entrée `DATA.TBL`
sélectionnée ; il vérifie la borne du fichier, fait un `seek/read` exact et
retourne la tranche. Aucun PAC complet n’est mappé ou copié.

Le nouveau test optionnel `ac6-campaign-retail-asset-tests` lit le corpus PAL
local sous `AC6_ASSET_ROOT`, valide `DATA.TBL` contre les tailles des deux PAC,
puis exerce le pipeline commun :

```text
selector 1 → DPL 9 → DATA.TBL[9] → decode → loadout → objectifs → completion
       → selector 2 → DPL 10 → DATA.TBL[10] → decode → loadout → completion
```

Les assertions observent 926 entrées, `pack_count=2`, les indices physiques 9
et 10, les tailles décompressées déclarées, deux lectures bornées dans
`DATA00.PAC` et aucune lecture dans `DATA01.PAC`. La sortie du test est :

```text
retail_asset_pipeline_ok entries=926 mission1_entry=9 mission2_entry=10 reads=2
```

Sans `AC6_ASSET_ROOT`, le test retourne 77 et CTest le marque `Skipped`; la
CI ne dépend donc pas de données retail locales.

## Validation

```text
ac6-campaign-runtime-tests : pass
ac6-campaign-retail-asset-tests + corpus PAL : pass
ac6-campaign-retail-asset-tests sans corpus : skipped (77)
CTest complet avec AC6_ASSET_ROOT : 52/52 pass, 40.31 s
git diff --check : pass
```

Hashes des artefacts ajoutés/modifiés :

```text
include/ac6/campaign_runtime.h        6718a99f19356edb1673af0f77d2256bcacf255a2baffd8cfca1317cea8504aa
src/campaign_runtime.cpp              b292f72ae8129a032222ca6cb04b1f9c8c05be62fa2d25027e8bf1c5fa726d9b
tests/campaign_retail_asset_tests.cpp 0281d6ae1cd035babe78e0090d7a871ce4adbd1b1536d1dd384c9db90759b1ff
CMakeLists.txt                        a3debe48d3df643f22c5a1d0425db26047825e69396f7ea329db5fd25763cf23
```

## Frontière conservée

Ce cycle ne prouve pas que l’exécution retail interactive atteint Mission 1,
que son monde/HUD sont rendus, ni que Mission 2 est débloquée par une vraie
sauvegarde. Les événements `loadout/objective/completion` du test sont des
contrats natifs déterministes appliqués à des payloads PAL réels; ils ne
remplacent pas une observation retail qualifiée. Le prochain gain doit porter
sur mips/layouts shader-mesh et la présentation Vulkan AC6, sans ajouter de
branche spécifique à Mission 2.
