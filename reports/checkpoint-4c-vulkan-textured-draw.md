# Checkpoint 4c — texture RGBA8 et mesh UV Vulkan

Date : 2026-08-12

Le transport Vulkan AC6-owned récupère maintenant un mécanisme générique de
l'ancien backend, sans son runtime campagne :

- `VulkanTexturedVertex` et des handles séparés pour mesh UV et texture ;
- upload RGBA8 via staging temporaire puis image device-local persistante ;
- sampler, descriptor set et pipeline layout avec binding combiné au fragment ;
- pipeline position/UV et `draw_textured_indexed` avec refus explicite des
  handles, layouts ou types incompatibles.

Le staging est libéré après la soumission d'upload. Aucun buffer de staging
n'est créé par dessin ; les compteurs de ressources restent stables pendant
les soumissions successives. Le fixture SPIR-V est un artefact de test borné,
pas un shader retail qualifié.

Preuve headless :

```text
vulkan_backend=pass device=NVIDIA RTX PRO 4000 Blackwell api=1.4 depth_d32=1
```

Le test crée un triangle UV et une texture opaque rouge, soumet le draw
texturé direct, vérifie le readback rouge au centre puis libère mesh, texture et
pipeline. `sampled_rgba8_unorm` est exposé dans `RenderDeviceCaps`; une option
non supportée retourne un handle nul.

Validation :

```text
ctest build : 100% (78/78 non-skipped; 2 skipped)
ctest build-core : 100% (73/73 non-skipped; 1 skipped)
python3 -m pytest -q tools/tests : 139 passed, 22 subtests passed
```

Les audits de frontière produit et de récupération Vulkan passent. Ce
checkpoint ne qualifie pas encore les formats NTXR, les shaders retail, les
transforms 3D, la profondeur, le HUD ou la parité Mission 01 ; le cache de
scène borné continue donc à refuser ces entrées explicitement.
