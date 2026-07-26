# AC6 — réinitialisation opaque `FUN_822993A0`

Date : 2026-07-16

## Identité et preuve

- XEX PAL : `default.xex`, Xbox 360 PowerPC big-endian ; adresse `0x822993A0`.
- Export Ghidra qualifié :
  `workspaces/ace-combat-6/export-catalog/functions/8229/0x822993a0__FUN_822993a0.json`.
- Corps : deux stores de mot `0xffffffff` à `param_1 + 0x164`, puis
  `param_1 + 0x160`, sans appel ni donnée globale.
- Appelants exportés : `Function_822A3238` et `Function_822FD278`. Ils
  exécutent ensuite un appel virtuel conditionnel, dont le type d'objet et la
  cible restent inconnus.

## Transposition

`reset_function_822993a0_state` reproduit les deux stores sur
`Function822993a0State`, une vue de mots/octet conservant les offsets retail.
Les mots sont `uint32_t` afin de représenter exactement le motif PPC
`0xffffffff` sans transformer ce sentinel en une convention métier native.

La même vue comprend maintenant les quatre wrappers exportés :

- `Function_822FD278` : reset, puis callback virtuel `1` seulement si
  `+0x214 != 0xffffffff` ;
- `Function_822A3238` : reset, puis callback virtuel `1` seulement si
  `+0x220 != 0xffffffff` ;
- `Function_8228EC68` : wrapper `822FD278`, puis callback `2` si
  `+0x324 != 0xffffffff` ;
- `Function_82226B10` : wrapper `822FD278`, store `0xffffffff` à `+0x27b4`,
  callbacks `2` puis `3`, puis octet nul à `+0x27ef`.

Le receiver virtuel reste une callback opaque fournie par l'appelant. Cela
préserve les appels sans supposer de vtable, de type d'objet, ni de rôle de
mission.

## Limite

Cette fonction n'est reliée à aucun objet de scène, mission, avion, ni au
dispatch virtuel de ses appelants. Sa validation est structurelle et de build
uniquement ; une trace Xenia qualifiée est nécessaire avant tout raccordement
runtime.
