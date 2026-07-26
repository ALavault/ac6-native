# Cycle 231 — hook XenonRecomp `dcbst`

Date : 2026-07-18

## Identité et frontière

- Cible : AC6 retail Xbox 360, `default.xex` PAL.
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Architecture : Xenon PowerPC big-endian, registres 64 bits et adresses invité
  32 bits.
- Sites retail : `0x821D8DD0`, `0x821D9210`, `0x821D9240` et
  `0x821D9270`.

Ce cycle étend uniquement la traduction déterministe XenonRecomp. Il ne
qualifie ni un runtime Xenos complet, ni la cohérence d'un futur renderer, ni
la jouabilité du XEX recompilé.

## Preuve et décision de traduction

Le décodeur amont identifiait déjà `PPC_INST_DCBST`, mais le générateur le
signalait comme non reconnu. Les quatre occurrences AC6 parcourent des plages
mémoire par pas de 128 octets.

La référence locale Xenia conserve cette instruction comme une opération de
contrôle de cache `DATA_STORE` sur une ligne Xenon de 128 octets :

- `tools/xenia-source/src/xenia/cpu/ppc/ppc_emit_memory.cc` ;
- `tools/xenia-source/src/xenia/cpu/backend/x64/x64_seq_memory.cc`.

Xenia matérialise cette frontière avec deux `clflush` de 64 octets. Une
recompilation native ne peut toutefois pas imposer cette stratégie x86 à tous
les runtimes : la mémoire hôte ordinaire est cohérente, tandis qu'un renderer
ou consommateur non cohérent peut exiger une publication spécifique.

Le patch reproductible
`tools/patches/xenonrecomp/0003-support-dcbst-hook.patch` ajoute donc :

```cpp
PPC_DCBST(effective_address);
```

L'adresse respecte l'encodage indexé PPC : `RA=0` omet le premier terme,
sinon le hook reçoit `RA + RB`. `ppc_context.h` fournit un défaut portable qui
évalue seulement l'adresse ; le runtime peut surcharger `PPC_DCBST` sans
modifier les 81 unités générées. Le patch a été réappliqué depuis le commit
XenonRecomp propre après les patches `0001` et `0002`, puis comparé octet par
octet aux deux sources de travail : `PATCH_SEQUENCE_OK`.

SHA-256 du patch :
`eedf2399c90fd22237a8a8197436a4d798ea5c7752a43929d928eb468614eac4`.

## Validation

```sh
cmake --build .tools/xenonrecomp-clang-build -j16 --target XenonRecomp
.tools/xenonrecomp-clang-build/XenonRecomp/XenonRecomp \
  .tools/recomp-eval/ac6/ac6.toml \
  .tools/xenonrecomp-source/XenonUtils/ppc_context.h \
  > .tools/recomp-eval/ac6/xenonrecomp-20260718-dcbst.log 2>&1

cmake -S reconstruction/ace-combat-6 \
  -B .build/ace-combat-6-clang-probes
cmake --build .build/ace-combat-6-clang-probes -j16
ctest --test-dir .build/ace-combat-6-clang-probes \
  --output-on-failure -j16

find .tools/recomp-eval/ac6/output -maxdepth 1 -name '*.cpp' -print0 | \
  xargs -0 -n1 -P16 clang++ -std=c++20 -fsyntax-only \
    -I.tools/recomp-eval/ac6/output \
    -I.tools/xenonrecomp-source/thirdparty/simde

cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6
cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 --output-on-failure -j16
cmake --install .build/ace-combat-6 --prefix "$PWD"
```

Résultats :

- probe `dcbst` borné : **1/1 PASS** ;
- `RA=0`, `RA+RB` et surcharge du hook vérifiés ;
- quatre commentaires retail et quatre hooks correspondants ;
- aucun diagnostic `dcbst` restant ;
- instructions non reconnues : **9 -> 5** ;
- compilation syntaxique : **81/81** unités C++ générées ;
- corpus Clang incluant les probes : **46/46 PASS** ;
- corpus GNU natif : **42/42 PASS** ;
- installation racine : `ac6-current-level-catalog` et `ac6-scene-shell`,
  sans `bin/bin`.

Le journal de génération a pour SHA-256
`62ee38c7fc2b8fc5b578f66baaabda9f91a796bc917f896002116947631a2179`.

## Limites restantes

- Le défaut `PPC_DCBST` sans effet doit être requalifié lorsque le renderer
  consommera directement ces plages invitées ; le hook rend cette politique
  explicite, il ne prouve pas encore la cohérence GPU.
- Les cinq instructions non reconnues restantes sont deux `vpkswss`, un
  `mulhdu` et deux `frsqrte`.
- Les diagnostics de switch hors limites, appels indirects, services runtime
  et warnings RC `vcmpbfp.` restent hors de ce cycle.
- Aucun Xenia, Wine, VNC, GUI ou geste humain n'a été utilisé.
