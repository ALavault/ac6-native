# Cycle 1666 — matérialisation EDRAM bornée depuis le draw PAL exact

## Changement

Le resolve Vulkan ne remplit plus l’EDRAM atteint par un `memset` noir
inconditionnel. Il exige le readback exact du draw normal PAL (640×360,
RGBA8, SHA `0b150fd3…ec58366`, tous les octets nuls), puis matérialise ses
quatre samples par pixel dans la disposition Xenos 80×16 samples, pitch 16
tiles. Toute taille, dimension, hash ou valeur divergente trappe avant le
dispatch.

La surface matérialisée couvre exactement `0x384000` octets (`16 × 45 × 5120`).
Le reste du buffer EDRAM est rempli avec le canari `0x5A`; le resolve doit donc
rester dans la plage source qualifiée. Le destination buffer conserve ses
canaris avant/après `0x398000`.

Cette projection est une preuve de chaîne native draw→source resolve pour le
frame noir atteint, pas encore une preuve d’un dump EDRAM guest-owned. Aucun
pixel non nul n’est accepté sans qualification séparée de packing/endian.

## Validation

Builds démo OFF/ON et CTest : `18/18` et `17/17`.

Deux probes codegen ON/Vulkan, stores neufs, neutral et START (`0x0010` au tick
252), atteignent 600 ticks (`max_ticks`, code 4) :

- 463 notifications PRESENT par route ;
- IB intermédiaire `ef7ab6e4…d2b0` et IB principal `d121c8d8…358d6` identiques ;
- 5 shader loads, 26 draws, 1 present, 1 normal draw, 1 neutral resolve ;
- normal readback `0b150fd3…ec58366` et resolve `0c660f2b…a4913a5f` identiques ;
- aucune milestone frontend, mission ou terminal.

Le RTPLY/movie neutral du cycle 1665 a aussi été rejoué après ce changement,
depuis un store neuf : `deterministic=true`, 2 151 événements, code retour 0,
aucune divergence XAM/trace et les mêmes deux digests renderer.

Traces temporaires : `/fastdata/lavaulta/tmp/ac6-demo-vulkan-cycle1666-IiSsYi/`.
Replay de revalidation : `/fastdata/lavaulta/tmp/ac6-demo-vulkan-cycle1666-replay-TtPWPG/`.
Leurs SHA RTPLY restent neutral `6c34827c…07970c7` et START
`2a4577f8…f27cc`.

Source modifiée et gardée : `src/vulkan_neutral_resolve.cpp`, SHA
`cb4ef06d158a4624a85f9713697da5dc7227c2af9fde6831b078c806f926b803`.

## Classification et politique

- `demo-qualified` : garde draw/readback exacte, bornes de surface EDRAM et
  invariants canari, reproductibilité A/B après changement.
- `demo-observed` : renderer et PRESENT à 600 ticks.
- `unknown` : contenu EDRAM guest-owned, destination guest-owned, pixels non
  noirs, frontend, mission et résultat.

Aucun fallback visuel, screencap, actif propriétaire, microcode, C++ généré,
Ghidra, Xenia ou ReXGlue n’a été modifié ou suivi.

Capsule : `analysis/demo/ac6-demo-edram-source-materialization-v1.json`.
