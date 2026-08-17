# Cycle 1631 — provenance `rr` des trois vertex shaders neutral

## Résultat

Le gate `rr` neutral/headless tick 253 est préservé. Les trois vertex shaders
atteints ne sont pas rattachés à un container : ils proviennent de trois
plages exactes de l'image PAL décompressée, initialisées par
`GuestMemory::map_bytes` depuis `DemoSession::start`, puis copiées par le guest
vers les IB PM4.

| Rôle | Source image | Destination PM4 | SHA-256 | Writer guest / caller / LR |
|---|---|---|---|---|
| bootstrap | `0x82013E20..0x82013E7F` | `0x16ADF014..0x16ADF073` | `099625f3…e4e3` | `0x82327DEC` / `0x821B1D58` / `0x821B1DB8` |
| rectangle normal | `0x820140A0..0x8201410B` | `0x1274A254..0x1274A2BF` | `93488cb9…402b` | `0x82327DEC` / `0x821B6078` / `0x821B63FC` |
| rectangle resolve | `0x82014140..0x8201417B` | `0x1274A540..0x1274A57B` | `586168ec…3cc0` | `0x82327E38` / `0x821B6FD0` / `0x821B7830` |

Les deux premiers writers exécutent `stw r6,0(r3)`, bytes PAL
`90 C3 00 00`; le troisième exécute `stdu r7,8(r3)`, bytes
`F8 E3 00 09`. Les trois copies sont observées sur le thread guest 1 au tick
0. Les hashes des plages source du basefile PAL qualifié
`b98a9ac1…4218` sont exactement ceux des microcodes PM4.

Le début/fin et la publication du main IB restent ceux du reçu cycle 1622 :
`0x821B0D70`, `0x821BA01C`, `0x821B9D24`, IB
`d121c8d8…358d6`. La slice causale reste
`draw239 → COPY326 → VS333 → SQ351 → fetch376 → draw387 → EVENT389 →
coher393–398 → reset404/406 → present-fetch408 → XE_SWAP415`.

## Qualification et limites

- `demo-qualified` : source image exacte, hash, destination PM4, writer, PC,
  bytes, caller, LR, thread et tick des trois copies ;
- `demo-qualified` : l'initialisation source est un `map_bytes` hôte avant
  exécution guest, donc elle n'a légitimement ni PC/LR guest ni tick ;
- `unknown` : format/container amont du XEX compressé ; aucun container n'est
  inventé ;
- aucune preuve retail, aucun `rr` système, aucune mutation Xenia/ReXGlue,
  Ghidra, C++ généré ou microcode ; aucun actif propriétaire suivi.

Reçu : `analysis/demo/ac6-demo-vertex-shader-rr-provenance-v1.json`.

Validation : hashes source basefile 3/3, garde Python 32/32, build incrémental,
CTest 18/18 sous Xvfb avec audio dummy, audits source et complexité : PASS.

## Prochain checkpoint

Consommer au runtime les plages image qualifiées par adresse/taille/hash,
traduire sous `TMPDIR`, puis exiger `spirv-val` avant de joindre le draw normal
et le resolve. Neutral reste prioritaire ; START et Vulkan requièrent leurs A/B
distincts.
