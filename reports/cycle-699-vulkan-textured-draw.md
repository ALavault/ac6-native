# Cycle 699 — draw Vulkan texturé avec descripteur

Date : 2026-08-03  
Périmètre : backend Vulkan AC6-owned, fixture synthétique, sans oracle.

## Résultat

Le pipeline générique de cycle 698 accepte maintenant un handle de texture
uploadée. Il crée un layout avec le set `0`, binding `0`, réutilise le
descripteur combiné image/sampler produit par `bind_texture_descriptor`,
déclare les attributs position/UV, lie le set et émet un triangle texturé.
Le smoke upload une texture RGBA8 4×4 uniforme `0x7f` et relit le pixel
central : `[127,127,127,127]`.

Le contrat permet donc de mesurer la chaîne réelle
`upload → image layout shader-read → descriptor → fragment texture() → draw →
readback`, sans ajouter de branche D5B4 ou d'exception Mission 1. Le format
portable reste RGBA8 : les vues BC1/BC3 et leurs mips Xenos doivent encore être
branchées après qualification des payloads natifs.

## API et invariants

```text
create_triangle_pipeline(..., optional texture_handle)
draw_textured_triangle(...)
```

Un pipeline texturé refuse un draw non texturé (et inversement), vérifie la
render pass, et exige une texture uploadée avec descriptor set valide. Le
readback refuse aussi une cible jamais rendue, ce qui ferme l'appel invalide
avant une transition Vulkan depuis `UNDEFINED`.

## Validation

```text
CTest : 51/51 passed
Total Test time : 40.06 s
ac6-vulkan-backend-tests : pass
git diff --check : pass
```

Hashes :

```text
vulkan_backend.h                    2448c92f4c80e1ab5e49c8b4a70113d3252249f5c59d8413497aeb079071d1c1
vulkan_backend.cpp                  597957b4ada493c194008954f9b634ae09946f06290babcf4bfec680827bab9b
vulkan_backend_tests.cpp            f346e4e78e6e71e29ecda61ec5466abb851b9928c4c3c19bea2dc3ca8a2af780
vulkan_textured_triangle.vert       87beaeaee2e424dfe22ed59034c4a17eee3a9a16e26769819f7c06185a03dbec
vulkan_textured_triangle.frag       20e1f4d10413b3c7071d36f7648ea8a7ac45fe45291e684870a79ff482f4f01f
vulkan_textured_triangle_spirv.h    3cb4e4bfd6df7156f00cae5e0e949a231b333f2ab63ba768cbd65ce6e07485fc
```

## Limites

Le résultat n'est pas encore un rendu AC6 : shader Xenos traduit, vertex/index
layouts issus du MATE/NDXR, BC1/BC3 natif, mips, profondeur, swapchain et
présentation restent ouverts. Selector 2 → DPL 10 → DATA.TBL[10] demeure une
route statique et la sauvegarde retail disponible est vide.
