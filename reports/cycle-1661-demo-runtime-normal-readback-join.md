# Cycle 1661 — jonction runtime du draw normal au resolve neutre PAL

## Résultat

Le build codegen ON a été reconstruit avec le `Default.xex` PAL exact
(`de917873…5da8`), puis exécuté sous Vulkan depuis un store démo neuf, entrée
neutre, borné à 253 ticks. Le probe termine volontairement sur `max_ticks`
(`exit_status=4`) parce que le frontend n’est pas encore qualifié; ce n’est
pas un échec du renderer.

La frontière Vulkan a exécuté `5` shader loads, `26` draws, `1` present, `1`
normal draw et `1` neutral resolve. Le draw normal traduit et lit réellement
un buffer RGBA8 `640×360` de `921600` octets, SHA-256
`0b150fd3…ec58366`, dont chaque octet est nul. Cette observation est
maintenant contrôlée avant d’autoriser le resolve neutre; le resolve ne peut
plus partir d’un zéro synthétique sans preuve du draw précédent.

Le resolve atteint ensuite la destination `0x1374A000`, 1280×720, pitch 1280,
format brut 6, endian 0, tiled, swap 1, avec le SPIR-V ReXGlue épinglé et le
dispatch `20×90×1`. Le readback linéaire obtenu est
`0c660f2b…a4913a5f`, avec gardes et untile validés par le test Vulkan.

## Qualification et limite

Cette jonction est `demo-qualified` pour l’identité PAL, les IB, le draw
traduit et la séquence runtime observée. Elle ne constitue pas encore un
readback guest-owned : le buffer EDRAM injecté dans le resolve reste la
projection neutre 1× autorisée par le draw noir observé. Il ne s’agit donc ni
d’une preuve générale de conversion EDRAM/MSAA, ni d’une screencap native.

Le contenu exact du color RT0 guest avant le copy, la conversion non noire et
la comparaison neutral/START restent à qualifier. Toute variation hors du
profil atteint doit continuer à trapper.

## Vérifications

- CTest codegen ON : `17/17`.
- CTest build démo OFF : `18/18`.
- `spirv-val --target-env vulkan1.1` : PASS sur le blob ReXGlue épinglé.
- Trace runtime temporaire : SHA-256
  `c5357c6d…b1c5794`; rapport frontier : SHA-256
  `33b6c8b3…5685a7`.
- Sources modifiées : conservation du readback normal et garde all-zero avant
  projection EDRAM; aucune mutation Xenia, Ghidra, microcode ou C++ généré.

Capsule durable : `analysis/demo/ac6-demo-runtime-normal-readback-join-v1.json`.
