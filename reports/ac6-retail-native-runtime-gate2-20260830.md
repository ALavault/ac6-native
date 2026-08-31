# AC6 retail NTSC-U/J — Gate 2 runtime natif préflight

Date: 2026-08-30.

## Preuve acquise

`NativeRuntime` relie désormais les frontières natives dans un lifecycle
déterministe:

- entrée ISO ou `assets/` existante;
- boot explicite et stockage mutable sous racine utilisateur;
- replay poll-exact;
- soumission ring vers `VdBridge`, décodage PM4 et sink Vulkan typé;
- save/load atomique et teardown explicite.

Le test ne fabrique aucun objectif, compteur, frame gameplay ou état terminal.
Il fournit seulement une PresentPacket de fixture pour vérifier la couture des
composants.

## Validation

```text
cmake --build ... --target ac6_native_xenos_tests ac6_native_services_tests
  ac6_native_ppc_abi_tests ac6_native_frontend_tests ac6_native_runtime_tests
ctest ... --output-on-failure
5/5 passed
pytest -q recompilation/ace-combat-6-retail/tests
111 passed
tools/validate.py --target ntsc-uj --runtime native
pass; release_ready=false
```

Tous les exécutables natifs passent l'audit `nm` sans ReXGlue/Xenia/
XenonRecomp/XenosRecomp/D3D12.

## Frontière Gate 2 encore ouverte

Le runtime n'exécute pas encore le XEX US qualifié: génération XenonRecomp
directe, contexte PPC complet branché au dispatcher, 229 imports offline et
branche Vd réelle restent à intégrer. Aucun boot retail, menu, cinématique ou
M01 n'est donc qualifié. Prochaine action autorisée: préparer cette génération
dans `build/ntsc-uj/native` ignoré, puis connecter uniquement les imports
qualifiés; ne pas lancer campagne/PAL/save release avant une arrivée M01
observable.
