# Cycle 1659 — fetch constant PAL et garde XE_SWAP

## Résultat

Le paquet type 0 de l’IB principal à l’offset 408 (`0x00054800`) écrit six
dwords à partir du registre Xenos `0x4800`. La table ReXGlue épinglée nomme ce
registre `SHADER_CONSTANT_FETCH_00_0`; le layout générique
`xe_gpu_texture_fetch_t` décode les champs sans utiliser de preuve retail.

Les six dwords démo sont :

```text
8A000002 1374A006 0059E4FF 00001414 00000000 00000200
```

Ils sont byte-identiques au fetch fourni par la frontière guest `VdSwap` et
portent les valeurs observées suivantes : type texture, pitch 40×32 = 1280
pixels, tiled, format brut 6, endian brut 0, base `0x1374A000`, dimensions
1280×720 et dimension 2D. Le paquet précède `XE_SWAP` à l’offset 415 dans le
même IB `d121c8d8…358d6`.

## Garde implémentée

`XenosCommandProcessor` exige maintenant, avant d’accepter `XE_SWAP`, l’égalité
des six dwords et la cohérence des champs décodés avec l’adresse, le format et
les dimensions du swap. Une modification d’un seul dword trap. Cette garde ne
traduit pas les pixels et n’active aucun fallback.

## Qualification

- `demo-qualified` : bytes, hashes, registre, adresse, dimensions, format,
  endian, tiling, pitch et ordre paquet→swap ; garde négative.
- `demo-observed` : égalité avec le fetch `VdSwap` et position dans l’IB.
- `xenia-generic` : nom du registre et bitfields `xe_gpu_texture_fetch_t`.
- `unknown` : swizzle/filtrage au-delà des valeurs brutes, contenu EDRAM,
  source/destination copy réellement exécutés, pixels, frontend et mission.

## Validation

- Build démo et codegen avec ccache : PASS.
- CTest démo : 18/18 ; codegen : 17/17.
- Le test négatif modifiant le fetch trap avant présentation.
- Aucun Xenia/ReXGlue/Ghidra/C++ généré/microcode/actif propriétaire modifié ou
  suivi.

Capsule durable : `analysis/demo/ac6-demo-fetch-constant-join-v1.json`.
