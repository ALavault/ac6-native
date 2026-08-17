# Cycle 1667 — garde d’absence d’écriture frontbuffer PAL

## Verdict

La plage de présentation observée dans l’IB PAL est bien committée dans la
mémoire guest et reste entièrement nulle après 253 ticks, mais aucun store
guest ni aucune écriture host issue de `XenosBatchResult.memory_writes` n’a été
observé sur cette plage pendant neutral ou START. Le résultat ne qualifie donc
pas encore des pixels guest-owned : le renderer Vulkan actuel conserve sa
destination dans un `VkBuffer` host et ne la réinjecte pas dans `GuestMemory`.

Le diagnostic est désactivé par défaut et ne change aucun effet du renderer.

## Identité et périmètre

- cible : `ac6-demo-xbox360-pal`, `Default.xex` ; SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
  Xenon big-endian/Xenos ;
- store utilisé : `.build/ac6-demo-store-test-3`, marqueur et XEX concordants ;
- destination observée : `[0x1374A000, 0x13AE2000)`, soit `0x398000` octets ;
- aucune preuve retail, Xenia/ReXGlue, Ghidra, microcode ou actif propriétaire
  n’a été ajouté.

## Instrumentation durable

- `src/guest_bridge/frontbuffer_writer_trace.hpp` journalise les cinq formes
  de stores PPC chevauchant la plage, avec adresse, largeur, tick, thread, LR,
  fonction générée et ligne ;
- `src/guest_memory.cpp` journalise les écritures host `store_bytes` qui la
  chevauchent, afin de couvrir les `memory_writes` du chemin PM4 ;
- les deux chemins sont activés seulement par
  `AC6_DEMO_WATCH_FRONTBUFFER_WRITERS=1` et n’écrivent aucun artefact projet.

## Observations reproductibles

Le probe de mémoire guest a lu `0x1374A000..0x13AE2000` après 253 ticks :
`mapped=true`, `bytes=3768320`, `nonzero=0`. Le premier dword est également
lisible et nul. Cette lecture confirme le mapping et l’état nul, pas une
production de pixels.

| route | résultat | RTPLY SHA-256 | stores guest | écritures host |
|---|---|---|---:|---:|
| neutral, 253 ticks | `play` rc 0 | `1d41d2e26003a631f8bec19534812a258ffc5d90466be885f6f7f3df797ebef7` | 0 | 0 |
| START tick 252, 253 ticks | `probe` `max_ticks`, rc 4 attendu | `31553733582cf7345375d8f197a28645621e488700f2a31b7e883b66109360b2` | 0 | 0 |

Le rapport START est `0f66d089f0656be4c79f0cd9b101f194d27ba6cefcd91e49e60ca2dfa10d564e`.
Les stderr des deux watchers sont vides (SHA
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`).

## Validation

- CTest codegen OFF : 18/18 ;
- CTest codegen ON : 17/17 ;
- complexité et audit source : pass ;
- binaire codegen ON après instrumentation :
  `baabfdc45f3a622c145a5ec020b05ac5d99f542def3575fda4845559aa3f19a3`.

## Classification et prochain checkpoint

- `demo-qualified` : adresse/étendue de destination, mapping lisible nul à
  tick 253, absence de stores observables sur les deux routes, garde
  fail-closed activable ;
- `unknown` : contenu GPU de destination, copie GPU→guest, source RT/EDRAM
  guest-owned, pixels non noirs et screencap native ;
- `xenia-generic` : aucune nouvelle preuve utilisée dans ce cycle.

Prochain test : joindre le `VkBuffer` de destination au producteur de
`XE_SWAP`, effectuer un readback frais, puis démontrer une écriture guest
distincte et son premier consumer avant toute promotion visuelle. Ne pas
considérer l’état nul actuel comme une screencap.
