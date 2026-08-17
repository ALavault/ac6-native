# Cycle 1672 — A/B vectoriel du consumer frontbuffer

## Verdict

Un build diagnostique séparé, avec interception opt-in des chargements VMX
directs, a rejoué neutral et START depuis des stores neufs jusqu'au tick 253.
Les deux routes produisent les mêmes notifications `VdSwap`, les mêmes IB,
les mêmes charges shader observées, le même resolve et le même digest guest.
Les quatre helpers scalaires PPC et le hook vectoriel ne voient aucune lecture
de la plage frontbuffer `[0x1374A000,0x13AE2000)`.

Cela renforce l'absence bornée d'un consumer dans le code recompilé à cette
borne, mais ne qualifie ni le scanout hôte, ni une instruction vectorielle
non couverte par cette forme d'instrumentation après le tick 253. Le
writeback guest reste un effet renderer qualifié; il ne devient pas une
screencap. Le résultat est fail-closed.

## Cible et instrumentation

- `Default.xex`, démo PAL Xbox LIVE, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
- Xenon big-endian / Xenos; aucune preuve retail fusionnée;
- build diagnostique séparé `.build/ac6-demo-vector-build-1`,
  `AC6_DEMO_ENABLE_VECTOR_READ_TRACE=ON`, Release, ccache activé;
- build normal de produit conserve l'option vectorielle désactivée;
- le hook est dans `tools/ppc_context_adapter.h`,
  `src/guest_bridge.cpp` et le fichier diagnostique séparé
  `src/guest_bridge/vector_read_trace.cpp`, jamais dans le C++ généré: il
  intercepte la forme `simde_mm_load_si128` et appelle
  `AC6_PPC_RECORD_VECTOR_READ` uniquement pour les 16 octets qui chevauchent
  la plage exacte;
- les valeurs lues ne sont pas modifiées; l'instrumentation est désactivée
  par défaut par `AC6_DEMO_WATCH_FRONTBUFFER_READERS`.

Empreintes de la garde et du build diagnostique:

| élément | SHA-256 |
|---|---|
| `ppc_context_adapter.h` | `e743b4f410938a8b2dc3074d0d9c2ae6237a6bfb7c868fe8c763a821a226bae6` |
| `guest_bridge.cpp` | `7e276fac2f5fbe8264efcb864050751693eb68825ab7766f5366e2c0b21e0579` |
| `frontbuffer_writer_trace.hpp` | `69387c378d53fd2cbcbdb6d6ff37d605284521978d72526f7497e3afa708fdae` |
| `vector_read_trace.cpp` | `dfc0cff64162077c921bcc2418b0751ccb9cb9b001b155211ed237cd8362b556` |
| `CMakeLists.txt` | `a0a3be885e052d58226f6ecf66957185badfac08469af119e3b7ac88fba83c82` |
| binaire diagnostique | `5e423f95a2b75746d2aca55e7b1a431bdb40417f39134a950f82192093943c83` |
| binaire normal codegen | `6d59405310748f2e480745e4114331b06da5cf0c4be4a62301e5e37b0b74cda4` |

## Résultats A/B

| mesure | neutral 253 | START 253 |
|---|---:|---:|
| processus/store neuf | oui | oui |
| ticks complétés | 253 | 253 |
| notifications `VdSwap` | 116, dernière tick 252 | 116, dernière tick 252 |
| lignes `AC6_FRONTBUFFER_READ` | 0 | 0 |
| lignes `AC6_FRONTBUFFER_VECTOR_READ` | 0 | 0 |
| guest writeback | 1 | 1 |
| digest guest linéaire | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` | identique |
| `normal_readback_sha256` | `0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366` | identique |
| `neutral_resolve_sha256` | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` | identique |
| shaders chargés | 5 | 5 |
| draws/presents | 26 / 1 | 26 / 1 |
| frontend/mission/terminal | faux / faux / faux | faux / faux / faux |

Les sorties brutes de la relance post-refactor sont conservées sous `TMPDIR`
(`ac6-vector2-*-253-*`):

- neutral: trace `c5357c6d9639c2a675161bd10c3cdbe97096df0dac11d760fe8a2d964b1c5794`,
  rapport `33b6c8b3759461fd7720ca3942258cee3fcd1ec4732746a1df8314e50b5685a7`,
  stdout `e89f0b09bfaece518fef12882c0aa91d18ef9c06c323906f95c1bd901728e39a`,
  stderr vide `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`;
- START: trace `4a7326d9b1148dc6a5943cabea5c0e2562ae7ad833eda6bc7cd92a089e25724f`,
  rapport `2d0c391ba3d0d2a6f2dbf0907ac8538abde5be17f8f9424c2fb63cb2bb91a6cf`,
  stdout `390a3cc1731c925969b1d2d6f9d18340d553290c124b076f6a5702df79fab103`,
  stderr vide `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.

Les deux rapports portent les IB démo inchangés: intermédiaire
`0x127CA0C0/11`, SHA `ef7ab6e4832aed218b50126464de899ccf0f4bf2eaf26ecfac6371c51671d2b0`,
et principal `0x1274A000/3029`, SHA
`d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6`.
Les hashes shader observés restent `099625f3…e4e3`, `4913603d…98e25`,
`93488cb9…402b` et `586168ec…3cc0`.

Le build normal et le build diagnostique passent chacun CTest `17/17`; les
audits de complexité, de source, de statut et `git diff --check` sont verts.

Une relance START distincte avec le même build diagnostique couvre aussi 800
ticks: 663 `VdSwap` jusqu'au tick 799, zéro ligne scalaire/vectorielle,
frontend/mission/terminal faux, et les mêmes digests/IB. Elle étend la borne
START mais ne remplace pas un A/B vectoriel neutral à 800 ticks.

| artefact START 800 | SHA-256 |
|---|---|
| trace RTPLY | `54a2860fccb21ab3be0595ae5532a7f9e0dbc3b7b971aaf334a7a087f5a427e1` |
| rapport frontier | `062e97c4087ec46b14979ecfd0ce3cef1e5f00531e27bfd899d7e6b7386c36fe` |
| stdout renderer | `31ba1a5dff8119a093a4dda19130478e7e83e9496655d3e5ea6541feb5583ef7` |
| stderr | vide, `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |

## Qualification

- `demo-qualified`: A/B process-fresh à 253 ticks, zéro lecture scalaire ou
  vectorielle dans la plage instrumentée, writeback et digest identiques,
  IB/présentations identiques;
- `demo-observed`: 116 `VdSwap`, un PRESENT Xenos typé, 26 draws et un
  resolve Vulkan exact dans chaque route;
- `xenia-generic`: aucune nouvelle preuve;
- `unknown`: consumer après tick 253, formes VMX non réécrites par la macro
  interceptée, lecture de scanout hôte, pixels non noirs, frontend, mission
  et screencap.

## Garde et prochain checkpoint

La garde vectorielle reste hors du produit `play`: elle ne peut ni écrire le
guest ni fournir une image. Le prochain test doit joindre une lecture réelle
à un PC/LR/thread/tick, soit par une autre forme de load VMX prouvée, soit par
la frontière `VdSwap`/interrupt et un accès scanout observé. Tant que cette
preuve manque, aucun screencap n'est promu et START ne constitue pas une
transition frontend qualifiée.
