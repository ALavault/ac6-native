# Cycle 1632 — shaders image PAL joints au runtime neutral

## Résultat

Le runtime ReXGlue ne se contente plus d'une identité PM4 : chaque VS neutral
qualifié porte désormais sa provenance image PAL exacte. La traduction refuse
un VS atteint dont le triplet stage/taille/hash n'a pas une plage qualifiée.

| VS | Plage image | Taille | SPIR-V ReXGlue |
|---|---|---:|---|
| `099625f3…e4e3` | `0x82013E20..0x82013E7F` | 96 | 7 800 bytes, `944fd752…ce6` |
| `93488cb9…402b` | `0x820140A0..0x8201410B` | 108 | 12 496 bytes, `ba9b97cc…576` |
| `586168ec…3cc0` | `0x82014140..0x8201417B` | 60 | 9 288 bytes, `4913cadb…920` |

`tools/validate_qualified_vertex_sources.py` vérifie le SHA du basefile PAL,
les trois ranges et hashes, appelle le probe runtime, contrôle taille/hash des
sorties puis exige le `spirv-val` épinglé avec Vulkan 1.1 et
`--scalar-block-layout`. Sources et SPIR-V restent dans un répertoire temporaire
sous `TMPDIR`, supprimé à la fin.

## Qualification

- `demo-qualified` : plages image, tailles, hashes, copies PM4 et trois sorties
  SPIR-V ;
- `xenia-generic` : règles du translator ReXGlue et ABI scalar-block ;
- `unknown` : container compressé amont ; aucune identité n'est inventée ;
- aucun changement Xenia/ReXGlue, Ghidra, C++ généré ou microcode ; aucun actif
  propriétaire suivi.

Validation : probe source 3/3, `spirv-val` externe 3/3, Python 32/32, build,
CTest 18/18 sous Xvfb/audio dummy, audits source et complexité : PASS.

## Prochain checkpoint

Le chemin renderer neutral est joint jusqu'au draw/resolve noir qualifié. Le
prochain frontier causal redevient le premier état new-press START à
`0x829D1550` : watchpoint exact, premier writer/consumer et persistance, avant
toute progression frontend.
